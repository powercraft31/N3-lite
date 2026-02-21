#ifndef __CHARGE_STATION_MANAGER_CONTROLLER_H__
#define __CHARGE_STATION_MANAGER_CONTROLLER_H__
#include "types.h"
#include <string.h>
#include "cJSON.h"

/********************************************************
	*@Function name:SearchChargeStationRequest
	*@Description:蓝牙或wifi搜索充电桩
	*@input param:method 方法
	*@input param:request 请求内容
    *@input param:requestLen 请求数据长度
    *@output param:response 要回复的数据
    *@output param:responseLen 要回复的数据长度
	*@Return:RTN_SUCCESS –成功,RTN_FAIL—失败
********************************************************************************/
int SearchChargeStationRequest(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:SelectSubDeviceByPortName
	*@Description:查询已经添加过的充电桩
********************************************************************************/
int SelectSubDeviceByPortName(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:UpdateMajorSubDeviceByPortName
    *@Description:更新所有充电桩
********************************************************************************/
int UpdateMajorSubDeviceByPortName(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:StartChargingRequest
	*@Description:启动充电请求
********************************************************************************/
int StartChargingRequest(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:StopChargingRequest
	*@Description:停止充电请求
********************************************************************************/
int StopChargingRequest(char *method,char *request,int requestLen,char *response,int *responseLen);
/********************************************************
	*@Function name:SelectChargingDetailsByMac
	*@Description:根据mac地址查询充电详情(时长/状态/能量/充电桩状态)
********************************************************************************/
int SelectChargingDetailsByMac(char *method,char *request,int requestLen,char *response,int *responseLen);


/********************************************************
	*@Function name:SelectChargingLoadCurrent
	*@Description:查询充电桩电流信息
********************************************************************************/
int SelectChargingLoadCurrent(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:SelectWifiList
	*@Description:查询wifi列表
****************************************************************/
int SelectWifiList(char *method,char *request,int requestLen,char *response,int *responseLen);
/********************************************************
	*@Function name:SelectChargingLoadCurrentList
	*@Description:查询充电桩电流所有信息
********************************************************************************/
int SelectChargingLoadCurrentList(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:GetChargingStationsInfoRequest
	*@Description:获取当前充电桩信息
********************************************************************************/
int GetChargingStationsInfoRequest(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:SelectChargingHistory
	*@Description:查询充电历史记录（分页，每页最多3条）
********************************************************************************/
int SelectChargingHistory(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:SetChargingWorkMode
	*@Description:设置充电桩工作模式
********************************************************************************/
int SetChargingWorkMode(char *method,char *request,int requestLen,char *response,int *responseLen);

#endif
