#include "systick.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "DeBug.h"
#include "CTimer.h"

/**
 * @file    systick.c
 * @brief   ESP32 SysTick 模拟实现（基于FreeRTOS任务）
 * @details 使用FreeRTOS任务实现1ms周期性软件定时器轮询
 */

// SysTick任务句柄
static TaskHandle_t systick_task_handle = NULL;

// SysTick任务运行标志
static volatile bool systick_running = false;

/**
 * @brief SysTick软件定时器任务
 * @note 此任务每1ms轮询一次，调用TimerUpdate()更新所有软件定时器
 * @param pvParameters 任务参数
 */
static void systick_timer_task(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 10ms周期

    // 初始化xLastWakeTime为当前时间
    xLastWakeTime = xTaskGetTickCount();

    dPrint(INFO, "SysTick timer task started\n");

    while (systick_running) {
        // 等待下一个周期（精确延时）
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 调用定时器更新函数
        TimerUpdate();
    }

    dPrint(INFO, "SysTick timer task stopped\n");

    // 任务退出前清空任务句柄
    systick_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief 初始化SysTick定时器
 * @note 创建并启动1ms周期性软件定时器任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_init(void)
{
    BaseType_t ret;
    // 检查是否已经初始化
    if (systick_task_handle != NULL) {
        dPrint(WARN, "SysTick already initialized\n");
        return ESP_OK;
    }
    // 设置运行标志
    systick_running = true;
    ret = xTaskCreate(systick_timer_task,
                      "systick_task",
                      4096,
                      NULL,
                      5,
                      &systick_task_handle);

    if (ret != pdPASS) {
        dPrint(DERROR, "Failed to create SysTick task\n");
        systick_running = false;
        systick_task_handle = NULL;
        return ESP_FAIL;
    }

    dPrint(INFO, "SysTick initialized successfully (Software Timer Task, 1ms period)\n");

    return ESP_OK;
}

/**
 * @brief 停止SysTick定时器
 * @note 停止任务运行但不删除任务，可以用systick_resume()恢复
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_stop(void)
{
    if (systick_task_handle == NULL) {
        dPrint(WARN, "SysTick not initialized\n");
        return ESP_FAIL;
    }

    // 挂起任务
    vTaskSuspend(systick_task_handle);

    dPrint(INFO, "SysTick task suspended\n");
    return ESP_OK;
}

/**
 * @brief 恢复SysTick定时器
 * @note 恢复之前停止的任务
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_resume(void)
{
    if (systick_task_handle == NULL) {
        dPrint(WARN, "SysTick not initialized\n");
        return ESP_FAIL;
    }

    // 恢复任务
    vTaskResume(systick_task_handle);

    dPrint(INFO, "SysTick task resumed\n");
    return ESP_OK;
}

/**
 * @brief 反初始化SysTick定时器
 * @note 停止并删除任务，释放所有资源
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t systick_deinit(void)
{
    if (systick_task_handle == NULL) {
        dPrint(WARN, "SysTick not initialized\n");
        return ESP_OK;
    }

    // 设置停止标志，让任务自行退出
    systick_running = false;

    // 等待任务退出（最多等待100ms）
    int wait_count = 0;
    while (systick_task_handle != NULL && wait_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(1));
        wait_count++;
    }

    // 如果任务仍然存在，强制删除
    if (systick_task_handle != NULL) {
        dPrint(WARN, "Force delete SysTick task\n");
        vTaskDelete(systick_task_handle);
        systick_task_handle = NULL;
    }

    dPrint(INFO, "SysTick deinitialized\n");

    return ESP_OK;
}