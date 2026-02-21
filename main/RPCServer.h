#ifndef __RPCSERVER_H__
#define __RPCSERVER_H__
#include "types.h"
/********************************************************************************
* @File name:RPCServer.h
* @Author:shen
* @ModifyDate:2025-11-08
* @ModifyAuthor:shen
* @ModifyContent:统一的对外入口
* @Description:业务转发文件，蓝牙/wifi调用内部的函数，统一通过此文件转发
********************************************************************************/

//搜索充电桩方法定义
#define SERACH_CHARGING_STATION_METHOD                    "SubDeviceManager.SearchChargeStationRequest"
//查询已经添加过的充电桩
#define SELECT_ADDED_CHARGING_STATION_METHOD  			  "SubDeviceManager.SelectSubDeviceByPortName"
//添加充电桩方法调用定义
#define UPDATE_CHARGING_STATION_METHOD                     "SubDeviceManager.UpdateMajorSubDeviceByPortName"
//启动充电
#define START_CHARGING_REQUEST_METHOD						"SubDeviceManager.StartChargingRequest"
//停止充电
#define STOP_CHARGING_REQUEST_METHOD						"SubDeviceManager.StopChargingRequest"
//查询充电桩详细信息的方法定义
#define SELECT_CHARGING_STATION_DETAIL_METHOD			   "SubDeviceManager.SelectChargingDetails"
//更新配置项方法
#define	COMFIGMANAGER_SETCONFIG							  "ConfigManager.SetConfig"
//查询配置项方法
#define	COMFIGMANAGER_GETCONFIG							  "ConfigManager.GetConfig"
//查询充电桩电流信息
#define SELECT_CHARGING_LOAD_CURRENT_METHOD               "SubDeviceManager.SelectChargingLoadCurrent"
//获取系统信息
#define GET_SYSTEM_INFO_METHOD  							"SystemTabService.GetSystemInfo"
//获取wifi列表
#define GET_WIFI_LIST_METHOD								"WifiManager.SelectWifiList"
//获取所有充电桩电流信息
#define SELECT_ALL_CHARGING_LOAD_CURRENT_METHOD               "SubDeviceManager.SelectChargingLoadCurrentList"
//获取当前充电桩详细设备信息
#define GET_CHARGING_STATIONS_INFO_METHOD                     "SubDeviceManager.GetChargingStationsInfo"
//查询充电历史记录
#define SELECT_CHARGING_HISTORY_METHOD                        "SubDeviceManager.SelectChargingHistory"
//设置充电桩工作模式
#define SET_CHARGING_WORK_MODE_METHOD							"SubDeviceManager.SetChargingStationWorkMode"

/********************************************************
*@Function name:InitRPCServer
*@Description:RPCServer初始化
*@Return:
********************************************************************************/
void InitRPCServer();

/********************************************************
	*@Function name:processRequest
	*@Description:统一处理wifi/蓝牙消息
	*@input param:method 方法
	*@input param:request 请求内容
    *@input param:requestLen 请求数据长度
    *@output param:response 要回复的数据
    *@output param:responseLen 要回复的数据长度
	*@Return:true –成功,false—失败
********************************************************************************/
int ProcessRPCRequest(char *request,int requestLen,char *response,int *responseLen);

#endif
