#ifndef _OTA_MANAGER_H_
#define _OTA_MANAGER_H_

#include "esp_err.h"
#include <stdint.h>

/**
 * 帧格式：
 * 开始帧(0x01): [type][len][total_size][version][crc16]
 * 数据帧(0x02): [type][len][packet_id][data...][crc16]
 * 结束帧(0x03): [type][len][total_packets][total_size][final_crc32][crc16]
 * 应答帧(0x04): [type][error_code][packet_id][received_bytes][progress]
 */

#define OTA_VERSION_KEY "fw_version"          // 固件版本号的NVS key
#define OTA_DEFAULT_VERSION "0.0.1"           // 出厂默认版本号

/* ========== 帧类型枚举 ========== */
typedef enum {
    OTA_FRAME_START   = 0x01,  // 开始帧
    OTA_FRAME_DATA    = 0x02,  // 数据帧
    OTA_FRAME_END     = 0x03,  // 结束帧
    OTA_FRAME_ACK     = 0x04,  // 应答帧
} OTA_Frame_Type_t;

/* ========== 错误码枚举 ========== */
typedef enum {
    OTA_ACK_OK              = 0x00,  // 成功
    OTA_ERR_INVALID_FRAME   = 0x10,  // 帧格式错误
    OTA_ERR_CRC_MISMATCH    = 0x11,  // CRC校验失败
    OTA_ERR_SEQ_ERROR       = 0x12,  // 包序号错误
    OTA_ERR_FLASH_WRITE     = 0x13,  // Flash写入失败
    OTA_ERR_SIZE_MISMATCH   = 0x14,  // 大小不匹配
    OTA_ERR_VERIFY_FAILED   = 0x15,  // 固件验证失败
    OTA_ERR_STATE_ERROR     = 0x17,  // 状态错误
} OTA_Error_Code_t;

/**
 * @brief 初始化OTA管理器
 * @return ESP_OK:成功
 * @note 该函数会检查固件版本号，如果不存在则写入出厂默认版本"0.0.1"
 */
esp_err_t OTAManager_Init(void);

/**
 * @brief 处理接收到的OTA帧数据
 * @param data 接收到的数据
 * @param len 数据长度
 * @return ESP_OK:成功
 * @note 此函数在rpc_task中被调用，会自动识别帧类型并处理
 */
esp_err_t OTAManager_ProcessFrame(uint8_t *data, uint16_t len);

/**
 * @brief 设置固件版本号
 * @param version 版本号字符串（例如："0.0.1"）
 * @return ESP_OK:成功, ESP_ERR_INVALID_ARG:参数无效, ESP_FAIL:写入失败
 * @note 版本号会持久化保存到NVS中
 */
esp_err_t OTAManager_SetVersion(const char *version);

/**
 * @brief 获取固件版本号
 * @param version 输出缓冲区，用于存储版本号字符串
 * @param max_len 缓冲区最大长度
 * @return ESP_OK:成功, ESP_ERR_INVALID_ARG:参数无效, ESP_ERR_NOT_FOUND:未找到版本号, ESP_FAIL:读取失败
 * @note 如果从未设置过版本号，将返回ESP_ERR_NOT_FOUND
 */
esp_err_t OTAManager_GetVersion(char *version, size_t max_len);

#endif /* _OTA_MANAGER_H_ */
