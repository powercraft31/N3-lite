#ifndef __GPIO_MANAGER_H__
#define __GPIO_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include "led.h"

/**
 * @file    GPIOManager.h
 * @brief   GPIO管理模块 - 提供按键和LED的操作接口
 */

// 电流档位枚举
typedef enum CurrentLevel {
    LEVEL_20A = 20,
    LEVEL_25A = 25,
    LEVEL_32A = 32,
    LEVEL_40A = 40,
    LEVEL_50A = 50,
    LEVEL_63A = 63
} CurrentLevel_t;


#define KEY_LONG_PRESS_TIME_MS  3000    // 长按时间阈值：3秒
#define KEY_SHORT_PRESS_TIME_MS 50      // 短按消抖时间：50ms
#define LED_BLINK_COUNT         1       // LED闪烁次数
#define LED_BLINK_INTERVAL_MS   200     // LED闪烁间隔：200ms

// 按键状态枚举
typedef enum {
    KEY_STATE_IDLE = 0,         // 空闲状态
    KEY_STATE_PRESSED,          // 按下状态
    KEY_STATE_LONG_PRESS,       // 长按状态
    KEY_STATE_RELEASED          // 释放状态
} KeyState_t;

/**
 * @brief 初始化GPIO管理模块
 * @return 0=成功, -1=失败
 * @note 从Flash加载或初始化入户电流配置
 */
int GPIOManager_Init(void);

/**
 * @brief 获取入户电流值
 * @return 入户电流值
 */
uint8_t GPIOManager_GetInletCurrent(void);

/**
 * @brief 设置入户电流值并保存到Flash
 * @param current 入户电流值
 * @return 0=成功, -1=失败
 */
int GPIOManager_SetInletCurrent(uint8_t current);

void GPIOManager_Control_Task(void *pvParameter);

#endif // __GPIO_MANAGER_H__
