#include "OTAManager.h"
#include "BLEManager.h"
#include "ConfigManager.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "DeBug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/**
 * @file    OTAManager.c
 * @brief   OTA升级管理模块实现
 * @details 通过BLE接收固件bin文件并更新到Flash
 */


/* ========== 全局变量 ========== */
static esp_ota_handle_t s_ota_handle = 0;   // OTA句柄
static const esp_partition_t *s_update_partition = NULL;    // 升级分区
static uint32_t s_total_size = 0;   // 总字节数
static uint32_t s_received_bytes = 0;   // 已接收的字节数
static uint16_t s_expected_packet_id = 0;  // 期望的下一个包ID
static bool s_ota_started = false;  //是否接受到OTA开始
static uint32_t s_new_version = 0;  // 新固件版本号

/* ========== Flash写入缓冲区 ========== */
#define OTA_WRITE_BUFFER_SIZE  (4096)  // 4KB缓冲区，对齐Flash扇区
static uint8_t s_write_buffer[OTA_WRITE_BUFFER_SIZE];  // Flash写入缓冲区
static uint16_t s_buffer_pos = 0;  // 缓冲区当前位置

/* ========== OTA管理器初始化 ========== */

/**
 * @brief 初始化OTA管理器
 * @return ESP_OK:成功
 * @note 检查固件版本号，如果不存在则写入出厂默认版本
 */
esp_err_t OTAManager_Init(void)
{
    char version[16] = {0};

    // 尝试读取现有版本号
    esp_err_t ret = OTAManager_GetVersion(version, sizeof(version));

    if (ret == ESP_ERR_NOT_FOUND) {
        // 版本号不存在，写入出厂默认版本
        dPrint(INFO, "版本号不存在，写入出厂默认版本: %s\n", OTA_DEFAULT_VERSION);
        ret = OTAManager_SetVersion(OTA_DEFAULT_VERSION);
        if (ret == ESP_OK) {
            dPrint(INFO, "出厂版本号设置成功\n");
        } else {
            dPrint(DERROR, "出厂版本号设置失败\n");
            return ret;
        }
    } else if (ret == ESP_OK) {
        // 版本号已存在
        dPrint(INFO, "检测到现有固件版本: %s\n", version);
    } else {
        // 其他错误
        dPrint(WARN, "读取版本号失败，错误码: %d\n", ret);
        return ret;
    }

    return ESP_OK;
}

/* ========== CRC16 ========== */
static uint16_t crc16_calculate(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* ========== Flash缓冲区写入函数 ========== */

/**
 * @brief 刷新缓冲区，将缓冲区中的数据写入Flash
 * @return ESP_OK:成功, 其他:失败
 * @note 在OTA结束时必须调用，确保所有数据都写入Flash
 */
static esp_err_t flush_ota_buffer(void)
{
    if (s_buffer_pos > 0) {
        dPrint(DEBUG, "刷新缓冲区: %d bytes\n", s_buffer_pos);
        esp_err_t err = esp_ota_write(s_ota_handle, s_write_buffer, s_buffer_pos);
        if (err != ESP_OK) {
            dPrint(DERROR, "刷新缓冲区失败: %s\n", esp_err_to_name(err));
            return err;
        }
        s_buffer_pos = 0;
    }
    return ESP_OK;
}

/**
 * @brief 缓冲写入函数，先缓存到内存，凑齐4KB再写入Flash
 * @param data 要写入的数据
 * @param len 数据长度
 * @return ESP_OK:成功, 其他:失败

 */
static esp_err_t buffered_ota_write(const uint8_t *data, uint16_t len)
{
    uint16_t remaining = len;
    uint16_t offset = 0;

    while (remaining > 0) {
        // 计算本次可以复制的字节数
        uint16_t copy_len = OTA_WRITE_BUFFER_SIZE - s_buffer_pos;
        if (copy_len > remaining) {
            copy_len = remaining;
        }

        // 复制数据到缓冲区
        memcpy(&s_write_buffer[s_buffer_pos], &data[offset], copy_len);
        s_buffer_pos += copy_len;
        offset += copy_len;
        remaining -= copy_len;

        // 缓冲区满了，写入Flash
        if (s_buffer_pos >= OTA_WRITE_BUFFER_SIZE) {
            dPrint(DEBUG, "缓冲区已满，写入Flash: %d bytes\n", s_buffer_pos);
            esp_err_t err = esp_ota_write(s_ota_handle, s_write_buffer, s_buffer_pos);
            if (err != ESP_OK) {
                dPrint(DERROR, "Flash写入失败: %s\n", esp_err_to_name(err));
                return err;
            }
            s_buffer_pos = 0;  // 重置缓冲区位置
        }
    }

    return ESP_OK;
}

/* ========== 发送ACK应答（带重试） ========== */
static void send_ack(uint8_t error_code, uint16_t packet_id, uint8_t progress)
{
    uint8_t ack[10] = {
        OTA_FRAME_ACK,                         // [0] frame_type
        error_code,                            // [1] error_code
        (uint8_t)(packet_id & 0xFF),           // [2] packet_id低字节
        (uint8_t)(packet_id >> 8),             // [3] packet_id高字节
        (uint8_t)(s_received_bytes & 0xFF),          // [4-7] received_bytes (小端)
        (uint8_t)((s_received_bytes >> 8) & 0xFF),
        (uint8_t)((s_received_bytes >> 16) & 0xFF),
        (uint8_t)((s_received_bytes >> 24) & 0xFF),
        progress,                              // [8] progress
        0x00                                   // [9] 保留
    };

    // 重试机制：最多重试5次
    const int max_retry = 5;
    for (int i = 0; i < max_retry; i++) {
        esp_err_t err = BLEManager_SendData(ack, sizeof(ack));
        if (err == ESP_OK) {
            dPrint(DEBUG, "ACK已发送: error=0x%02X, packet=%d, progress=%d%%\n",
                   error_code, packet_id, progress);
            return;  // 发送成功，直接返回
        }

        // 发送失败，稍作延迟后重试
        dPrint(WARN, "ACK发送失败(尝试%d/%d)，重试中...\n", i + 1, max_retry);
        vTaskDelay(pdMS_TO_TICKS(5));  // 延迟5ms后重试
    }

    // 所有重试都失败
    dPrint(DERROR, "ACK发送失败，已重试%d次: packet=%d\n", max_retry, packet_id);
}

/* ========== 处理开始帧 ========== */
static void process_start_frame(uint8_t *data, uint16_t len)
{
    // 帧格式：[type(1)][len(2)][total_size(4)][version(4)][packet_size(2)][crc16(2)]
    // 最小长度：1 + 2 + 4 + 4 + 2 + 2 = 15
    if (len < 15) {
        dPrint(DERROR, "开始帧长度错误: %d\n", len);
        send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
        return;
    }

    // 解析字段（小端格式）
    //uint16_t data_len = data[1] | (data[2] << 8);
    s_total_size = data[3] | (data[4] << 8) | (data[5] << 16) | (data[6] << 24);
    uint32_t version = data[7] | (data[8] << 8) | (data[9] << 16) | (data[10] << 24);
    uint16_t packet_size = data[11] | (data[12] << 8);

    // CRC校验（校验整帧除了最后2字节）
    uint16_t recv_crc = data[len - 2] | (data[len - 1] << 8);
    uint16_t calc_crc = crc16_calculate(data, len - 2);
    if (recv_crc != calc_crc) {
        dPrint(DERROR, "开始帧CRC错误: recv=0x%04X, calc=0x%04X\n", recv_crc, calc_crc);
        send_ack(OTA_ERR_CRC_MISMATCH, 0, 0);
        return;
    }

    dPrint(INFO, "========== OTA开始 ==========\n");
    dPrint(INFO, "固件大小: %u bytes\n", s_total_size);
    dPrint(INFO, "固件版本: 0x%08X (v%u.%u.%u)\n", version,
           (version >> 24) & 0xFF, (version >> 16) & 0xFF, (version >> 8) & 0xFF);
    dPrint(INFO, "包大小: %u bytes\n", packet_size);

    // 获取OTA分区
    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_update_partition == NULL) {
        dPrint(DERROR, "未找到OTA分区\n");
        send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
        return;
    }

    dPrint(INFO, "OTA分区: %s, 大小: %u bytes\n",
           s_update_partition->label, s_update_partition->size);

    // 检查空间
    if (s_total_size > s_update_partition->size) {
        dPrint(DERROR, "固件过大: %u > %u\n", s_total_size, s_update_partition->size);
        send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
        return;
    }

    // 开始OTA（擦除分区）
    esp_err_t err = esp_ota_begin(s_update_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "esp_ota_begin失败: %s\n", esp_err_to_name(err));
        send_ack(OTA_ERR_FLASH_WRITE, 0, 0);
        return;
    }

    // 重置状态
    s_received_bytes = 0;
    s_expected_packet_id = 0;
    s_ota_started = true;
    s_new_version = version;  // 保存新固件版本号
    s_buffer_pos = 0;  // 初始化缓冲区位置

    dPrint(INFO, "OTA已准备就绪，等待数据...\n");

    // 发送成功应答
    send_ack(OTA_ACK_OK, 0, 0);
}

/* ========== 处理数据帧 ========== */
static void process_data_frame(uint8_t *data, uint16_t len)
{
    if (!s_ota_started) {
        dPrint(WARN, "OTA未开始，忽略数据帧\n");
        send_ack(OTA_ERR_STATE_ERROR, 0, 0);
        return;
    }

    // 帧格式：[type(1)][len(2)][packet_id(2)][data(n)][crc16(2)]
    // 最小长度：1 + 2 + 2 + 1 + 2 = 8
    if (len < 8) {
        dPrint(DERROR, "数据帧长度错误: %d\n", len);
        send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
        return;
    }

    // 解析字段
    //uint16_t data_len = data[1] | (data[2] << 8);
    uint16_t packet_id = data[3] | (data[4] << 8);

    // CRC校验
    uint16_t recv_crc = data[len - 2] | (data[len - 1] << 8);
    uint16_t calc_crc = crc16_calculate(data, len - 2);
    if (recv_crc != calc_crc) {
        dPrint(DERROR, "数据帧CRC错误: packet=%d, recv=0x%04X, calc=0x%04X\n",
               packet_id, recv_crc, calc_crc);
        send_ack(OTA_ERR_CRC_MISMATCH, packet_id, 0);
        return;
    }

    // 检查包序号
    if (packet_id != s_expected_packet_id) {
        dPrint(WARN, "包序号错误: 期望%d, 收到%d\n", s_expected_packet_id, packet_id);
        send_ack(OTA_ERR_SEQ_ERROR, packet_id, 0);
        return;
    }

    // 计算实际数据长度：总长度 - type(1) - len(2) - packet_id(2) - crc(2)
    uint16_t payload_len = len - 7;

    // 使用缓冲写入（数据从第5字节开始）
    esp_err_t err = buffered_ota_write(&data[5], payload_len);
    if (err != ESP_OK) {
        dPrint(DERROR, "缓冲写入失败: %s\n", esp_err_to_name(err));
        send_ack(OTA_ERR_FLASH_WRITE, packet_id, 0);
        return;
    }

    s_received_bytes += payload_len;
    s_expected_packet_id++;

    // 计算进度
    uint8_t progress = (s_total_size > 0) ? (s_received_bytes * 100) / s_total_size : 0;

    // 每10%打印一次
    static uint8_t last_progress = 0;
    if (progress >= last_progress + 10 || progress == 100) {
        dPrint(INFO, "OTA进度: %d%% (%u/%u bytes, 包#%d)\n",
               progress, s_received_bytes, s_total_size, packet_id);
        last_progress = progress;
    }

    // 批量ACK：每100个包发送一次ACK（配合手机端批量发送）
    // 条件：1) 每100个包 2) 最后一个包（接收完成）
    bool is_batch_boundary = ((packet_id + 1) % 100 == 0);  // packet_id从0开始，所以+1
    bool is_last_packet = (s_received_bytes >= s_total_size);

    if (is_batch_boundary || is_last_packet) {
        // 手机端已停止发送等待ACK，无需延迟
        send_ack(OTA_ACK_OK, packet_id, progress);
        dPrint(INFO, "批量ACK已发送: packet_id=%d, 已收%u个包, 进度=%d%%\n",
               packet_id, packet_id + 1, progress);
    }
}

/* ========== 处理结束帧 ========== */
static void process_end_frame(uint8_t *data, uint16_t len)
{
    if (!s_ota_started) {
        dPrint(WARN, "OTA未开始，忽略结束帧\n");
        send_ack(OTA_ERR_STATE_ERROR, 0, 0);
        return;
    }

    // 帧格式：[type(1)][len(2)][total_packets(2)][total_size(4)][final_crc32(4)][crc16(2)]
    // 最小长度：1 + 2 + 2 + 4 + 4 + 2 = 15
    if (len < 15) {
        dPrint(DERROR, "结束帧长度错误: %d\n", len);
        send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
        return;
    }

    // 解析字段
    uint16_t total_packets = data[3] | (data[4] << 8);
    uint32_t total_size = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
    uint32_t final_crc32 = data[9] | (data[10] << 8) | (data[11] << 16) | (data[12] << 24);

    // CRC校验
    uint16_t recv_crc = data[len - 2] | (data[len - 1] << 8);
    uint16_t calc_crc = crc16_calculate(data, len - 2);
    if (recv_crc != calc_crc) {
        dPrint(DERROR, "结束帧CRC错误: recv=0x%04X, calc=0x%04X\n", recv_crc, calc_crc);
        send_ack(OTA_ERR_CRC_MISMATCH, 0, 100);
        return;
    }

    dPrint(INFO, "========== OTA结束 ==========\n");
    dPrint(INFO, "总包数: %d, 总字节: %u, CRC32: 0x%08X\n",
           total_packets, total_size, final_crc32);

    // 校验大小
    if (total_size != s_received_bytes) {
        dPrint(DERROR, "大小不匹配: 期望%u, 收到%u\n", total_size, s_received_bytes);
        send_ack(OTA_ERR_SIZE_MISMATCH, 0, 100);
        esp_ota_abort(s_ota_handle);
        s_ota_started = false;
        return;
    }

    // 校验包数
    if (total_packets != s_expected_packet_id) {
        dPrint(DERROR, "包数不匹配: 期望%d, 收到%d\n", s_expected_packet_id, total_packets);
        send_ack(OTA_ERR_SIZE_MISMATCH, 0, 100);
        esp_ota_abort(s_ota_handle);
        s_ota_started = false;
        return;
    }

    // 刷新缓冲区，将剩余数据写入Flash
    esp_err_t err = flush_ota_buffer();
    if (err != ESP_OK) {
        dPrint(DERROR, "刷新缓冲区失败: %s\n", esp_err_to_name(err));
        send_ack(OTA_ERR_FLASH_WRITE, 0, 100);
        esp_ota_abort(s_ota_handle);
        s_ota_started = false;
        return;
    }

    // 结束OTA
    err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "esp_ota_end失败: %s\n", esp_err_to_name(err));
        send_ack(OTA_ERR_VERIFY_FAILED, 0, 100);
        s_ota_started = false;
        return;
    }

    // 设置启动分区
    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        dPrint(DERROR, "设置启动分区失败: %s\n", esp_err_to_name(err));
        send_ack(OTA_ERR_VERIFY_FAILED, 0, 100);
        s_ota_started = false;
        return;
    }

    s_ota_started = false;
    dPrint(INFO, "OTA成功！将在3秒后重启到新固件...\n");

    // 发送成功应答
    send_ack(OTA_ACK_OK, 0, 100);

    // 将新版本号写入NVS
    char version_str[16] = {0};
    snprintf(version_str, sizeof(version_str), "%lu.%lu.%lu",
             (s_new_version >> 24) & 0xFF,
             (s_new_version >> 16) & 0xFF,
             (s_new_version >> 8) & 0xFF);

    esp_err_t ver_ret = OTAManager_SetVersion(version_str);
    if (ver_ret == ESP_OK) {
        dPrint(INFO, "新固件版本号已保存: %s\n", version_str);
    } else {
        dPrint(WARN, "保存版本号失败，但OTA成功\n");
    }

    // 延时3秒后重启
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

/* ========== OTA帧处理入口 ========== */
esp_err_t OTAManager_ProcessFrame(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t frame_type = data[0];

    dPrint(DEBUG, "处理OTA帧: type=0x%02X, len=%d\n", frame_type, len);

    switch (frame_type) {
        case OTA_FRAME_START:
            process_start_frame(data, len);
            break;

        case OTA_FRAME_DATA:
            process_data_frame(data, len);
            break;

        case OTA_FRAME_END:
            process_end_frame(data, len);
            break;

        case OTA_FRAME_ACK:
            // 应答帧由设备发送，不应接收到
            dPrint(WARN, "收到应答帧（忽略）\n");
            break;

        default:
            dPrint(WARN, "未知OTA帧类型: 0x%02X\n", frame_type);
            send_ack(OTA_ERR_INVALID_FRAME, 0, 0);
            break;
    }

    return ESP_OK;
}

/* ========== 版本号管理 ========== */

/**
 * @brief 设置固件版本号
 * @param version 版本号字符串（例如："0.0.1"）
 * @return ESP_OK:成功, ESP_ERR_INVALID_ARG:参数无效, ESP_FAIL:写入失败
 */
esp_err_t OTAManager_SetVersion(const char *version)
{
    if (version == NULL) {
        dPrint(DERROR, "版本号参数为空\n");
        return ESP_ERR_INVALID_ARG;
    }

    // 检查版本号长度（最大15字节，例如"255.255.65535"）
    size_t len = strlen(version);
    if (len == 0 || len > 15) {
        dPrint(DERROR, "版本号长度无效: %d\n", len);
        return ESP_ERR_INVALID_ARG;
    }

    // 调用ConfigManager保存版本号
    ConfigStatus_t status = SetConfigString(OTA_VERSION_KEY, version);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "保存版本号失败: %d\n", status);
        return ESP_FAIL;
    }

    dPrint(INFO, "固件版本号已设置: %s\n", version);
    return ESP_OK;
}

/**
 * @brief 获取固件版本号
 * @param version 输出缓冲区，用于存储版本号字符串
 * @param max_len 缓冲区最大长度
 * @return ESP_OK:成功, ESP_ERR_INVALID_ARG:参数无效, ESP_ERR_NOT_FOUND:未找到版本号, ESP_FAIL:读取失败
 */
esp_err_t OTAManager_GetVersion(char *version, size_t max_len)
{
    if (version == NULL || max_len == 0) {
        dPrint(DERROR, "获取版本号参数无效\n");
        return ESP_ERR_INVALID_ARG;
    }

    // 调用ConfigManager读取版本号
    ConfigStatus_t status = GetConfigString(OTA_VERSION_KEY, version, max_len);

    if (status == CONFIG_ERROR_NOT_FOUND) {
        dPrint(WARN, "未找到版本号配置\n");
        return ESP_ERR_NOT_FOUND;
    } else if (status != CONFIG_OK) {
        dPrint(DERROR, "读取版本号失败: %d\n", status);
        return ESP_FAIL;
    }

    dPrint(INFO, "当前固件版本: %s\n", version);
    return ESP_OK;
}
