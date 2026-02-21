#include "ChargingStationManagerController.h"
#include "DeBug.h"
#include "ChargingStationManager.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ConfigManager.h"
#include "StringUtils.h"
#include "GPIOManager.h"
#include "BL0942Meter.h"
#include "WifiManager.h"
#include "BLEManager.h"
#include "OrderStorage.h"
#include <sys/time.h>
#include <time.h>

int SearchChargeStationRequest(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    //先触发搜搜一次
    QuickLySearchStation();

    // 分段延时，每次延时短于看门狗超时时间（5秒）
    // 总共延时15秒，分成30次，每次500ms，并主动让出CPU
    for(int i = 0; i < 30; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
    }
    memset(response,0,strlen(response));
    //返回搜索到的充电桩信息
    // 获取所有充电桩信息
    int stationCount = 0;
    SearchStation *stations = SelectAllSearchStation(&stationCount);
    // 创建根JSON对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        dPrint(DERROR, "Failed to create root JSON object\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SearchChargeStationRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 添加 ret 对象
    cJSON *ret = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ret", ret);
    cJSON_AddStringToObject(ret, "SubDeviceManager.SearchChargeStationRequest", "success");

    // 添加 deviceId
    cJSON_AddStringToObject(root, "deviceId", "N3Lite");

    // 添加 code
    cJSON_AddNumberToObject(root, "code", 200);

    // 创建 data 对象
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);

    // 创建 deviceList 数组
    cJSON *deviceList = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "deviceList", deviceList);

    // 遍历所有充电桩，添加到 deviceList
    for (int i = 0; i < stationCount; i++) {
        SearchStation *station = &stations[i];

        // 创建充电桩设备对象
        cJSON *device = cJSON_CreateObject();

        // 添加各个字段
        //cJSON_AddStringToObject(device, "Cst_BackendUrl", "");
        //cJSON_AddBoolToObject(device, "bindStatus", station->isAdded);

        // clientId
        //char clientId[32] = {0};
        //snprintf(clientId, sizeof(clientId), "ocppclient_%d", i + 1);
        //cJSON_AddStringToObject(device, "clientId", clientId);

        // connectStatus
        //const char *connectStatus = (station->connectStatus == SUB_DEVICE_CONNECT_STATUS_ONLINE) ? "online" : "offline";
        cJSON_AddStringToObject(device, "connectStatus", "online");

        // deviceExisted
        cJSON_AddBoolToObject(device, "deviceExisted", station->isAdded);

        // snr 和 atten
        cJSON_AddNumberToObject(device, "snr", station->snr);
        cJSON_AddNumberToObject(device, "atten", station->atten);

        // mdcFwVersion
        cJSON_AddStringToObject(device, "mdcFwVersion", station->mdcFwVersion);

        // deviceBrand
        //cJSON_AddStringToObject(device, "deviceBrand", "ATPIII");

        // deviceSn
        //cJSON_AddStringToObject(device, "deviceSn", station->serialNum);

        // mac
        cJSON_AddStringToObject(device, "mac", station->mac);

        // modelId
        //cJSON_AddStringToObject(device, "modelId", "ATPIII");

        // name
        //cJSON_AddStringToObject(device, "name", station->name);

        // portName
        //cJSON_AddStringToObject(device, "portName", "plc");

        // productId
        //cJSON_AddStringToObject(device, "productId", "ev");

        // productType
        //cJSON_AddStringToObject(device, "productType", "ev");

        // subDevId
        cJSON_AddStringToObject(device, "subDevId", station->serialNum);

        // vendor
        //cJSON_AddStringToObject(device, "vendor", "VoltronicPower");

        // 添加设备到 deviceList 数组
        cJSON_AddItemToArray(deviceList, device);
    }

    // 生成紧凑格式的JSON字符串（无空格、制表符、换行符）
    char *jsonString = cJSON_PrintUnformatted(root);
    if (jsonString == NULL) {
        dPrint(DERROR, "Failed to print JSON\n");
        cJSON_Delete(root);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SearchChargeStationRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 复制到response（外部已提供内存）
    int jsonLen = strlen(jsonString);
    if(jsonLen < 512)
    {
        strncpy(response, jsonString, jsonLen);
        response[jsonLen] = '\0';
        *responseLen = jsonLen;
    }
    else
    {
       dPrint(DERROR,"jsonLen > 512 not allow response\n");
       cJSON_free(jsonString);
       cJSON_Delete(root);
       strcpy(response,"{\"ret\": {\"SubDeviceManager.SearchChargeStationRequest\":\"failed\"},\"data\":{}}");
       *responseLen = strlen(response);
       return RTN_FAIL;
    }
    // 释放资源
    cJSON_free(jsonString);
    cJSON_Delete(root);

    dPrint(INFO, "SearchChargeStationRequest: Found %d charging stations\n", stationCount);

    return RTN_SUCCESS;
}
//查询已经添加过的充电桩
int SelectSubDeviceByPortName(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    //读取到本地配置了之后，就读取配置中保存的结构:
    /*[
        {
            "mac":"",
            "subDevId":"",
            "name":"",
            "maxlimitCurrent":""
        }
    ]
    */
    memset(response,0,strlen(response));
    //直接从配置文件读取
    char buffer[512] = {0};

    ConfigStatus_t status = GetEVConfigJSON(buffer,sizeof(buffer));
    if(status != CONFIG_OK)
    {
        dPrint(DERROR,"GetEVConfigJSON failed \n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectSubDeviceByPortName\":\"success\"},\"data\":{\"deviceList\":[]}}");
        *responseLen = strlen(response);
        return RTN_SUCCESS;
    }
    
    // 解析配置文件中的 JSON 数组
    cJSON *configArray = cJSON_Parse(buffer);
    if (configArray == NULL || !cJSON_IsArray(configArray)) {
        dPrint(DERROR, "Failed to parse config JSON or not an array\n");
        if (configArray != NULL) {
            cJSON_Delete(configArray);
        }
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectSubDeviceByPortName\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 创建响应根JSON对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        dPrint(DERROR, "Failed to create root JSON object\n");
        cJSON_Delete(configArray);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectSubDeviceByPortName\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 添加 ret 对象
    cJSON *ret = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ret", ret);
    cJSON_AddStringToObject(ret, "SubDeviceManager.SelectSubDeviceByPortName", "success");

    // 添加 deviceId
    cJSON_AddStringToObject(root, "deviceId", "N3Lite");

    // 添加 code
    cJSON_AddNumberToObject(root, "code", 200);

    // 创建 data 对象
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);

    // 创建 deviceList 数组
    cJSON *deviceList = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "deviceList", deviceList);

    // 遍历配置文件中的充电桩
    int arraySize = cJSON_GetArraySize(configArray);
    for (int i = 0; i < arraySize; i++) {
        cJSON *configItem = cJSON_GetArrayItem(configArray, i);
        if (configItem == NULL) {
            continue;
        }

        // 从配置文件中读取字段
        cJSON *macItem = cJSON_GetObjectItem(configItem, "mac");
        cJSON *subDevIdItem = cJSON_GetObjectItem(configItem, "subDevId");
        cJSON *nameItem = cJSON_GetObjectItem(configItem, "name");
        //cJSON *maxlimitCurrentItem = cJSON_GetObjectItem(configItem, "maxlimitCurrent");

        if (macItem == NULL || !cJSON_IsString(macItem)) 
        {
            continue;
        }
        char *mac = macItem->valuestring;
        // 根据 MAC 地址从内存中获取充电桩详细信息
        ChargingStation *station = GetChargingStation(mac);

        // 创建设备对象
        cJSON *device = cJSON_CreateObject();

        // deviceSn 和 subDevId
        if (subDevIdItem != NULL && cJSON_IsString(subDevIdItem)) {
            //cJSON_AddStringToObject(device, "deviceSn", subDevIdItem->valuestring);
            cJSON_AddStringToObject(device, "subDevId", subDevIdItem->valuestring);
        } else if (station != NULL) {
            //cJSON_AddStringToObject(device, "deviceSn", station->serialNum);
            cJSON_AddStringToObject(device, "subDevId", station->serialNum);
        } else {
            //cJSON_AddStringToObject(device, "deviceSn", "");
            cJSON_AddStringToObject(device, "subDevId", "");
        }
        // deviceBrand
        //cJSON_AddStringToObject(device, "deviceBrand", "ATPIII");

        // productId
        //cJSON_AddStringToObject(device, "productId", "ev");

        // productType
        //cJSON_AddStringToObject(device, "productType", "ev");
        // name
        if (nameItem != NULL && cJSON_IsString(nameItem)) {
            cJSON_AddStringToObject(device, "name", nameItem->valuestring);
        } else {
            cJSON_AddStringToObject(device, "name", "充电桩");
        }
        // modelId
        //cJSON_AddStringToObject(device, "modelId", "ATPIII");
        // portName
        //cJSON_AddStringToObject(device, "portName", "plc");
        // bindStatus - 从配置文件读取到的都是已添加的
        //cJSON_AddBoolToObject(device, "bindStatus", TRUE);
        // mdcFwVersion - 从内存中的充电桩获取
        if (station != NULL) {
            cJSON_AddStringToObject(device, "mdcFwVersion", station->mdcFwVersion);
        } else {
            cJSON_AddStringToObject(device, "mdcFwVersion", "");
        }
        // connectStatus - 从内存中的充电桩获取
        if (station != NULL) {
            const char *connectStatus = (station->connectStatus == SUB_DEVICE_CONNECT_STATUS_ONLINE) ? "online" : "offline";
            cJSON_AddStringToObject(device, "connectStatus", connectStatus);
        } else {
            cJSON_AddStringToObject(device, "connectStatus", "offline");
        }
        // mac
        cJSON_AddStringToObject(device, "mac", mac);
        // chargingStatus - 从内存中的充电桩获取
        if (station != NULL) {
            //const char *chargingStatus = (station->enumStatus == Charging) ? "1" : "0";
            cJSON_AddStringToObject(device, "EVStatus", station->EVStatus);
        } else {
            cJSON_AddStringToObject(device, "EVStatus", "unkown");
        }
        // vendor
        //cJSON_AddStringToObject(device, "vendor", "VoltronicPower");
        // phase - 从内存中的充电桩获取
        // 添加设备到 deviceList 数组
        //workmode -- 从内存中的充电桩获取
        if (station != NULL) {
            //const char *chargingStatus = (station->enumStatus == Charging) ? "1" : "0";
            if (station->workMode == App) {
                cJSON_AddStringToObject(device, "workMode", "App");
            } else if (station->workMode == Plc) {
                cJSON_AddStringToObject(device, "workMode", "Plc");
            } else if (station->workMode == Ocpp) {
                cJSON_AddStringToObject(device, "workMode", "Ocpp");
            }
        } else {
            cJSON_AddStringToObject(device, "workMode", "unkown");
        }

        cJSON_AddItemToArray(deviceList, device);
    }
    // 释放配置 JSON
    cJSON_Delete(configArray);
    // 生成紧凑格式的JSON字符串
    char *jsonString = cJSON_PrintUnformatted(root);
    if (jsonString == NULL) {
        dPrint(DERROR, "Failed to print JSON\n");
        cJSON_Delete(root);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectSubDeviceByPortName\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    // 复制到response（外部已提供内存）
    int jsonLen = strlen(jsonString);
    if (jsonLen < 512) {
        strncpy(response, jsonString, jsonLen);
        response[jsonLen] = '\0';
        *responseLen = jsonLen;
    } else {
        dPrint(DERROR, "jsonLen:%d > 512 not allow response\n", jsonLen);
        cJSON_free(jsonString);
        cJSON_Delete(root);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectSubDeviceByPortName\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    // 释放资源
    cJSON_free(jsonString);
    cJSON_Delete(root);
    dPrint(INFO, "SelectSubDeviceByPortName: Found %d devices in config\n", arraySize);
    return RTN_SUCCESS;
}
//更新所有充电桩
int UpdateMajorSubDeviceByPortName(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    
    memset(response,0,strlen(response));
    strcpy(response, "{\"ret\": {\"SubDeviceManager.UpdateMajorSubDeviceByPortName\":\"failed\"},\"data\":{}}");
    *responseLen = strlen(response);
    //更新本地配置文件保存的充电桩信息，如果内存中存在，也一并更新
    /*
    请求格式：
    {
        "method":"SubDeviceManager.UpdateMajorSubDeviceByPortName",
        "deviceId":"E9w0vbEo-1736210258",
        "data": {
                "portName": "plc",
                "deviceList":[
                {
                    "subDevId": "AC000000000001",
                    "name":"充电桩",
                    "mac":"4080E1346368",
                }
              ]
        }
    }
    */
    dPrint(DEBUG,"request:%s\n",request);
    // 解析请求 JSON
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse request JSON\n");
        return RTN_FAIL;
    }

    // 获取 data 对象
    cJSON *dataObj = cJSON_GetObjectItem(requestJson, "data");
    if (dataObj == NULL || !cJSON_IsObject(dataObj)) {
        dPrint(DERROR, "Failed to get data object\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    // 获取 deviceList 数组
    cJSON *deviceListArray = cJSON_GetObjectItem(dataObj, "deviceList");
    if (deviceListArray == NULL || !cJSON_IsArray(deviceListArray)) {
        dPrint(DERROR, "Failed to get deviceList array\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    // 创建配置 JSON 数组
    cJSON *configArray = cJSON_CreateArray();
    if (configArray == NULL) {
        dPrint(DERROR, "Failed to create config array\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }
    
    // 遍历 deviceList，构建配置数组
    int arraySize = cJSON_GetArraySize(deviceListArray);
    if(arraySize > CHAEGING_STATION_MAX_NUM)
    {
        dPrint(DERROR, "arraySize > CHAEGING_STATION_MAX_NUM\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }
    for (int i = 0; i < arraySize; i++) {
        char macString[13] = {0};
        cJSON *deviceItem = cJSON_GetArrayItem(deviceListArray, i);
        if (deviceItem == NULL) {
            continue;
        }

        // 获取各个字段
        cJSON *subDevIdItem = cJSON_GetObjectItem(deviceItem, "subDevId");
        cJSON *nameItem = cJSON_GetObjectItem(deviceItem, "name");
        cJSON *macItem = cJSON_GetObjectItem(deviceItem, "mac");
        //版本
        cJSON *mdcFwVersion = cJSON_GetObjectItem(deviceItem, "mdcFwVersion");
        // 创建配置项
        cJSON *configItem = cJSON_CreateObject();
        if (configItem == NULL) {
            continue;
        }

        // 添加字段到配置项
        if (subDevIdItem != NULL && cJSON_IsString(subDevIdItem)) {
            cJSON_AddStringToObject(configItem, "subDevId", subDevIdItem->valuestring);
        } else {
            cJSON_AddStringToObject(configItem, "subDevId", "");
        }

        if (nameItem != NULL && cJSON_IsString(nameItem)) {
            cJSON_AddStringToObject(configItem, "name", nameItem->valuestring);
        } else {
            cJSON_AddStringToObject(configItem, "name", "充电桩");
        }

        if (macItem != NULL && cJSON_IsString(macItem)) {
            cJSON_AddStringToObject(configItem, "mac", macItem->valuestring);
            strncpy(macString, macItem->valuestring,sizeof(macString));
        } else {
            cJSON_AddStringToObject(configItem, "mac", "");
         
        }
        //版本
        if (mdcFwVersion != NULL && cJSON_IsString(mdcFwVersion)) {
            cJSON_AddStringToObject(configItem, "mdcFwVersion", mdcFwVersion->valuestring);
        } else {
            cJSON_AddStringToObject(configItem, "mdcFwVersion", "");
        }
        // 添加到配置数组
        cJSON_AddItemToArray(configArray, configItem);
    }
    // 生成配置 JSON 字符串
    char *configJsonString = cJSON_PrintUnformatted(configArray);
    if (configJsonString == NULL) {
        dPrint(DERROR, "Failed to print config JSON\n");
        cJSON_Delete(configArray);
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }
    // 保存配置到 Flash
    ConfigStatus_t status = SetEVConfigJSON(configJsonString);
    cJSON_free(configJsonString);
    cJSON_Delete(configArray);
    cJSON_Delete(requestJson);

    if (status != CONFIG_OK) {
        dPrint(DERROR, "Failed to save config to Flash\n");
        return RTN_FAIL;
    }

    // 构建响应 JSON
    cJSON *responseRoot = cJSON_CreateObject();
    if (responseRoot == NULL) {
        dPrint(DERROR, "Failed to create response JSON\n");
        return RTN_FAIL;
    }

    // 添加 ret 对象
    cJSON *ret = cJSON_CreateObject();
    cJSON_AddItemToObject(responseRoot, "ret", ret);
    cJSON_AddStringToObject(ret, "SubDeviceManager.UpdateMajorSubDeviceByPortName", "success");

    // 添加 deviceId
    cJSON_AddStringToObject(responseRoot, "deviceId", "N3Lite");

    // 添加空的 data 对象
    cJSON *responseData = cJSON_CreateObject();
    cJSON_AddItemToObject(responseRoot, "data", responseData);

    // 生成响应 JSON 字符串
    char *responseJsonString = cJSON_PrintUnformatted(responseRoot);
    if (responseJsonString == NULL) 
    {
        dPrint(DERROR, "Failed to print response JSON\n");
        cJSON_Delete(responseRoot);
        return RTN_FAIL;
    }
    // 复制到 response
    int jsonLen = strlen(responseJsonString);
    if (jsonLen < 512) {
        memset(response,0,strlen(response));
        strncpy(response, responseJsonString, jsonLen);
        response[jsonLen] = '\0';
        *responseLen = jsonLen;
    } else {
        dPrint(DERROR, "Response JSON too long: %d bytes\n", jsonLen);
        cJSON_free(responseJsonString);
        cJSON_Delete(responseRoot);
        return RTN_FAIL;
    }
    // 释放资源
    cJSON_free(responseJsonString);
    cJSON_Delete(responseRoot);

    dPrint(INFO, "UpdateMajorSubDeviceByPortName: Updated %d devices\n", arraySize);

    //需要重新构建已添加充电桩
    InitChargingStationInfo(SUB_DEVICE_CONNECT_STATUS_ONLINE);
    //这个用不上，因为搜索的时候，会调用
    //立马更新搜索列表里面的已添加和未添加状态
    //UpdateAllSearchStationByConfig();

    return RTN_SUCCESS;
}

int StartChargingRequest(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    memset(response,0,strlen(response));
    char mac[13] = {0};
    dPrint(DEBUG,"收到手机APP发送的开始充电StartCharging 请求request:%s\n",request);
    //解析出mac地址
    // 解析请求 JSON
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse request JSON\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StartChargingRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取 data 对象
    cJSON *dataObj = cJSON_GetObjectItem(requestJson, "data");
    if (dataObj == NULL || !cJSON_IsObject(dataObj)) {
        dPrint(DERROR, "Failed to get data object\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StartChargingRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    cJSON *deviceInfo = cJSON_GetObjectItem(dataObj, "deviceInfo");
    cJSON *macItem = cJSON_GetObjectItem(deviceInfo, "mac");
    cJSON *startTimeItem = cJSON_GetObjectItem(deviceInfo, "startTime");

    strncpy(mac,macItem->valuestring,sizeof(mac));
    unsigned long long startTime = (unsigned long long)startTimeItem->valuedouble;
    cJSON_Delete(requestJson);

    int iRet = StartCharging(mac,startTime);
    if(iRet == RTN_SUCCESS)
    {
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StartChargingRequest\":\"success\"},\"data\":{}}");
    }
    else
    {
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StartChargingRequest\":\"failed\"},\"data\":{}}");
    }
    *responseLen = strlen(response);
    return RTN_SUCCESS;
}
int StopChargingRequest(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    memset(response,0,strlen(response));
    char mac[13] = {0};
    int iRet = 0;
    dPrint(DEBUG,"收到手机APP发送的停止充电StopCharging请求 request:%s\n",request);
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse request JSON\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StopChargingRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取 data 对象
    cJSON *dataObj = cJSON_GetObjectItem(requestJson, "data");
    if (dataObj == NULL || !cJSON_IsObject(dataObj)) {
        dPrint(DERROR, "Failed to get data object\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StopChargingRequest\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    cJSON *deviceInfo = cJSON_GetObjectItem(dataObj, "deviceInfo");
    cJSON *macItem = cJSON_GetObjectItem(deviceInfo, "mac");
    cJSON *endTimeItem = cJSON_GetObjectItem(deviceInfo, "endTime");

    strncpy(mac,macItem->valuestring,sizeof(mac));
    unsigned long long endTime = (unsigned long long)endTimeItem->valuedouble;
    cJSON_Delete(requestJson);

    iRet = StopCharging(mac,endTime);
    if(iRet == RTN_SUCCESS)
    {
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StopChargingRequest\":\"success\"},\"data\":{}}");
    }
    else
    {
        strcpy(response,"{\"ret\": {\"SubDeviceManager.StopChargingRequest\":\"failed\"},\"data\":{}}");
    }
    *responseLen = strlen(response);
    return RTN_SUCCESS;
}
int SelectChargingDetailsByMac(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    //根据mac地址获取到充电桩对象
    char subDevId[64] = {0};
    //char name[64] = {0};
    char mac[13] = {0};
    //获取出设备ID
    //extract_string(request,"subDevId",subDevId,sizeof(subDevId));
    //获取出名称
    //extract_string(request,"name",name,sizeof(name));
    //获取出mac地址
    extract_string(request,"mac",mac,sizeof(mac));
	
    
    dPrint(DEBUG,"接收到手机APP查询充电桩详情data:%s,mac:%s,subDevId:%s\n",request,mac,subDevId);
    ChargingStation* station = GetChargingStation(mac);
    if(station == NULL)
    {
        dPrint(DERROR,"station == NULL");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingDetailsByMac\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
	//根据mac地址获取subDevId
	strncpy(subDevId,station->serialNum,sizeof(subDevId));

    // 计算实时 duration
    int realTimeDuration = station->duration;
    if (station->isStartTimeCalibrated && station->startTime > 0 &&
        (station->enumStatus == Charging || station->enumStatus == SuspendEvse)) {
        // 时间已校准且正在充电中：获取当前时间戳计算实时 duration
        struct timeval tv;
        gettimeofday(&tv, NULL);
        unsigned long long currentTime = (unsigned long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
        realTimeDuration = (int)((currentTime - station->startTime) / 1000);
    }
    // 否则直接使用 station->duration（未校准时由硬件定时器累加，或充电已结束）

    //构建json
    memset(response,0,strlen(response));
    sprintf(response,"%s%s%s%s%s%s%s%llu%s%llu%s%s%s%f%s%d%s%d%s%d%s",
            "{\"ret\":{\"SubDeviceManager.SelectChargingDetails\":\"success\"},\"data\":{\"subDevId\":\"",subDevId,
            "\",\"name\":\"",station->name,
            "\",\"mac\":\"",mac,
            "\",\"startTime\":\"",station->startTime,
            "\",\"endTime\":\"",station->endTime,
            "\",\"EVStatus\":\"",station->EVStatus,
            "\",\"energy\":\"",(station->energy - station->lastEnergy)/1000.0,
            "\",\"duration\":\"",realTimeDuration,
            "\",\"isPlugged\":\"",station->isPlugged,
            "\",\"chargingMethod\":\"",station->chargingMethod,
            "\"}}");
    *responseLen = strlen(response);
	dPrint(DEBUG,"mac:%s,subDevId:%s,startTime:%llu,endTime:%llu,EVStatus:%s,lastEnergy:%f,energy:%f,duration:%d,isPlugged:%d,chargingMethod:%d\n",mac,subDevId,station->startTime,station->endTime,station->EVStatus,station->lastEnergy,station->energy,realTimeDuration,station->isPlugged,station->chargingMethod);
    return RTN_SUCCESS;
}



int SelectChargingLoadCurrent(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    dPrint(DEBUG,"收到查询充电桩电流信息请求 request:%s\n",request);

    // 解析请求JSON
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse SelectChargingLoadCurrent request JSON\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取data对象
    cJSON *dataItem = cJSON_GetObjectItem(requestJson, "data");
    if (dataItem == NULL || !cJSON_IsObject(dataItem)) {
        dPrint(DERROR, "data字段不存在或不是对象类型\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取deviceInfo对象
    cJSON *deviceInfo = cJSON_GetObjectItem(dataItem, "deviceInfo");
    if (deviceInfo == NULL || !cJSON_IsObject(deviceInfo)) {
        dPrint(DERROR, "deviceInfo字段不存在或不是对象类型\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取mac地址
    cJSON *macItem = cJSON_GetObjectItem(deviceInfo, "mac");
    if (macItem == NULL || !cJSON_IsString(macItem)) {
        dPrint(DERROR, "mac字段不存在或不是字符串类型\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取subDevId
    /*
    cJSON *subDevIdItem = cJSON_GetObjectItem(deviceInfo, "subDevId");
    if (subDevIdItem == NULL || !cJSON_IsString(subDevIdItem)) {
        dPrint(DERROR, "subDevId字段不存在或不是字符串类型\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    */
    char *mac = macItem->valuestring;
    //char *subDevId = subDevIdItem->valuestring;
    dPrint(DEBUG, "查询充电桩电流 mac:%s\n", mac);

    // 根据mac地址获取指定充电桩信息
    ChargingStation *station = GetChargingStation(mac);
    if (station == NULL) {
        dPrint(DERROR, "未找到充电桩 mac:%s\n", mac);
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrent\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取所有充电桩，计算正在充电的充电桩总电流
    int stationCount = 0;
    ChargingStation *allStations = SelectAllChargeStation(&stationCount);

    float totalChargingCurrent = 0;  // 所有正在充电的充电桩电流之和
    for (int i = 0; i < stationCount; i++) {
        ChargingStation *s = &allStations[i];
        // 只统计已绑定且正在充电的充电桩
        if (s->isAdded == TRUE && strcmp(s->EVStatus, "Charging") == 0) {
            totalChargingCurrent += s->acCurrentL1 / 100.0;
            dPrint(DEBUG, "充电桩 %s 正在充电，电流: %u A\n", s->mac, s->acCurrentL1);
        }
    }

    // 获取总电表电流
    float meterCurrent = bl0942_get_current() / 10.0;

    // 计算负载电流（非充电负载）= 总电表电流 - 充电桩总电流
    float loadCurrent = (meterCurrent > totalChargingCurrent) ?
                           (meterCurrent - totalChargingCurrent) : 0;

    dPrint(INFO, "电流统计: 总电表=%u A, 充电桩总计=%u A, 其他负载=%u A\n",
           meterCurrent, totalChargingCurrent, loadCurrent);

    // 构造响应JSON
    sprintf(response,"%s%s%s%s%s%s%s%f%s%.1f%s%.1f%s%.1f%s",
            "{\"ret\":{\"SubDeviceManager.SelectChargingLoadCurrent\":\"success\"},\"data\":{\"deviceInfo\":{\"subDevId\":\"",station->serialNum,
            "\",\"name\":\"",station->name,
            "\",\"mac\":\"",mac,
            "\",\"energy\":\"",(station->energy- station->lastEnergy) / 1000.0,
            "\",\"ChargingCurrent\":\"",(float)station->acCurrentL1 / 100.0,
            "\",\"LoadCurrent\":\"",loadCurrent,
            "\",\"MeterCurrent\":\"",meterCurrent,
            "\"}}}");
    *responseLen = strlen(response);

    dPrint(INFO, "返回充电桩电流信息 ChargingCurrent:%u, LoadCurrent:%u, MeterCurrent:%u,energy:%f,lastEnergy:%f,station->energy- station->lastEnergy:%f\n",
        station->acCurrentL1, loadCurrent, meterCurrent,station->energy,station->lastEnergy,station->energy- station->lastEnergy);

    cJSON_Delete(requestJson);
    return RTN_SUCCESS;
}

int SelectWifiList(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    dPrint(DEBUG, "收到查询WiFi列表请求 request:%s\n", request);
    memset(response, 0, strlen(response));

    int totalPages = 0;

    // 循环获取并发送每一页WiFi列表
    for (int pageIndex = 0; ; pageIndex++) {
        char wifiListBuffer[380] = {0};  // 容纳3个WiFi的JSON数组，为响应头尾预留空间

        // 获取当前页的WiFi列表（pageIndex=0时会扫描，后续使用扫描结果）
        int ret = WifiManager_SelectWifiList(wifiListBuffer, sizeof(wifiListBuffer), pageIndex, &totalPages);

        // 第一页失败说明扫描失败
        if (pageIndex == 0 && ret != 0) {
            dPrint(DERROR, "WiFi扫描失败\n");
            strcpy(response, "{\"ret\":{\"WifiManager.SelectWifiList\":\"failed\"},\"data\":{\"error\":\"Scan failed\"}}");
            *responseLen = strlen(response);
            return RTN_FAIL;
        }

        // 超出页数范围，结束循环
        if (pageIndex >= totalPages) {
            break;
        }

        // 构造通知JSON（使用snprintf防止溢出）
        char notifyResponse[512] = {0};
        int written = snprintf(notifyResponse, sizeof(notifyResponse),
                "{\"ret\":{\"WifiManager.SelectWifiList\":\"success\"},\"data\":{\"wifiList\":%s,\"totalPages\":%d,\"pageIndex\":%d}}",
                wifiListBuffer, totalPages, pageIndex);

        if (written >= sizeof(notifyResponse)) {
            dPrint(DERROR, "第 %d 页WiFi列表过长: %d (被截断为%zu)\n", pageIndex, written, sizeof(notifyResponse));
            continue;
        }

        int notifyLen = written;

        // 通过BLE主动发送通知
        esp_err_t err = BLEManager_SendData((uint8_t *)notifyResponse, notifyLen);
        if (err == ESP_OK) {
            dPrint(INFO, "已发送第 %d/%d 页WiFi列表, 长度:%d\n", pageIndex + 1, totalPages, notifyLen);
        } else {
            dPrint(DERROR, "发送第 %d 页WiFi列表失败\n", pageIndex);
        }

        // 延时，避免发送过快
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 返回完成响应
    strcpy(response, "{\"ret\":{\"WifiManager.SelectWifiList\":\"completed\"},\"data\":{}}");
    *responseLen = strlen(response);

    dPrint(INFO, "WiFi列表发送完成，共 %d 页\n", totalPages);
    return RTN_SUCCESS;
}

int SelectChargingLoadCurrentList(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    char tempstr[50] = {0};
    dPrint(DEBUG,"收到查询所有充电桩电流信息请求 request:%s\n",request);

    // 获取所有充电桩，计算正在充电的充电桩总电流
    int stationCount = 0;
    ChargingStation *allStations = SelectAllChargeStation(&stationCount);

    float totalChargingCurrent = 0;  // 所有正在充电的充电桩电流之和
    for (int i = 0; i < stationCount; i++) 
    {
        ChargingStation *s = &allStations[i];
        // 只统计已绑定且正在充电的充电桩
        if (s->isAdded == TRUE && strcmp(s->EVStatus, "Charging") == 0) {
            totalChargingCurrent += s->acCurrentL1 / 100.0;
            dPrint(DEBUG, "充电桩 %s 正在充电，电流: %u A\n", s->mac, s->acCurrentL1);
        }
    }
    // 获取总电表电流
    float meterCurrent = bl0942_get_current() / 10.0;
    // 计算负载电流（非充电负载）= 总电表电流 - 充电桩总电流
    float loadCurrent = (meterCurrent > totalChargingCurrent) ?
                           (meterCurrent - totalChargingCurrent) : 0;

    dPrint(INFO, "电流统计: 总电表=%u A, 充电桩总计=%u A, 其他负载=%u A\n",
           meterCurrent, totalChargingCurrent, loadCurrent);

    // 创建根JSON对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        dPrint(DERROR, "Failed to create root JSON object\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrentList\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 添加 ret 对象
    cJSON *ret = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ret", ret);
    cJSON_AddStringToObject(ret, "SubDeviceManager.SelectChargingLoadCurrentList", "success");

    // 添加 deviceId
    //cJSON_AddStringToObject(root, "deviceId", "N3Lite");
    // 添加 code
    //cJSON_AddNumberToObject(root, "code", 200);
    // 创建 data 对象
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);

    //负载电流
    memset(tempstr,0,sizeof(tempstr));
    snprintf(tempstr, sizeof(tempstr), "%.1f", loadCurrent); // 将整数转换为字符串，并指定最大长度
    cJSON_AddStringToObject(root, "LoadCurrent", tempstr);
    //电表电流
    memset(tempstr,0,sizeof(tempstr));
    snprintf(tempstr, sizeof(tempstr), "%.1f", meterCurrent); // 将整数转换为字符串，并指定最大长度
    cJSON_AddStringToObject(root, "MeterCurrent", tempstr);
    //充电桩电流之和
    memset(tempstr,0,sizeof(tempstr));
    snprintf(tempstr, sizeof(tempstr), "%.1f", totalChargingCurrent); // 将整数转换为字符串，并指定最大长度
    cJSON_AddStringToObject(root, "totalChargingCurrent", tempstr);
    // 创建 deviceList 数组
    cJSON *deviceList = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "deviceList", deviceList);
    
    // 遍历所有充电桩，添加到 deviceList
    for (int i = 0; i < stationCount; i++) {
        ChargingStation *station = &allStations[i];
        if(station->isAdded == FALSE)
        {
            continue;
        }
        // 创建充电桩设备对象
        cJSON *device = cJSON_CreateObject();
        memset(tempstr,0,sizeof(tempstr));
        snprintf(tempstr, sizeof(tempstr), "%.1f", station->acCurrentL1/100.0); // 将整数转换为字符串，并指定最大长度
        cJSON_AddStringToObject(device, "ChargingCurrent", tempstr);
        
        //能量
        memset(tempstr,0,sizeof(tempstr));
        snprintf(tempstr, sizeof(tempstr), "%.2f", station[i].energy); // 将整数转换为字符串，并指定最大长度
        cJSON_AddStringToObject(device, "energy", tempstr);
       
        // mac
        cJSON_AddStringToObject(device, "mac", station->mac);
        // subDevId
        cJSON_AddStringToObject(device, "subDevId", station->serialNum);
        //状态
        cJSON_AddStringToObject(device, "EVStatus", station->EVStatus);
        // 在线状态
        cJSON_AddStringToObject(device, "connectStatus",
                    (station->connectStatus == SUB_DEVICE_CONNECT_STATUS_ONLINE) ? "online" : "offline");
        // 添加设备到 deviceList 数组
        cJSON_AddItemToArray(deviceList, device);
    }

    // 生成紧凑格式的JSON字符串（无空格、制表符、换行符）
    char *jsonString = cJSON_PrintUnformatted(root);
    if (jsonString == NULL) {
        dPrint(DERROR, "Failed to print JSON\n");
        cJSON_Delete(root);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrentList\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 复制到response（外部已提供内存）
    int jsonLen = strlen(jsonString);
    if(jsonLen < 512)
    {
        strncpy(response, jsonString, jsonLen);
        response[jsonLen] = '\0';
        *responseLen = jsonLen;
    }
    else
    {
       dPrint(DERROR,"jsonLen > 512 not allow response\n");
       cJSON_free(jsonString);
       cJSON_Delete(root);
       strcpy(response,"{\"ret\": {\"SubDeviceManager.SelectChargingLoadCurrentList\":\"failed\"},\"data\":{}}");
       *responseLen = strlen(response);
       return RTN_FAIL;
    }
    // 释放资源
    cJSON_free(jsonString);
    cJSON_Delete(root);

    return RTN_SUCCESS;
}

int GetChargingStationsInfoRequest(char *method, char *request, int requestLen, char *response, int *responseLen)
{
    memset(response, 0, strlen(response));
    dPrint(INFO, "收到获取当前充电桩信息请求\n");

    // 创建响应根JSON对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        dPrint(DERROR, "Failed to create root JSON object\n");
        strcpy(response, "{\"ret\":{\"SubDeviceManager.GetChargingStationsInfo\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 添加 ret 对象
    cJSON *ret = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ret", ret);
    cJSON_AddStringToObject(ret, "SubDeviceManager.GetChargingStationsInfo", "success");

    // 创建 data 对象
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);

    // 创建充电桩列表数组
    cJSON *stationsArray = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "chargingStations", stationsArray);

    // 获取所有充电桩
    int stationCount = 0;
    ChargingStation *stations = SelectAllChargeStation(&stationCount);

    if (stations != NULL && stationCount > 0) {
        for (int i = 0; i < stationCount; i++) {
            // 返回所有已添加的充电桩（包括离线的）
            if (stations[i].isAdded == TRUE) {

                cJSON *stationObj = cJSON_CreateObject();

                // MAC地址
                cJSON_AddStringToObject(stationObj, "mac", stations[i].mac);

                // 充电桩名称
                cJSON_AddStringToObject(stationObj, "name", stations[i].name);

                // 限制电流（转换为A，保留一位小数）
                double limitCurrentA = stations[i].limitCurrent / 100.0;
                cJSON_AddNumberToObject(stationObj, "limitCurrent", limitCurrentA);

                // 实际电流（转换为A，保留一位小数）
                double actualCurrentA = stations[i].acCurrentL1 / 10.0;
                cJSON_AddNumberToObject(stationObj, "actualCurrent", actualCurrentA);

                // 充电桩状态
                cJSON_AddStringToObject(stationObj, "status", stations[i].EVStatus);

                // 在线状态
                cJSON_AddStringToObject(stationObj, "connectStatus",
                    (stations[i].connectStatus == SUB_DEVICE_CONNECT_STATUS_ONLINE) ? "online" : "offline");

                cJSON_AddItemToArray(stationsArray, stationObj);
            }
        }
    }

    // 生成JSON字符串
    char *jsonString = cJSON_PrintUnformatted(root);
    if (jsonString == NULL) {
        dPrint(DERROR, "Failed to print JSON\n");
        cJSON_Delete(root);
        strcpy(response, "{\"ret\":{\"SubDeviceManager.GetChargingStationsInfo\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    int jsonLen = strlen(jsonString);
    if (jsonLen < 512) {
        strncpy(response, jsonString, jsonLen);
        response[jsonLen] = '\0';
        *responseLen = jsonLen;
    } else {
        dPrint(DERROR, "jsonLen > 512 not allow response\n");
        cJSON_free(jsonString);
        cJSON_Delete(root);
        strcpy(response, "{\"ret\":{\"SubDeviceManager.GetChargingStationsInfo\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    cJSON_free(jsonString);
    cJSON_Delete(root);

    dPrint(INFO, "获取当前充电桩信息成功\n");
    return RTN_SUCCESS;
}

int SelectChargingHistory(char *method, char *request, int requestLen, char *response, int *responseLen)
{
    dPrint(INFO, "查询充电历史记录请求: %s\n", request);

    // 使用堆内存而不是栈内存，避免栈溢出
    char *allOrdersBuffer = (char *)malloc(8192);
    if (allOrdersBuffer == NULL) {
        dPrint(DERROR, "分配内存失败\n");
        strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    memset(allOrdersBuffer, 0, 8192);

    // 读取所有订单数据
    int ret = OrderStorage_ReadAllJSON(allOrdersBuffer, 8192);

    if (ret != 0) {
        dPrint(DERROR, "读取充电历史失败\n");
        strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{}}");
        *responseLen = strlen(response);
        free(allOrdersBuffer);
        return RTN_FAIL;
    }

    // 解析所有订单
    cJSON *allOrdersArray = cJSON_Parse(allOrdersBuffer);
    if (allOrdersArray == NULL || !cJSON_IsArray(allOrdersArray)) {
        dPrint(DERROR, "解析充电历史 JSON 失败\n");
        strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{}}");
        *responseLen = strlen(response);
        free(allOrdersBuffer);
        return RTN_FAIL;
    }

    // 释放缓冲区，已经解析为cJSON对象了
    free(allOrdersBuffer);

    // 获取总订单数
    int totalOrders = cJSON_GetArraySize(allOrdersArray);

    if (totalOrders == 0) {
        // 没有订单，直接返回空列表
        dPrint(INFO, "没有充电历史记录\n");
        strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{\"ChargingHistoryList\":[]}}");
        *responseLen = strlen(response);
        cJSON_Delete(allOrdersArray);
        return RTN_SUCCESS;
    }

    // 每次最多发送3条
    const int batchSize = 3;
    int totalBatches = (totalOrders + batchSize - 1) / batchSize;  // 向上取整

    dPrint(INFO, "总订单数: %d, 分 %d 批发送\n", totalOrders, totalBatches);

    // 发送前面的批次（通过 Notify）
    for (int batchIndex = 0; batchIndex < totalBatches - 1; batchIndex++) {
        // 创建当前批次的订单数组
        cJSON *batchOrdersArray = cJSON_CreateArray();
        int startIndex = batchIndex * batchSize;
        int endIndex = startIndex + batchSize;

        for (int i = startIndex; i < endIndex && i < totalOrders; i++) {
            cJSON *order = cJSON_GetArrayItem(allOrdersArray, i);
            if (order != NULL) {
                cJSON *orderCopy = cJSON_Duplicate(order, 1);
                cJSON_AddItemToArray(batchOrdersArray, orderCopy);
            }
        }

        // 构造 Notify JSON
        cJSON *notifyJson = cJSON_CreateObject();
        cJSON_AddStringToObject(notifyJson, "method", "SubDeviceManager.SelectChargingHistory");

        cJSON *dataObj = cJSON_CreateObject();
        cJSON_AddItemToObject(dataObj, "ChargingHistoryList", batchOrdersArray);
        cJSON_AddItemToObject(notifyJson, "data", dataObj);

        // 转换为字符串
        char *notifyStr = cJSON_PrintUnformatted(notifyJson);
        if (notifyStr != NULL) {
            int notifyLen = strlen(notifyStr);
            if (notifyLen < 2048) {
                // 通过BLE主动发送通知
                esp_err_t err = BLEManager_SendData((uint8_t *)notifyStr, notifyLen);
                if (err == ESP_OK) {
                    dPrint(INFO, "已发送第 %d/%d 批充电历史, 长度:%d\n", batchIndex + 1, totalBatches, notifyLen);
                } else {
                    dPrint(DERROR, "发送第 %d 批充电历史失败\n", batchIndex);
                }
            } else {
                dPrint(DERROR, "第 %d 批充电历史过长: %d 字节\n", batchIndex, notifyLen);
            }
            cJSON_free(notifyStr);
        }
        cJSON_Delete(notifyJson);

        // 延时，避免发送过快
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 处理最后一批（通过 return 返回）
    cJSON *lastBatchArray = cJSON_CreateArray();
    int lastBatchStartIndex = (totalBatches - 1) * batchSize;

    for (int i = lastBatchStartIndex; i < totalOrders; i++) {
        cJSON *order = cJSON_GetArrayItem(allOrdersArray, i);
        if (order != NULL) {
            cJSON *orderCopy = cJSON_Duplicate(order, 1);
            cJSON_AddItemToArray(lastBatchArray, orderCopy);
        }
    }

    // 构造最后一批的响应 JSON
    cJSON *responseJson = cJSON_CreateObject();
    cJSON_AddStringToObject(responseJson, "method", "SubDeviceManager.SelectChargingHistory");

    cJSON *dataObj = cJSON_CreateObject();
    cJSON_AddItemToObject(dataObj, "ChargingHistoryList", lastBatchArray);
    cJSON_AddItemToObject(responseJson, "data", dataObj);

    // 转换为字符串
    char *responseStr = cJSON_PrintUnformatted(responseJson);
    if (responseStr != NULL) {
        int jsonLen = strlen(responseStr);
        if (jsonLen < 2048) {
            strcpy(response, responseStr);
            *responseLen = jsonLen;
            dPrint(INFO, "已返回最后一批充电历史 (第 %d/%d 批), 长度:%d\n", totalBatches, totalBatches, jsonLen);
        } else {
            dPrint(DERROR, "最后一批充电历史过长: %d 字节\n", jsonLen);
            strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{}}");
            *responseLen = strlen(response);
            cJSON_free(responseStr);
            cJSON_Delete(responseJson);
            cJSON_Delete(allOrdersArray);
            return RTN_FAIL;
        }
        cJSON_free(responseStr);
    } else {
        dPrint(DERROR, "生成最后一批响应 JSON 失败\n");
        strcpy(response, "{\"method\":\"SubDeviceManager.SelectChargingHistory\",\"data\":{}}");
        *responseLen = strlen(response);
        cJSON_Delete(responseJson);
        cJSON_Delete(allOrdersArray);
        return RTN_FAIL;
    }

    cJSON_Delete(responseJson);
    cJSON_Delete(allOrdersArray);

    dPrint(INFO, "充电历史发送完成，共 %d 条订单，分 %d 批\n", totalOrders, totalBatches);
    return RTN_SUCCESS;
}

int SetChargingWorkMode(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    char mac[32] = {0};
    char workMode[8] = {0};

    //解析数据请求
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse request JSON\n");
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SetChargingWorkMode\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }

    // 获取 data 对象
    cJSON *dataObj = cJSON_GetObjectItem(requestJson, "data");
    if (dataObj == NULL || !cJSON_IsObject(dataObj)) {
        dPrint(DERROR, "Failed to get data object\n");
        cJSON_Delete(requestJson);
        strcpy(response,"{\"ret\": {\"SubDeviceManager.SetChargingWorkMode\":\"failed\"},\"data\":{}}");
        *responseLen = strlen(response);
        return RTN_FAIL;
    }
    cJSON *deviceInfo = cJSON_GetObjectItem(dataObj, "deviceInfo");
    cJSON *macItem = cJSON_GetObjectItem(deviceInfo, "mac");
    cJSON *workModeItem = cJSON_GetObjectItem(deviceInfo, "workMode");

    if (cJSON_IsString(macItem)) 
    {
        strncpy(mac,macItem->valuestring,sizeof(mac));
    }

    if (cJSON_IsString(workModeItem))
    {
         strncpy(workMode,workModeItem->valuestring,sizeof(workMode));
    }

    // 设置工作模式
    chargingWorkMode(mac, workMode);

    return RTN_SUCCESS;
}