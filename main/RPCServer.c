#include "RPCServer.h"
#include "ChargingStationManagerController.h"
#include "cJSON.h"
#include "DeBug.h"
#include <stdio.h>
#include <string.h>
#include "ConfigManagerController.h"
#include "SystemMaintenance.h"

void InitRPCServer()
{

    return;
}
int ProcessRPCRequest(char *request,int requestLen,char *response,int *responseLen)
{
    // 初始化空响应测试
    sprintf(response, "{\"ret\":{\"RPCServer\":\"no_handler\"},\"data\":{}}");
    *responseLen = strlen(response);

    char method[96] = {0};
     // 获取 json 对象
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) 
    {
        dPrint(DERROR, "Failed to parse request JSON\n");
        sprintf(response, "{\"ret\": {\"RPCServer.ProcessRPCRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    // 获取method字段的cJSON对象
    cJSON *methodItem = cJSON_GetObjectItem(requestJson, "method");
    // 检查字段是否存在且类型正确
    if (methodItem != NULL && cJSON_IsString(methodItem)) 
    {
        // 安全地获取字符串值
        strncpy(method,methodItem->valuestring,sizeof(method));
        dPrint(DEBUG,"Method: %s\n", method);
    } 
    else 
    {
        // 处理字段不存在或类型错误的情况
        dPrint(DERROR,"Method字段不存在或不是字符串类型\n");
        cJSON_free(requestJson);
        return RTN_FAIL;
    }
    cJSON_free(requestJson);

    //搜索充电桩
    if(strncmp(SERACH_CHARGING_STATION_METHOD,method,strlen(method)) == 0)
    {
        //返回搜索到的所有的充电桩信息
        SearchChargeStationRequest(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SELECT_ADDED_CHARGING_STATION_METHOD,method,strlen(method)) == 0)
    {
        //返回已添加过的充电桩信息
        SelectSubDeviceByPortName(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(UPDATE_CHARGING_STATION_METHOD,method,strlen(method)) == 0)
    {
        //更新充电桩,修改充电桩配置文件中的添加状态
        UpdateMajorSubDeviceByPortName(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(START_CHARGING_REQUEST_METHOD,method,strlen(method)) == 0)
    {
        //启动充电
        StartChargingRequest(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(STOP_CHARGING_REQUEST_METHOD,method,strlen(method)) == 0)
    {
        //停止充电
        StopChargingRequest(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SELECT_CHARGING_STATION_DETAIL_METHOD,method,strlen(method)) == 0)
    {
        //查询充电桩详细信息
        SelectChargingDetailsByMac(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(COMFIGMANAGER_SETCONFIG,method,strlen(method)) == 0)
    {
        //更新配置
        SetConfig(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(COMFIGMANAGER_GETCONFIG,method,strlen(method)) == 0)
    {
        //查询配置
        GetConfig(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SELECT_CHARGING_LOAD_CURRENT_METHOD,method,strlen(method)) == 0)
    {
        //查询充电桩电流信息
        SelectChargingLoadCurrent(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(GET_SYSTEM_INFO_METHOD,method,strlen(method)) == 0)
    {
        //获取系统信息
        GetSystemInfo(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(GET_WIFI_LIST_METHOD,method,strlen(method)) == 0)
    {
        //获取wifi列表
        SelectWifiList(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SELECT_ALL_CHARGING_LOAD_CURRENT_METHOD,method,strlen(method)) == 0)
    {
        //查询充电桩所有信息
        SelectChargingLoadCurrentList(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(GET_CHARGING_STATIONS_INFO_METHOD,method,strlen(method)) == 0)
    {
        //获取当前充电桩信息
        GetChargingStationsInfoRequest(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SELECT_CHARGING_HISTORY_METHOD,method,strlen(method)) == 0)
    {
        //查询充电历史记录
        SelectChargingHistory(method,request,requestLen,response,responseLen);
    }
    else if(strncmp(SET_CHARGING_WORK_MODE_METHOD, method, strlen(method)) == 0)
    {
        //设置充电桩工作模式
        SetChargingWorkMode(method,request,requestLen,response,responseLen);
    }
    else
    {
        dPrint(DERROR,"not find method:%s\n",method);
    }
    return RTN_SUCCESS;
}