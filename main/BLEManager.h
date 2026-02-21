#ifndef _BLE_MANAGER_H_
#define _BLE_MANAGER_H_

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * =============================================================================
 * BLE服务定义
 * =============================================================================
 * 本模块实现双服务架构：
 * 1. 心率服务(Heart Rate Service) - 让设备在iOS系统蓝牙设置中可见
 * 2. Nordic UART Service - 提供双向数据通信功能
 */

// Nordic UART Service UUID（用于数据通信）
#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // Nordic UART Service
#define BLE_CHAR_TX_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // TX特性(设备->手机，通知)
#define BLE_CHAR_RX_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // RX特性(手机->设备，写入)

// 心率服务UUID（标准BLE SIG服务，iOS系统识别）
#define BLE_HEART_RATE_SERVICE_UUID     0x180D  // Heart Rate Service (标准服务)
#define BLE_HEART_RATE_MEASUREMENT_UUID 0x2A37  // Heart Rate Measurement (心率测量特性)

// BLE设备名称
#define BLE_DEVICE_NAME         "N3Lite"

/* ========== RPC异步处理配置 ========== */
#define RPC_QUEUE_SIZE 150                   // RPC请求队列大小
#define RPC_MAX_DATA_LEN 512                // 单个RPC请求最大长度

typedef struct {
    uint8_t data[RPC_MAX_DATA_LEN];         // 完整的JSON请求数据
    uint16_t len;                            // 数据长度
} rpc_request_t;

// BLE连接状态
typedef enum {
    BLE_STATUS_DISCONNECTED = 0,
    BLE_STATUS_CONNECTED = 1,
    BLE_STATUS_ADVERTISING = 2
} BLEStatus_t;

// BLE数据接收回调函数类型
typedef void (*BLE_RxCallback_t)(uint8_t *data, uint16_t len);

/**
 * @brief 初始化BLE管理器
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_FAIL: 失败
 */
esp_err_t BLEManager_Init(void);

/**
 * @brief 设置数据接收回调函数
 *
 * @param callback 回调函数指针
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t BLEManager_SetRxCallback(BLE_RxCallback_t callback);

/**
 * @brief 通过BLE发送数据到手机
 *
 * @param data 要发送的数据
 * @param len 数据长度
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_INVALID_STATE: BLE未连接
 *         - ESP_FAIL: 发送失败
 */
esp_err_t BLEManager_SendData(uint8_t *data, uint16_t len);

/**
 * @brief 获取当前BLE连接状态
 *
 * @return BLEStatus_t 当前状态
 */
BLEStatus_t BLEManager_GetStatus(void);

/**
 * @brief 开始广播
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t BLEManager_StartAdvertising(void);

/**
 * @brief 停止广播
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t BLEManager_StopAdvertising(void);

/**
 * @brief 断开BLE连接
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t BLEManager_Disconnect(void);

#endif /* _BLE_MANAGER_H_ */
