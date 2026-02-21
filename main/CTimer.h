#ifndef __CTIMER_H__
#define __CTIMER_H__

#include <stdio.h>

// 定时器ID枚举，可以根据需要添加更多定时器
typedef enum TIMER_ID
{
	TIMER_ID_TEST1 = 0,    // 测试定时器1
	TIMER_ID_TEST2,        // 测试定时器2
	TIMER_ID_WATCHDOG,     // 看门狗定时器
	TIMER_ID_METER,        // 电表定时器
	TIMER_ID_COMM,         // 通信定时器
	TIMER_ID_LOADBALANCE,  // 负载均衡定时器
	TIMER_ID_SELECT_STATION_REAL_DATA,	//查询充电桩实时数据定时器
	TIMER_ID_CEHCK_STATION_CONNECT_STATUS, //判断充电桩的在离线状态
	TIMER_MAX              // 定时器最大数量
}TIMER_ID;

// 定时器回调函数类型
// 参数1: timerId - 定时器ID
// 参数2: 保留参数（可传递字符串）
// 参数3: 保留参数（可传递整数）
typedef void (*TimerFunc)(TIMER_ID timerId, char *, int);

// 初始化定时器系统
// 返回: 0-成功, -1-失败
int InitTimer(void);

// 添加/启动定时器
// 参数: timerId - 定时器ID
//       iTmrLen - 定时器超时时间（10毫秒）
//       timerFunc - 超时回调函数
// 返回: 0-成功, -1-失败
int AddTimer(TIMER_ID timerId, int iTmrLen, TimerFunc timerFunc);

// 删除/停止定时器
// 参数: timerId - 定时器ID
// 返回: 0-成功, -1-失败
int DelTimer(TIMER_ID timerId);

// 检查定时器是否存在
// 参数: timerId - 定时器ID
// 返回: 1-存在, 0-不存在
int IsExistTimer(TIMER_ID timerId);

// 定时器更新函数（在SysTick中断中调用，1ms调用一次）
void TimerUpdate(void);

// 获取定时器剩余时间（毫秒）
// 参数: timerId - 定时器ID
// 返回: 剩余时间（ms），-1表示定时器不存在
int GetTimerRemain(TIMER_ID timerId);


#endif

