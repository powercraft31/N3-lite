#ifndef __BL0942_METER_H__
#define __BL0942_METER_H__

#include <stdint.h>
#include "esp_err.h"
#include "CEvent.h"
#include "CTimer.h"
#include <stdbool.h>

// BL0942寄存器地址定义
#define BL0942_REG_ADDR_03  0x03    // 电流有效值
#define BL0942_REG_ADDR_04  0x04    // 电压有效值
#define BL0942_REG_ADDR_06  0x06    // 有功功率寄存器
#define BL0942_REG_ADDR_07  0x07    // 有功电能
#define BL0942_REG_ADDR_08  0x08    // 线电压频率

// BL0942帧识别字节
#define BL0942_READ_CMD     0x58

// 每个寄存器返回4字节数据 (24位数据 + 1字节校验和)
#define BL0942_REG_DATA_SIZE 4
// 总共5个寄存器
#define BL0942_REG_COUNT     5

// BL0942 通信时序要求（根据手册3.2.6节）
#define BL0942_FRAME_INTERVAL_MS  25   // 帧间隔时间 >20ms，使用25ms

#define BL0942_DATA_CONVERT_COEF    10 // 电流、电压、功率、电能、频率转换系数

// BL0942 数据结构定义（存储转换后的实际值）
typedef struct {
    uint16_t current;       // 电流有效值 (单位：A) - 转换后的实际值
    uint16_t voltage;       // 电压有效值 (单位：V) - 转换后的实际值
    uint16_t power;         // 有功功率   (单位：W) - 转换后的实际值
    uint32_t energy;        // 有功电能   (单位：Wh) - 转换后的实际值
    uint16_t frequency;     // 线电压频率 (单位：Hz) - 转换后的实际值
    bool isConnected;       // 设备是否在线
} BL0942_Data_t;

// BL0942 转换系数结构体
typedef struct {
    float voltage;      // 电压转换系数
    float current;      // 电流转换系数
    float power;        // 有功功率转换系数
    float energy;       // 有功电能转换系数
    float frequency;    // 线电压频率转换系数
} BL0942_Coef_t;

// USART2 函数声明

/**
 * @brief USART2初始化函数
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t usart2_init(void);

/**
 * @brief BL0942读取单个寄存器（同步模式）
 * @param reg_addr 寄存器地址
 * @param rx_data 接收数据缓冲区（4字节：DATA[0-2] + CHECKSUM）
 * @return 0=成功, -1=失败
 */
int bl0942_read_register(uint8_t reg_addr, uint8_t *rx_data);

/**
 * @brief BL0942查询所有配置的寄存器（5个）- 同步模式
 * @note 依次查询 0x03, 0x04, 0x06, 0x07, 0x08
 *       查询成功后会自动更新全局数据结构bl0942_data
 */
void bl0942_query_all_registers(void);

/**
 * @brief BL0942数据校验和计算
 * @param frame_id 帧识别字节（0x5A）
 * @param reg_addr 寄存器地址
 * @param pData 数据指针（3字节：DATA[7:0], DATA[15:8], DATA[23:16]）
 * @return 计算得到的校验和
 * @note CHECKSUM = ~((frame_id + reg_addr + DATA[0] + DATA[1] + DATA[2]) & 0xFF)
 */
uint8_t bl0942_calc_checksum(uint8_t frame_id, uint8_t reg_addr, uint8_t *pData);

/**
 * @brief 获取BL0942最新数据
 * @param data 指向BL0942_Data_t结构体的指针，用于接收数据
 * @return 1=数据有效, 0=数据无效
 * @note 调用此函数后会获取最近一次成功接收并校验的数据
 */
uint8_t bl0942_get_data(BL0942_Data_t *data);

/**
 * @brief 获取BL0942电流有效值
 * @return 电流有效值（转换为uint16_t，单位：A）
 */
uint16_t bl0942_get_current(void);

/**
 * @brief 获取BL0942连接状态
 * @return true=已连接, false=未连接/通信异常
 */
bool bl0942_is_connected(void);

/**
 * @brief 将BL0942原始数据转换为实际物理量
 * @param raw_value 24位原始数据
 * @param coef 转换系数
 * @param is_multiply true=乘法(raw×coef), false=除法(raw÷coef)
 * @return 转换后的实际值
 */
float bl0942_convert_to_actual(uint32_t raw_value, float coef, bool is_multiply);

/**
 * @brief 初始化BL0942模块
 * @note 在系统初始化时调用，初始化UART2并启动定时查询
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t bl0942_event_init(void);

/**
 * @brief 设置BL0942转换系数
 * @param coef_json JSON格式的系数配置字符串
 * @return 0=成功, -1=失败
 * @note JSON格式: {"voltage":916.581,"current":276334.811,"power":39.578,"energy":0.00294308,"frequency":1000000.0}
 */
int bl0942_set_coef(const char *coef_json);

/**
 * @brief 获取BL0942转换系数
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 0=成功, -1=失败
 */
int bl0942_get_coef(char *buffer, size_t buffer_size);

#endif
