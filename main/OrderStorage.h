#ifndef __ORDER_STORAGE_H__
#define __ORDER_STORAGE_H__

#include "types.h"
#include <stdint.h>
#include "ChargingStation.h"

/**
 * @brief 初始化订单存储系统
 * @return 0-成功，-1-失败
 * @description 初始化并读取全局 ID
 */
int OrderStorage_Init(void);

/**
 * @brief 从充电桩结构体保存订单（直接组装 JSON 并保存）
 * @param station 充电桩结构体指针
 * @return 0-成功，-1-失败
 * @description 直接从 ChargingStation 组装 JSON 对象并保存到 Flash
 *              JSON 格式：{"startTime":"xxx","endTime":"xxx","energy":"xxx","duration":"xxx","subDevId":"xxx"}
 */
int OrderStorage_SaveFromStation(const ChargingStation *station);

/**
 * @brief 读取所有有效订单（返回 JSON 字符串）
 * @param json_buffer 输出参数，JSON 字符串缓冲区
 * @param buffer_size 缓冲区大小
 * @return 0-成功，-1-失败
 * @description 返回 JSON 数组格式的所有订单，例如：
 * [{"startTime":"1702345678","endTime":"1702349278","energy":"1234","duration":"3600","subDevId":"CHARGER-001"},...]
 */
int OrderStorage_ReadAllJSON(char *json_buffer, size_t buffer_size);

/**
 * @brief 清空所有订单数据
 * @return 0-成功，-1-失败
 * @description 清除所有订单和全局 ID
 */
int OrderStorage_ClearAll(void);

/**
 * @brief 获取当前全局 ID
 * @return 当前全局 ID
 */
uint32_t OrderStorage_GetGlobalId(void);

#endif // __ORDER_STORAGE_H__
