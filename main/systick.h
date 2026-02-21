#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#include "esp_err.h"

/**
 * @file    systick.h
 * @brief   基于FreeRTOS任务的软件定时器
 * @details 提供1ms周期性定时器轮询
 *          使用FreeRTOS任务（线程）每1ms轮询一次，用于驱动CTimer模块的TimerUpdate()函数
 */

/**
 * @brief 初始化SysTick定时器
 * @note 创建并启动1ms周期性软件定时器任务，自动在任务中调用TimerUpdate()
 *       使用FreeRTOS的vTaskDelayUntil()实现精确1ms周期
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_init(void);

/**
 * @brief 停止SysTick定时器
 * @note 挂起定时器任务，不释放资源，可通过systick_resume()恢复
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_stop(void);

/**
 * @brief 恢复SysTick定时器
 * @note 恢复之前通过systick_stop()挂起的定时器任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_resume(void);

/**
 * @brief 反初始化SysTick定时器
 * @note 停止并删除定时器任务，释放所有资源
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_deinit(void);

#endif // __SYSTICK_H__
