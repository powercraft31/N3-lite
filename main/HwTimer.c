#include "HwTimer.h"
#include "esp_timer.h"
#include "DeBug.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// 定时器控制块结构
typedef struct {
    HW_TIMER_ID timerId;            // 定时器ID
    unsigned int timeoutSeconds;     // 超时时间（秒）
    unsigned int remainSeconds;      // 剩余时间（秒）
    HW_TIMER_MODE mode;             // 定时器模式
    HwTimerCallback callback;       // 回调函数
    void *arg;                      // 用户参数
    int argLen;                     // 参数长度
    BOOL isActive;                  // 是否激活
} HwTimerCtrlBlock;

// 定时器管理结构
static struct {
    HwTimerCtrlBlock timers[MAX_HW_TIMERS];  // 定时器数组
    esp_timer_handle_t hwTimer;              // ESP32硬件定时器句柄
    SemaphoreHandle_t mutex;                 // 互斥锁
    BOOL initialized;                        // 初始化标志
} g_hwTimerMgr = {0};

// 硬件定时器中断回调函数（每秒触发一次）
static void IRAM_ATTR hw_timer_callback(void *arg)
{
    // 遍历所有定时器，更新计数
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        HwTimerCtrlBlock *timer = &g_hwTimerMgr.timers[i];

        if (!timer->isActive) {
            continue;
        }

        // 减少剩余时间
        if (timer->remainSeconds > 0) {
            timer->remainSeconds--;

            // 检查是否超时
            if (timer->remainSeconds == 0) {
                // 调用回调函数
                if (timer->callback != NULL) {
                    timer->callback(timer->timerId, timer->arg, timer->argLen);
                }

                // 根据模式处理
                if (timer->mode == HW_TIMER_MODE_PERIODIC) {
                    // 周期模式：重新加载
                    timer->remainSeconds = timer->timeoutSeconds;
                } else {
                    // 单次模式：停止定时器
                    timer->isActive = FALSE;
                    if (timer->arg != NULL) {
                        free(timer->arg);
                        timer->arg = NULL;
                    }
                }
            }
        }
    }
}

/********************************************************
 * @Function name: InitHwTimer
 * @Description: 初始化硬件定时器系统
 ********************************************************/
int InitHwTimer(void)
{
    if (g_hwTimerMgr.initialized) {
        dPrint(WARN, "HwTimer already initialized\n");
        return RTN_SUCCESS;
    }

    // 创建互斥锁
    g_hwTimerMgr.mutex = xSemaphoreCreateMutex();
    if (g_hwTimerMgr.mutex == NULL) {
        dPrint(DERROR, "Failed to create mutex for HwTimer\n");
        return RTN_FAIL;
    }

    // 初始化定时器数组
    memset(g_hwTimerMgr.timers, 0, sizeof(g_hwTimerMgr.timers));

    // 创建ESP32硬件定时器（1秒精度）
    esp_timer_create_args_t timer_args = {
        .callback = &hw_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "hw_timer_1s"
    };

    esp_err_t err = esp_timer_create(&timer_args, &g_hwTimerMgr.hwTimer);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to create ESP32 timer: %s\n", esp_err_to_name(err));
        vSemaphoreDelete(g_hwTimerMgr.mutex);
        return RTN_FAIL;
    }

    // 启动硬件定时器（周期1秒）
    err = esp_timer_start_periodic(g_hwTimerMgr.hwTimer, 1000000); // 1秒 = 1000000微秒
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to start ESP32 timer: %s\n", esp_err_to_name(err));
        esp_timer_delete(g_hwTimerMgr.hwTimer);
        vSemaphoreDelete(g_hwTimerMgr.mutex);
        return RTN_FAIL;
    }

    g_hwTimerMgr.initialized = TRUE;
    dPrint(INFO, "HwTimer initialized successfully\n");

    return RTN_SUCCESS;
}

/********************************************************
 * @Function name: AddHwTimer
 * @Description: 添加/启动一个定时器
 ********************************************************/
int AddHwTimer(HW_TIMER_ID timerId, unsigned int timeoutSeconds, HW_TIMER_MODE mode,
               HwTimerCallback callback, void *arg, int argLen)
{
    if (!g_hwTimerMgr.initialized) {
        dPrint(DERROR, "HwTimer not initialized\n");
        return RTN_FAIL;
    }

    if (callback == NULL || timeoutSeconds == 0) {
        dPrint(DERROR, "Invalid parameters: callback=%p, timeoutSeconds=%u\n",
               callback, timeoutSeconds);
        return RTN_FAIL;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    // 查找是否已存在该ID的定时器
    int existingIndex = -1;
    int freeIndex = -1;

    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].isActive &&
            g_hwTimerMgr.timers[i].timerId == timerId) {
            existingIndex = i;
            break;
        }

        if (!g_hwTimerMgr.timers[i].isActive && freeIndex == -1) {
            freeIndex = i;
        }
    }

    int index;

    // 如果已存在，更新定时器
    if (existingIndex != -1) {
        index = existingIndex;
        dPrint(DEBUG, "Updating existing timer ID=%d\n", timerId);

        // 释放旧的参数内存
        if (g_hwTimerMgr.timers[index].arg != NULL) {
            free(g_hwTimerMgr.timers[index].arg);
            g_hwTimerMgr.timers[index].arg = NULL;
        }
    }
    // 如果不存在，使用空闲槽位
    else if (freeIndex != -1) {
        index = freeIndex;
        dPrint(DEBUG, "Adding new timer ID=%d\n", timerId);
    }
    // 没有空闲槽位
    else {
        dPrint(DERROR, "No free timer slot available\n");
        xSemaphoreGive(g_hwTimerMgr.mutex);
        return RTN_FAIL;
    }

    // 设置定时器参数
    HwTimerCtrlBlock *timer = &g_hwTimerMgr.timers[index];
    timer->timerId = timerId;
    timer->timeoutSeconds = timeoutSeconds;
    timer->remainSeconds = timeoutSeconds;
    timer->mode = mode;
    timer->callback = callback;
    timer->argLen = argLen;

    // 复制用户参数
    if (arg != NULL && argLen > 0) {
        timer->arg = malloc(argLen);
        if (timer->arg == NULL) {
            dPrint(DERROR, "Failed to allocate memory for timer arg\n");
            xSemaphoreGive(g_hwTimerMgr.mutex);
            return RTN_FAIL;
        }
        memcpy(timer->arg, arg, argLen);
    } else {
        timer->arg = NULL;
    }

    timer->isActive = TRUE;

    xSemaphoreGive(g_hwTimerMgr.mutex);

    dPrint(INFO, "Timer ID=%d added: timeout=%us, mode=%s\n",
           timerId, timeoutSeconds,
           (mode == HW_TIMER_MODE_ONE_SHOT) ? "ONE_SHOT" : "PERIODIC");

    return RTN_SUCCESS;
}

/********************************************************
 * @Function name: DelHwTimer
 * @Description: 删除/停止一个定时器
 ********************************************************/
int DelHwTimer(HW_TIMER_ID timerId)
{
    if (!g_hwTimerMgr.initialized) {
        dPrint(DERROR, "HwTimer not initialized\n");
        return RTN_FAIL;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    // 查找定时器
    BOOL found = FALSE;
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].isActive &&
            g_hwTimerMgr.timers[i].timerId == timerId) {

            // 释放参数内存
            if (g_hwTimerMgr.timers[i].arg != NULL) {
                free(g_hwTimerMgr.timers[i].arg);
                g_hwTimerMgr.timers[i].arg = NULL;
            }

            // 清空定时器
            memset(&g_hwTimerMgr.timers[i], 0, sizeof(HwTimerCtrlBlock));

            found = TRUE;
            dPrint(INFO, "Timer ID=%d deleted\n", timerId);
            break;
        }
    }

    xSemaphoreGive(g_hwTimerMgr.mutex);

    if (!found) {
        dPrint(WARN, "Timer ID=%d not found\n", timerId);
        return RTN_FAIL;
    }

    return RTN_SUCCESS;
}

/********************************************************
 * @Function name: IsHwTimerExist
 * @Description: 判断定时器是否存在
 ********************************************************/
BOOL IsHwTimerExist(HW_TIMER_ID timerId)
{
    if (!g_hwTimerMgr.initialized) {
        return FALSE;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    BOOL exists = FALSE;
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].isActive &&
            g_hwTimerMgr.timers[i].timerId == timerId) {
            exists = TRUE;
            break;
        }
    }

    xSemaphoreGive(g_hwTimerMgr.mutex);

    return exists;
}

/********************************************************
 * @Function name: GetHwTimerRemain
 * @Description: 获取定时器剩余时间
 ********************************************************/
int GetHwTimerRemain(HW_TIMER_ID timerId)
{
    if (!g_hwTimerMgr.initialized) {
        return -1;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    int remain = -1;
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].isActive &&
            g_hwTimerMgr.timers[i].timerId == timerId) {
            remain = g_hwTimerMgr.timers[i].remainSeconds;
            break;
        }
    }

    xSemaphoreGive(g_hwTimerMgr.mutex);

    return remain;
}

/********************************************************
 * @Function name: ResetHwTimer
 * @Description: 重置定时器（重新开始计时）
 ********************************************************/
int ResetHwTimer(HW_TIMER_ID timerId)
{
    if (!g_hwTimerMgr.initialized) {
        dPrint(DERROR, "HwTimer not initialized\n");
        return RTN_FAIL;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    BOOL found = FALSE;
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].isActive &&
            g_hwTimerMgr.timers[i].timerId == timerId) {

            // 重置剩余时间
            g_hwTimerMgr.timers[i].remainSeconds = g_hwTimerMgr.timers[i].timeoutSeconds;

            found = TRUE;
            dPrint(INFO, "Timer ID=%d reset to %u seconds\n",
                   timerId, g_hwTimerMgr.timers[i].timeoutSeconds);
            break;
        }
    }

    xSemaphoreGive(g_hwTimerMgr.mutex);

    if (!found) {
        dPrint(WARN, "Timer ID=%d not found\n", timerId);
        return RTN_FAIL;
    }

    return RTN_SUCCESS;
}

/********************************************************
 * @Function name: DeinitHwTimer
 * @Description: 反初始化硬件定时器系统
 ********************************************************/
int DeinitHwTimer(void)
{
    if (!g_hwTimerMgr.initialized) {
        dPrint(WARN, "HwTimer not initialized\n");
        return RTN_SUCCESS;
    }

    xSemaphoreTake(g_hwTimerMgr.mutex, portMAX_DELAY);

    // 停止并删除硬件定时器
    esp_timer_stop(g_hwTimerMgr.hwTimer);
    esp_timer_delete(g_hwTimerMgr.hwTimer);

    // 释放所有定时器的参数内存
    for (int i = 0; i < MAX_HW_TIMERS; i++) {
        if (g_hwTimerMgr.timers[i].arg != NULL) {
            free(g_hwTimerMgr.timers[i].arg);
        }
    }

    // 清空定时器数组
    memset(g_hwTimerMgr.timers, 0, sizeof(g_hwTimerMgr.timers));

    xSemaphoreGive(g_hwTimerMgr.mutex);

    // 删除互斥锁
    vSemaphoreDelete(g_hwTimerMgr.mutex);
    g_hwTimerMgr.mutex = NULL;

    g_hwTimerMgr.initialized = FALSE;

    dPrint(INFO, "HwTimer deinitialized\n");

    return RTN_SUCCESS;
}
