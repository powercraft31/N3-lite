#include "CTimer.h"
#include <string.h>

// 定时器结构体
typedef struct {
    uint8_t  active;        // 定时器是否活动: 1-活动, 0-未激活
    uint32_t timeout;       // 超时时间（毫秒）
    uint32_t counter;       // 当前计数值（毫秒）
    TimerFunc callback;     // 超时回调函数
} Timer_t;

// 定时器数组
static Timer_t g_timers[TIMER_MAX];

// 定时器系统初始化标志
static uint8_t g_timer_initialized = 0;

/*!
    \brief      初始化定时器系统
    \param[in]  none
    \param[out] none
    \retval     0: 成功, -1: 失败
*/
int InitTimer(void)
{
    // 清空所有定时器
    memset(g_timers, 0, sizeof(g_timers));

    g_timer_initialized = 1;

    return 0;
}

/*!
    \brief      添加/启动定时器
    \param[in]  timerId: 定时器ID
    \param[in]  iTmrLen: 超时时间（10毫秒）
    \param[in]  timerFunc: 超时回调函数
    \param[out] none
    \retval     0: 成功, -1: 失败
*/
int AddTimer(TIMER_ID timerId, int iTmrLen, TimerFunc timerFunc)
{
    // 参数检查
    if (timerId >= TIMER_MAX) {
        return -1;
    }

    if (iTmrLen <= 0) {
        return -1;
    }

    if (timerFunc == NULL) {
        return -1;
    }

    if (!g_timer_initialized) {
        InitTimer();
    }

    // 配置定时器
    g_timers[timerId].timeout = iTmrLen;
    g_timers[timerId].counter = iTmrLen;
    g_timers[timerId].callback = timerFunc;
    g_timers[timerId].active = 1;

    return 0;
}

/*!
    \brief      删除/停止定时器
    \param[in]  timerId: 定时器ID
    \param[out] none
    \retval     0: 成功, -1: 失败
*/
int DelTimer(TIMER_ID timerId)
{
    // 参数检查
    if (timerId >= TIMER_MAX) {
        return -1;
    }

    // 停止定时器
    g_timers[timerId].active = 0;
    g_timers[timerId].counter = 0;
    g_timers[timerId].callback = NULL;

    return 0;
}

/*!
    \brief      检查定时器是否存在（活动）
    \param[in]  timerId: 定时器ID
    \param[out] none
    \retval     1: 存在, 0: 不存在
*/
int IsExistTimer(TIMER_ID timerId)
{
    // 参数检查
    if (timerId >= TIMER_MAX) {
        return 0;
    }

    return g_timers[timerId].active ? 1 : 0;
}

/*!
    \brief      定时器更新函数（在SysTick中断中调用，1ms调用一次）
    \param[in]  none
    \param[out] none
    \retval     none
    \note       此函数应在 SysTick_Handler 中调用
*/
void TimerUpdate(void)
{
    uint8_t i;
    TimerFunc callback_func;

    if (!g_timer_initialized) {
        return;
    }

    // 遍历所有定时器
    for (i = 0; i < TIMER_MAX; i++) {
        // 跳过未激活的定时器
        if (!g_timers[i].active) {
            continue;
        }

        // 递减计数器
        if (g_timers[i].counter > 0) {
            g_timers[i].counter--;

            // 检查是否超时
            if (g_timers[i].counter == 0) {
                // 保存回调函数指针
                callback_func = g_timers[i].callback;

                // 先停止定时器（一次性定时器）
                // 如果回调函数中需要周期触发，可以在回调中重新调用AddTimer
                g_timers[i].active = 0;

                // 调用回调函数（在停止定时器之后调用）
                // 这样如果回调中调用AddTimer重新激活定时器，不会被覆盖
                if (callback_func != NULL) {
                    callback_func((TIMER_ID)i, NULL, 0);
                }
            }
        }
    }
}

/*!
    \brief      获取定时器剩余时间
    \param[in]  timerId: 定时器ID
    \param[out] none
    \retval     剩余时间（ms），-1表示定时器不存在
*/
int GetTimerRemain(TIMER_ID timerId)
{
    // 参数检查
    if (timerId >= TIMER_MAX) {
        return -1;
    }

    // 检查定时器是否活动
    if (!g_timers[timerId].active) {
        return -1;
    }

    return (int)g_timers[timerId].counter;
}
