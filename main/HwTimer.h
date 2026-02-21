#ifndef __HW_TIMER_H__
#define __HW_TIMER_H__

#include <stdio.h>
#include "types.h"

// 最大定时器数量
#define MAX_HW_TIMERS       20

// 定时器ID类型
//typedef int HW_TIMER_ID;

typedef enum HW_TIMER_ID
{
    HW_TIMER_CHARGING_DURING_ID_0,            //对充电时长计算时间定时器,充电桩索引0
    HW_TIMER_CHARGING_DURING_ID_1,            //对充电时长计算时间定时器,充电桩索引1
    HW_TIMER_MAX_ID
}HW_TIMER_ID;

// 定时器回调函数类型
// 参数1: timerId - 定时器ID
// 参数2: arg - 用户自定义参数
// 参数3: argLen - 参数长度
typedef void (*HwTimerCallback)(HW_TIMER_ID timerId, void *arg, int argLen);

// 定时器模式
typedef enum {
    HW_TIMER_MODE_ONE_SHOT = 0,    // 单次触发
    HW_TIMER_MODE_PERIODIC = 1      // 周期触发
} HW_TIMER_MODE;

/********************************************************
 * @Function name: InitHwTimer
 * @Description: 初始化硬件定时器系统
 * @Return: RTN_SUCCESS-成功, RTN_FAIL-失败
 ********************************************************/
int InitHwTimer(void);

/********************************************************
 * @Function name: AddHwTimer
 * @Description: 添加/启动一个定时器
 * @param timerId - 定时器ID (用户自定义，建议使用枚举)
 * @param timeoutSeconds - 超时时间（秒）
 * @param mode - 定时器模式（单次/周期）
 * @param callback - 超时回调函数
 * @param arg - 用户参数（可为NULL）
 * @param argLen - 参数长度
 * @Return: RTN_SUCCESS-成功, RTN_FAIL-失败
 ********************************************************/
int AddHwTimer(HW_TIMER_ID timerId, unsigned int timeoutSeconds, HW_TIMER_MODE mode,
               HwTimerCallback callback, void *arg, int argLen);

/********************************************************
 * @Function name: DelHwTimer
 * @Description: 删除/停止一个定时器
 * @param timerId - 定时器ID
 * @Return: RTN_SUCCESS-成功, RTN_FAIL-失败
 ********************************************************/
int DelHwTimer(HW_TIMER_ID timerId);

/********************************************************
 * @Function name: IsHwTimerExist
 * @Description: 判断定时器是否存在
 * @param timerId - 定时器ID
 * @Return: TRUE-存在, FALSE-不存在
 ********************************************************/
BOOL IsHwTimerExist(HW_TIMER_ID timerId);

/********************************************************
 * @Function name: GetHwTimerRemain
 * @Description: 获取定时器剩余时间
 * @param timerId - 定时器ID
 * @Return: 剩余秒数，-1表示定时器不存在
 ********************************************************/
int GetHwTimerRemain(HW_TIMER_ID timerId);

/********************************************************
 * @Function name: ResetHwTimer
 * @Description: 重置定时器（重新开始计时）
 * @param timerId - 定时器ID
 * @Return: RTN_SUCCESS-成功, RTN_FAIL-失败
 ********************************************************/
int ResetHwTimer(HW_TIMER_ID timerId);

/********************************************************
 * @Function name: DeinitHwTimer
 * @Description: 反初始化硬件定时器系统
 * @Return: RTN_SUCCESS-成功, RTN_FAIL-失败
 ********************************************************/
int DeinitHwTimer(void);

#endif // __HW_TIMER_H__
