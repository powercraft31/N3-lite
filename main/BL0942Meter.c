#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "BL0942Meter.h"
#include <string.h>
#include <stdio.h>
#include "serial.h"
#include "DeBug.h"
#include "ConfigManager.h"
#include "StringUtils.h"
#include "ChargingStationManager.h"

/**
 * @file    BL0942Meter.c
 * @brief   USART2串口收发处理模块
 * @details 提供USART2的数据收发
 *          集成BL0942芯片寄存器读取功能（0x03, 0x04, 0x06, 0x07, 0x08）
 */

// BL0942 数据存储
static BL0942_Data_t bl0942_data = {0};

// BL0942 转换系数（全局变量，可根据实际校准修改）
BL0942_Coef_t bl0942_coef = {
    .voltage = 916.581,         // 电压转换系数
    .current = 83737.821,      // 电流转换系数
    .power = 130.606,            // 有功功率转换系数
    .energy = 0.00089184,       // 有功电能转换系数
    .frequency = 1000000.000      // 线电压频率转换系数
};

/**
 * @brief 从NVS加载BL0942系数配置
 * @return CONFIG_OK 成功, 其他值表示失败
 */
static ConfigStatus_t bl0942_load_config_from_nvs(void)
{
    char config_buffer[256] = {0};
    
    ConfigStatus_t ret = GetConfigString("bl0942_coef", config_buffer, sizeof(config_buffer));
    if (ret != CONFIG_OK) {
        dPrint(WARN, "未找到BL0942系数配置\n");
        return ret;
    }

    dPrint(INFO, "从NVS读取BL0942系数配置: %s\n", config_buffer);

    // 使用StringUtils解析JSON中的浮点数值
    float voltage = extract_float(config_buffer, "voltage");
    float current = extract_float(config_buffer, "current");
    float power = extract_float(config_buffer, "power");
    float energy = extract_float(config_buffer, "energy");
    float frequency = extract_float(config_buffer, "frequency");

    // 更新系数（如果解析成功且非零）
    if (voltage > 0) bl0942_coef.voltage = voltage;
    if (current > 0) bl0942_coef.current = current;
    if (power > 0) bl0942_coef.power = power;
    if (energy > 0) bl0942_coef.energy = energy;
    if (frequency > 0) bl0942_coef.frequency = frequency;

    dPrint(INFO, "BL0942系数加载完成: V=%.3f, I=%.3f, P=%.3f, E=%.8f, F=%.3f\n",
           bl0942_coef.voltage, bl0942_coef.current, bl0942_coef.power,
           bl0942_coef.energy, bl0942_coef.frequency);

    return CONFIG_OK;
}

/**
 * @brief 保存BL0942系数配置到NVS
 * @return CONFIG_OK 成功, 其他值表示失败
 */
static ConfigStatus_t bl0942_save_config_to_nvs(void)
{
    char config_json[256];

    // 构造JSON字符串（根据各字段特点使用合适的精度）
    // voltage, current, power 使用3位小数
    // energy 使用8位小数（因为值较小，如 0.00294308）
    snprintf(config_json, sizeof(config_json),
             "{\"voltage\":%.3f,\"current\":%.3f,\"power\":%.3f,\"energy\":%.8f,\"frequency\":%.3f}",
             bl0942_coef.voltage, bl0942_coef.current, bl0942_coef.power,
             bl0942_coef.energy, bl0942_coef.frequency);

    dPrint(INFO, "保存BL0942系数配置到NVS: %s\n", config_json);

    ConfigStatus_t ret = SetConfigString("bl0942_coef", config_json);
    if (ret != CONFIG_OK) {
        dPrint(DERROR, "保存BL0942系数配置失败, ret=%d\n", ret);
        return ret;
    }

    dPrint(INFO, "BL0942系数配置保存成功\n");
    return CONFIG_OK;
}

/**
 * @brief BL0942查询任务
 * @param pvParameters 任务参数（未使用）
 * @note 此任务每隔一定时间（默认1秒）查询一次BL0942所有寄存器
 */
static void bl0942_query_task(void *pvParameters)
{
    dPrint(INFO, "BL0942 query task started\n");

    // 等待1秒后开始首次查询
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        // 查询所有寄存器
        bl0942_query_all_registers();

        // 等待一段时间后再次查询（默认1秒）
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief 初始化usart2和BL0942查询任务
 * @note 在系统初始化时调用，初始化UART2并创建查询任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t bl0942_event_init(void)
{
    esp_err_t ret;
    ret = usart2_init();
    if (ret != ESP_OK) {
        dPrint(DERROR, "UART2 initialization failed\n");
        return ret;
    }

    // 从NVS读取系数配置
    //强制重写NVS配置（更新系数后使用，烧录一次后请恢复正常流程）
    //bl0942_save_config_to_nvs();
    //bl0942_save_config_to_nvs();
    // 正常流程
    ConfigStatus_t config_ret = bl0942_load_config_from_nvs();
    if (config_ret != CONFIG_OK) {
        // 配置不存在，保存默认配置
        dPrint(INFO, "BL0942系数配置不存在，保存默认配置\n");
        bl0942_save_config_to_nvs();
    }
    

    xTaskCreate(bl0942_query_task, "bl0942_query", 4096, NULL, 5, NULL);
    dPrint(INFO, "BL0942 event init done\n");
    return ESP_OK;
}

/**
 * @brief USART2初始化函数
 * @note ESP32 UART2配置
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t usart2_init()
{
    return ESP_OK;
}

/**
 * @brief BL0942读取单个寄存器
 * @param reg_addr 寄存器地址
 * @param rx_data 接收数据缓冲区（4字节：DATA[0-2] + CHECKSUM）
 * @return 0=成功, -1=失败
 * @note 发送格式：[帧识别字节 0x5A] [寄存器地址]
 *       接收格式：[DATA[7:0]] [DATA[15:8]] [DATA[23:16]] [CHECKSUM]
 */
int bl0942_read_register(uint8_t reg_addr, uint8_t *rx_data)
{
    if (rx_data == NULL) {
        return -1;
    }

    // 准备发送命令：[帧识别字节 0x5A] [寄存器地址]
    uint8_t tx_buf[2] = {BL0942_READ_CMD, reg_addr};

    //dPrint(DEBUG, "BL0942 Read Reg: 0x%02X (Send: 0x%02X 0x%02X)\n", reg_addr, BL0942_READ_CMD, reg_addr);

    int received = serial_read_with_result(SERIAL_UART2, tx_buf, 2,
                                          rx_data, BL0942_REG_DATA_SIZE, 500);

    // 先判断是否通信异常（没收到数据或出错）
    if(received <= 0)
    {
        //设备通信异常
        bl0942_data.isConnected = false;
        dPrint(WARN, "BL0942 communication error for reg 0x%02X (received %d bytes)\n", reg_addr, received);
        return -1;
    }

    // 再判断数据是否完整
    if (received != BL0942_REG_DATA_SIZE) {
        dPrint(WARN, "BL0942 read incomplete for reg 0x%02X (received %d/%d bytes)\n",
               reg_addr, received, BL0942_REG_DATA_SIZE);
        bl0942_data.isConnected = false;  // 数据不完整也认为是通信异常
        return -1;
    }

    // 数据接收成功
    bl0942_data.isConnected = true;
    // 打印接收到的数据
    /*
    dPrint(DEBUG, "BL0942 RX: 0x%02X 0x%02X 0x%02X 0x%02X\n",
           rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
    */
    return 0;
}

/**
 * @brief BL0942查询所有配置的寄存器(5个)
 * @note 依次查询5个寄存器，每完成一帧读写操作后，等待>20ms再进行下一帧
 *       查询成功后会自动更新全局数据结构bl0942_data
 */
void bl0942_query_all_registers(void)
{
    static uint32_t last_print_time = 0;  // 上次打印时间戳（毫秒）
    //dPrint(DEBUG, "BL0942: Querying 5 registers...\n");

    // 定义5个寄存器地址数组
    uint8_t reg_addresses[BL0942_REG_COUNT] = {
        BL0942_REG_ADDR_03,  // 电流有效值
        BL0942_REG_ADDR_04,  // 电压有效值
        BL0942_REG_ADDR_06,  // 有功功率
        BL0942_REG_ADDR_07,  // 有功电能
        BL0942_REG_ADDR_08   // 线电压频率
    };

    uint16_t valid_count = 0;
    uint16_t error_count = 0;
    uint32_t temp_data[BL0942_REG_COUNT] = {0};

    // 逐个查询5个寄存器
    for (uint16_t i = 0; i < BL0942_REG_COUNT; i++) {
        uint8_t rx_data[BL0942_REG_DATA_SIZE];

        // 同步读取寄存器
        int ret = bl0942_read_register(reg_addresses[i], rx_data);

        if (ret == 0) {
            // 计算校验和
            uint8_t calc_checksum = bl0942_calc_checksum(BL0942_READ_CMD, reg_addresses[i], rx_data);

            // 校验第4字节
            if (calc_checksum == rx_data[3]) {
                valid_count++;
                // 提取24位数据值（小端格式）
                temp_data[i] = rx_data[0] | (rx_data[1] << 8) | (rx_data[2] << 16);
            } else {
                error_count++;
                dPrint(WARN, "BL0942 checksum error for reg 0x%02X (calc=0x%02X, recv=0x%02X)\n",
                       reg_addresses[i], calc_checksum, rx_data[3]);
            }
        } else {
            error_count++;
        }

        // 帧间隔延时 >20ms
        if (i < BL0942_REG_COUNT - 1) {
            vTaskDelay(pdMS_TO_TICKS(BL0942_FRAME_INTERVAL_MS));
        }
    }

    // 数据有效性判断
    if (error_count == 0) {
        // 所有数据校验通过，转换并更新到全局结构体
        // 使用float计算后舍去小数部分转为uint16_t/uint32_t
        bl0942_data.current = (uint16_t)bl0942_convert_to_actual(temp_data[0], bl0942_coef.current, false);
        bl0942_data.voltage = (uint16_t)bl0942_convert_to_actual(temp_data[1], bl0942_coef.voltage, false);
        bl0942_data.power = (uint16_t)bl0942_convert_to_actual(temp_data[2], bl0942_coef.power, false);
        bl0942_data.energy = (uint32_t)bl0942_convert_to_actual(temp_data[3], bl0942_coef.energy, true);
        bl0942_data.frequency = (uint16_t)((bl0942_coef.frequency / (float)temp_data[4]) * BL0942_DATA_CONVERT_COEF);

        // 每10秒打印一次数据
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_print_time >= 10000) {
            dPrint(DEBUG, "BL0942数据:\n  电流:   %u A\n  电压:   %u V\n  功率:   %u W\n  电能:   %u Wh\n  频率:   %u Hz\n",
                   bl0942_data.current, bl0942_data.voltage, bl0942_data.power,
                   bl0942_data.energy, bl0942_data.frequency);
            //打印充电桩信息
            PrintChargingStationData();
            last_print_time = current_time;
        }
    } else {
        dPrint(WARN, "BL0942 query failed (%d errors) - Data not updated\n", error_count);
    }
}

/**
 * @brief BL0942数据校验和计算
 * @param frame_id 帧识别字节（0x5A）
 * @param reg_addr 寄存器地址
 * @param pData 数据指针（3字节：DATA[7:0], DATA[15:8], DATA[23:16]）
 * @return 计算得到的校验和
 * @note CHECKSUM = ~((frame_id + reg_addr + DATA[0] + DATA[1] + DATA[2]) & 0xFF)
 */
uint8_t bl0942_calc_checksum(uint8_t frame_id, uint8_t reg_addr, uint8_t *pData)
{
    uint8_t sum = frame_id + reg_addr + pData[0] + pData[1] + pData[2];
    sum = sum & 0xFF;  // 取低8位
    sum = ~sum;        // 取反

    return sum;
}

/**
 * @brief 获取BL0942最新数据
 * @param data 指向BL0942_Data_t结构体的指针
 * @return 1=数据有效, 0=数据无效
 */
uint8_t bl0942_get_data(BL0942_Data_t *data)
{
    if (data == NULL) {
        return 0;
    }

    // 复制数据
    data->current = bl0942_data.current;
    data->voltage = bl0942_data.voltage;
    data->power = bl0942_data.power;
    data->energy = bl0942_data.energy;
    data->frequency = bl0942_data.frequency;
    data->isConnected = bl0942_data.isConnected;
    return 1;
}

/**
 * @brief 获取BL0942电流有效值
 * @return 电流有效值
 * @note 直接返回已转换并存储的实际值
 */
uint16_t bl0942_get_current(void)
{
    return bl0942_data.current;
}

/**
 * @brief 获取BL0942连接状态
 * @return true=已连接, false=未连接/通信异常
 */
bool bl0942_is_connected(void)
{
    return bl0942_data.isConnected;
}

/**
 * @brief 将BL0942原始数据转换为实际物理量
 * @param raw_value 24位原始数据
 * @param coef 转换系数
 * @param is_multiply true=乘法(raw×coef), false=除法(raw÷coef)
 * @return 转换后的实际值（float）
 * @note 转换公式：
 *       - 乘法模式：实际值 = 原始值 × 系数
 *       - 除法模式：实际值 = 原始值 ÷ 系数
 */
float bl0942_convert_to_actual(uint32_t raw_value, float coef, bool is_multiply)
{
    if (raw_value == 0 || coef == 0) {
        return 0.0f;
    }

    if (is_multiply) {
        // 乘法模式
        return (float)raw_value * coef * BL0942_DATA_CONVERT_COEF;
    } else {
        // 除法模式
        return (float)raw_value / coef * BL0942_DATA_CONVERT_COEF;
    }
}

/**
 * @brief 设置BL0942转换系数
 * @param coef_json JSON格式的系数配置字符串
 * @return 0=成功, -1=失败
 */
int bl0942_set_coef(const char *coef_json)
{
    if (!coef_json) {
        dPrint(DERROR, "BL0942系数配置JSON为空\n");
        return -1;
    }

    dPrint(INFO, "收到BL0942系数配置: %s\n", coef_json);

    // 解析JSON并更新系数
    float voltage = extract_float(coef_json, "voltage");
    float current = extract_float(coef_json, "current");
    float power = extract_float(coef_json, "power");
    float energy = extract_float(coef_json, "energy");
    float frequency = extract_float(coef_json, "frequency");

    // 更新系数（如果解析成功且非零）
    if (voltage > 0) bl0942_coef.voltage = voltage;
    if (current > 0) bl0942_coef.current = current;
    if (power > 0) bl0942_coef.power = power;
    if (energy > 0) bl0942_coef.energy = energy;
    if (frequency > 0) bl0942_coef.frequency = frequency;

    dPrint(INFO, "BL0942系数更新: V=%.8f, I=%.8f, P=%.8f, E=%.8f, F=%.8f\n",
           bl0942_coef.voltage, bl0942_coef.current, bl0942_coef.power,
           bl0942_coef.energy, bl0942_coef.frequency);

    // 直接保存用户提供的原始JSON字符串到NVS，保持原有精度格式
    ConfigStatus_t ret = SetConfigString("bl0942_coef", coef_json);
    if (ret != CONFIG_OK) {
        dPrint(DERROR, "保存BL0942系数到NVS失败, ret=%d\n", ret);
        return -1;
    }

    dPrint(INFO, "BL0942系数配置保存成功\n");
    return 0;
}

/**
 * @brief 获取BL0942转换系数
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 0=成功, -1=失败
 */
int bl0942_get_coef(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        dPrint(DERROR, "缓冲区参数无效\n");
        return -1;
    }

    // 直接从NVS读取JSON配置并返回
    ConfigStatus_t status = GetConfigString("bl0942_coef", buffer, buffer_size);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "从NVS读取BL0942系数失败, 错误码: %d\n", status);
        return -1;
    }

    dPrint(INFO, "从NVS读取BL0942系数成功: %s\n", buffer);
    return 0;
}