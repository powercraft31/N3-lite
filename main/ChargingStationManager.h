#ifndef __CHARGE_STATION_MANAGER_H__
#define __CHARGE_STATION_MANAGER_H__
#include <stdio.h>
#include "ChargingStation.h"
#include "CEvent.h"
#include "CTimer.h"
#include "HwTimer.h"
/********************************************************
*@Function name:InitChargeStationManager
*@Description:充电桩管理初始化,外部调用
*@Return:
********************************************************************************/
extern void InitChargeStationManager();
/********************************************************
*@Function name:
*@Description:初始化充电桩信息默认值
*@input param:station 充电桩对象数组
*@Return:
********************************************************************************/
int InitChargingStationInfo(EnumConnectStatus connecStatus);

//获取搜索到的充电桩
SearchStation* GetSearchStation(char *nvMac);
/********************************************************
*@Function name:GetChargingStation
*@Description:根据mac地址找到对应的充电桩
*@input param:nvMac mac地址
*@Return:找到了返回对象地址，找不到返回NULL
********************************************************************************/
ChargingStation* GetChargingStation(char *nvMac);

//根据mac地址获取充电桩的索引号
int GetStationIndexByMac(char *nvMac);

//触发立马搜索一次附近充电桩
int QuickLySearchStation();
/********************************************************
*@Function name:CheckStationThread
*@Description:检测充电桩在离线线程
*@Return:
********************************************************************************/
void CheckStationThread(void *pvParameter);
//暂停充电桩查询线程
void SuspendStationTaskThread();
//恢复充电桩查询线程
void ResumeStationTaskThread();
//获取所有搜索到的充电桩
extern SearchStation * SelectAllSearchStation(int *stationCount);
/********************************************************
*@Function name:
*@Description:获取全部的充电桩信息,外部调用,
*@output param:stationArray 充电桩数组，内部回提供内存,不需要外部提供内存
*@Return:有几个充电桩,-1失败
********************************************************************************/
extern ChargingStation * SelectAllChargeStation(int *stationCount);


/********************************************************
*@Function name:HandleChargeStationEvent
*@Description:处理充电桩状态变化事件
*@input param:station 充电桩对象
*@input param:event   事件类型
*@Return:
********************************************************************************/
void HandleChargeStationEvent(EVENT_TYPE eventType,char *args,int argLen);

/********************************************************
*@Function name:GetChargeStationRealDataTimerFunc
*@Description:查询充电桩实时数据定时器
*@input param:timerId 定时器ID
*@input param:arg   参数
*@input param:argLen 参数长度
*@Return:
********************************************************************************/
void GetChargeStationRealDataTimerFunc(TIMER_ID timerId, char *arg, int argLen);

/********************************************************
*@Function name:UpdateAllSearchStationByConfig
*@Description:更新搜索列表的已添加状态
*@Return:
********************************************************************************/
void UpdateAllSearchStationByConfig();

/********************************************************
*@Function name:StartCharging
*@Description:启动充电
********************************************************************************/
int StartCharging(char *mac,unsigned long long startTime);
/********************************************************
*@Function name:StopCharging
*@Description:停止充电
********************************************************************************/
int StopCharging(char *mac,unsigned long long endTime);

//充电时长定时器回调
void ChargingDurationHwTimer(HW_TIMER_ID timerId, void *arg, int argLen);

/********************************************************
	*@Function name:GetStationCmdByName
	*@Description:根据命令名称获取控制充电桩的指令
	*@input param:cmdIdx 命令ID
	*@output param:
	*@Return:AT指令
********************************************************************************/
//char* GetStationCmdByName(const char* cmdIdx);

/********************************************************
*@Function name:CheckStationConnectTimerFunc
*@Description:查询充电桩实时数据定时器
*@input param:timerId 定时器ID
*@input param:arg   参数
*@input param:argLen 参数长度
*@Return:
********************************************************************************/
void CheckStationConnectTimerFunc(TIMER_ID timerId, char *arg, int argLen);
//打印充电桩的所有数据
void PrintChargingStationData();

//设置充电桩工作模式
void chargingWorkMode(char *mac, char *workmode);

#endif

