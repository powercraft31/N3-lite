#ifndef __ALLOCATION_CONTROLLER_H__
#define __ALLOCATION_CONTROLLER_H__

#include "ChargingStation.h"
#include "esp_err.h"
#include "CTimer.h"

// EV充电桩电流配置范围
#define EV_MIN_CURRENT             6     // EV充电桩最小电流 6A
#define EV_MAX_CURRENT             32    // EV充电桩最大电流 32A

// 负载均衡控制参数
#define MIN_ADJUST_THRESHOLD       2     // 非过载时，差异小于2A不调整
#define MODE_SWITCH_HYSTERESIS     1     // 模式切换滞回值，避免在临界值附近频繁切换

// 稳定电流跟踪参数
#define STABLE_CURRENT_SAMPLE_COUNT 3    // 需要连续3次采样判断稳定
#define STABLE_CURRENT_TOLERANCE    2    // 稳定电流误差范围±2A

// 稳定电流跟踪结构
typedef struct {
    uint16_t historyCurrent[STABLE_CURRENT_SAMPLE_COUNT]; // 历史电流值（A）
    uint8_t historyIndex;                                   // 当前历史记录索引
    uint8_t sampleCount;                                    // 已采样次数
    bool isStable;                                          // 是否达到稳定状态
    uint16_t stableCurrent;                                 // 稳定电流值（A）
} StableCurrentTracker;

/********************************************************
*@Function name:AutoControlInit
*@Description:自动控制初始化，外部调用
*@Return:ESP_OK成功
********************************************************************************/
esp_err_t AutoControlInit(void);

/********************************************************
*@Function name:AutoControl_task
*@Description:自动控制任务
*@input param:pvParameter 任务参数
*@Return:
********************************************************************************/
void AutoControl_task(void *pvParameter);

/********************************************************
*@Function name:LoadBalance_TimerFunc
*@Description:负载均衡定时器回调函数
*@input param:timerId 定时器ID
*@Return:
********************************************************************************/
void LoadBalance_TimerFunc(TIMER_ID timerId, char *, int);

/********************************************************
*@Function name:ProcessAllStations
*@Description:统一处理所有充电桩的负载均衡（不区分单桩或双桩）
*@input param:stations 充电桩数组
*@input param:stationCount 充电桩数量
*@Return:
********************************************************************************/
void ProcessAllStations(ChargingStation *stations, int stationCount);

/********************************************************
*@Function name:ProcessSingleStation
*@Description:处理单个充电桩的负载均衡（已废弃，保留用于兼容）
*@input param:station 充电桩对象
*@Return:
********************************************************************************/
void ProcessSingleStation(ChargingStation *station);

/********************************************************
*@Function name:ProcessTwoStations
*@Description:处理两个充电桩的负载均衡（已废弃，保留用于兼容）
*@input param:stations 充电桩数组
*@input param:stationCount 充电桩数量（必须为2）
*@Return:
********************************************************************************/
void ProcessTwoStations(ChargingStation *stations, int stationCount);

/********************************************************
*@Function name:RoundCurrent
*@Description:将0.1A单位的电流值四舍五入到整数安培
*@input param:current 电流值，单位0.1A (如321表示32.1A)
*@Return:四舍五入后的整数安培值 (如32.1A->32, 23.9A->24)
********************************************************************************/
uint16_t RoundCurrent(uint16_t current);

#endif

