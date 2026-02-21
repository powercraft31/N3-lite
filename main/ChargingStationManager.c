#include "ChargingStationManager.h"
#include "PlcManager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/time.h>
#include "DeBug.h"
#include "PlcRecvQueue.h"
#include "PlcModule3121.h"
#include "StringUtils.h"
#include "types.h"
#include "HexUtils.h"
#include "PlcSendQueue.h"
#include "PlcProtocolService.h"
#include "ConfigManager.h"
#include "ConfigManagerController.h"
#include "cJSON.h"
#include "OrderStorage.h"

//充电桩查询线程
TaskHandle_t sg_xSelectStationTaskHandle;

//定义可以搜索到的充电桩数量
SearchStation sg_SearchStation[CHAEGING_STATION_MAX_NUM] = {0};
//充电桩数组，最多2个充电桩
ChargingStation sg_chargingStationArray[CHAEGING_STATION_MAX_NUM] = {0};
//充电桩数量
int sg_chargingStationCount = 0;

//是否立马触发搜索
BOOL sg_isQuicklySearch = FALSE;

//线程锁
//SemaphoreHandle_t g_mutex;
//充电桩管理类初始化
void InitChargeStationManager()
{
   
    //PlcManager初始化
    PlcInit();
    //休眠1秒
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    //初始化设置一些默认值
    InitChargingStationInfo(SUB_DEVICE_CONNECT_STATUS_OFFLINE);
    //注册接收PLC消息协议处理函数
    RegChargeStationDataFunc(ProcessHexPduPacket);
    //监听充电桩事件
    //监听充电桩主动上报事件
    SubscribeEvent(EVENT_REPORT_STATUS,HandleChargeStationEvent);
    //监听远程开启充电事件
    SubscribeEvent(EVENT_REMOTE_START,HandleChargeStationEvent);
    //监听远程停止充电事件
    SubscribeEvent(EVENT_REMOTE_STOP,HandleChargeStationEvent);
    //监听自动控制模块自动开启充电事件
    SubscribeEvent(EVENT_AUTO_START,HandleChargeStationEvent);
    //监听自动控制模块暂停事件
    SubscribeEvent(EVENT_AUTO_SUSPEND,HandleChargeStationEvent);
    //监听自动控制模块限制电流事件
    SubscribeEvent(EVENT_AUTO_SET_LIMIT_CUUR,HandleChargeStationEvent);
    
    //开启一个线程定时检测有没有充电桩在线/掉线/
    xTaskCreate(&CheckStationThread, "CheckStationThread", 4096, NULL, 5, &sg_xSelectStationTaskHandle);
    
    //开启一个定时器定时查询电压电流值
    AddTimer(TIMER_ID_SELECT_STATION_REAL_DATA,500,GetChargeStationRealDataTimerFunc);
    //开启一个定时器检测充电桩的在离线,10秒检测一次
    AddTimer(TIMER_ID_CEHCK_STATION_CONNECT_STATUS,100*10,CheckStationConnectTimerFunc);
    return;
}
//参数上电的时候默认离线，添加的时候默认在线
int InitChargingStationInfo(EnumConnectStatus connecStatus)
{
    
    for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
    {
        memset(sg_chargingStationArray[i].mac,0,sizeof(sg_chargingStationArray[i].mac));
        //这里虽然时离线，但是下面找到了之后会设置为connecStatus
        sg_chargingStationArray[i].connectStatus = SUB_DEVICE_CONNECT_STATUS_OFFLINE;
        sg_chargingStationArray[i].phase = 1;
        sg_chargingStationArray[i].enumStatus = Poweron;
        strncpy(sg_chargingStationArray[i].EVStatus,"Poweron",strlen("Poweron"));
        sg_chargingStationArray[i].isPlugged = -1;
        sg_chargingStationArray[i].isEvseReady = -1;
        sg_chargingStationArray[i].isEvReady = -1;
        sg_chargingStationArray[i].workMode = Undefine;
        //未添加充电桩
        sg_chargingStationArray[i].isAdded = FALSE;
        //设置的电流值默认为0
        sg_chargingStationArray[i].limitCurrent = 0;
        //给充电桩的名称赋默认值
        strncpy(sg_chargingStationArray[i].name,"Charge01",strlen("Charge01"));
        //心跳次数
        sg_chargingStationArray[i].heart_beat_num = 0;
        sg_chargingStationArray[i].maxlimitCurrent = 32;
        sg_chargingStationArray[i].chargingMethod = FALSE;
        sg_chargingStationArray[i].lastenumStatus = Poweron;

        // 清空充电记录字段，避免重新绑定设备时数据混乱
        sg_chargingStationArray[i].energy = 0;
        sg_chargingStationArray[i].lastEnergy = 0;
        sg_chargingStationArray[i].startTime = 0;
        sg_chargingStationArray[i].endTime = 0;
        sg_chargingStationArray[i].duration = 0;
        sg_chargingStationArray[i].isStartTimeCalibrated = FALSE;
    }
    sg_chargingStationCount = 0;
    //从配置文件读取，初始化充电桩
    //读取本地文件中已经存储过的充电桩
    char json_string[512] = {0};
    ConfigStatus_t status = GetEVConfigJSON(json_string,sizeof(json_string));
    if(status != CONFIG_OK || strlen(json_string) == 0)
    {
        dPrint(WARN,"GetEVConfigJSON failed\n");
        return RTN_SUCCESS;
    }
    //获取到配置之后，读取配置更新内存中的是否已添加/充电桩最大限制电流
    // 验证JSON格式
    cJSON *array = cJSON_Parse(json_string);
    if (!array) 
    {
        dPrint(DERROR, "Invalid JSON format\n");
        return RTN_SUCCESS;
    }

    if (!cJSON_IsArray(array)) 
    {
        dPrint(DERROR, "JSON must be an array\n");
        cJSON_Delete(array);
        return RTN_SUCCESS;
    }
    
    //int i;
    cJSON *item;
    cJSON_ArrayForEach(item, array)
    {
        if (!cJSON_HasObjectItem(item, "mac") || !cJSON_HasObjectItem(item, "name") || !cJSON_HasObjectItem(item, "subDevId"))
        {
            dPrint(DERROR,"mac, name, subDevId not exist\n");
        }
        else
        {
            //根据mac地址查找充电桩对象
            char* mac = cJSON_GetObjectItem(item, "mac")->valuestring;
            char* subDevId = cJSON_GetObjectItem(item, "subDevId")->valuestring;
            dPrint(DEBUG,"初始化充电桩mac:%s,subDevId:%s\n",mac,subDevId);
            sg_chargingStationArray[sg_chargingStationCount].isAdded = TRUE;
            strncpy(sg_chargingStationArray[sg_chargingStationCount].mac,mac,sizeof(sg_chargingStationArray[sg_chargingStationCount].mac));
            strncpy(sg_chargingStationArray[sg_chargingStationCount].serialNum,subDevId,sizeof(sg_chargingStationArray[sg_chargingStationCount].serialNum));
            sg_chargingStationArray[sg_chargingStationCount].connectStatus = connecStatus;
            if(cJSON_HasObjectItem(item, "mdcFwVersion"))
            {
                char* mdcFwVersion = cJSON_GetObjectItem(item, "mdcFwVersion")->valuestring; 
                strncpy(sg_chargingStationArray[sg_chargingStationCount].mdcFwVersion,mdcFwVersion,sizeof(sg_chargingStationArray[sg_chargingStationCount].mdcFwVersion));   
            }
            //一次查询充电桩的状态,先放入到队列
            SendPlcDataByDestMac(sg_chargingStationArray[sg_chargingStationCount].mac,SELECT_CHARGING_STATION_STATUS_CMD,strlen(SELECT_CHARGING_STATION_STATUS_CMD));
            SendPlcDataByDestMac(sg_chargingStationArray[sg_chargingStationCount].mac,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));
            sg_chargingStationCount++;
        }
    }
    cJSON_Delete(array);

    //搜索列表也需要更新，如果没有找到则是未添加，找到则是已添加

    return RTN_SUCCESS;
}
SearchStation* GetSearchStation(char *nvMac)
{
    for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
    {
        if(strlen(sg_SearchStation[i].mac) == 0)
        {
            continue;
        }
        if(strncmp(sg_SearchStation[i].mac,nvMac,sizeof(sg_SearchStation[i].mac)) == 0)
        {
            return &sg_SearchStation[i];
        }
    }
    return NULL;
}
ChargingStation* GetChargingStation(char *nvMac)
{
    for(int i = 0;i<sg_chargingStationCount;i++)
    {
        if(strncmp(sg_chargingStationArray[i].mac,nvMac,sizeof(sg_chargingStationArray[i].mac)) == 0)
        {
            return &sg_chargingStationArray[i];
        }
    }
    return NULL;
}
//根据mac地址获取充电桩的索引号
int GetStationIndexByMac(char *nvMac)
{
    int index = 0;
    for(int i = 0;i<sg_chargingStationCount;i++)
    {
        if(strncmp(sg_chargingStationArray[i].mac,nvMac,sizeof(sg_chargingStationArray[i].mac)) == 0)
        {
           index =  i;
           break;
        }
    }
    return index;
}
int QuickLySearchStation()
{
    sg_isQuicklySearch = TRUE;
    return RTN_SUCCESS;
}
void CheckStationThread(void *pvParameter)
{
    char *rcv_buf = (char *)malloc(MAX_RECEI_BUFF_LEN);
    if (rcv_buf == NULL) {
        dPrint(DERROR,"CheckStationThread: Failed to allocate memory!\n");
        vTaskDelete(NULL);  // 删除自己
        return;
    }

    //int loop_count = 0;
    while (1)
    {
        
        //dPrint(INFO,"CheckStationThread: Loop #%d starting...\n", loop_count);
        if(sg_isQuicklySearch == TRUE)
        {
            sg_isQuicklySearch = FALSE;
            //loop_count = 0;
        }
        else
        {
            for(int i = 0; i < 2; i++) 
            {
                vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
                taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
            }
            continue;
        }

        memset(rcv_buf,0,MAX_RECEI_BUFF_LEN);
        //先把PLC发送线程暂停,接收完所有充电桩信息后再重新启动
        // 暂停PLC发送线程任务
        //暂停循环读取串口线程

        if(IsRuningPlcTaskThread())
        {
            SuspendAllPlcTaskThread();
        }
        else
        {
            //dPrint(DEBUG,"PLC发送线程已经是挂起状态\n");
        }
        //每次搜索前清空搜索充电桩
        memset(&sg_SearchStation[0],0,sizeof(sg_SearchStation));
        //获取所有充电桩的mac地址
        int count = GetMACadress(rcv_buf,&sg_SearchStation[0]);
        //dPrint(DEBUG,"GetMACadress返回: count=%d\n", count);
        if(count < 0)
        {
            dPrint(DERROR,"GetMACadress error\n");
            for(int i = 0; i < 2; i++) 
            {
                vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
                taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
            }
            continue;
        }
        else if(count == 0)
        {
            dPrint(DEBUG,"没有获取到其他充电桩\r\n");  
  
        }
        else
        {
            //恢复接收线程，然后再发送数据
            if(!IsRuningPlcTaskThread())
            {
                // 恢复PLC发送线程任务
                //dPrint(DEBUG,"恢复PLC发送线程任务\n");
                ResumeAllPlcTaskThread();
                //dPrint(DEBUG,"PLC发送线程已恢复\n");
            }
            else
            {
                dPrint(WARN,"PLC发送线程应该已恢复，但状态不对!\n");
            }   
            //从rcv_buf获取充电桩的信噪比/信号强度
            //开始获取其他信息
            for(int i = 0;i<count;i++)
            {
                if(strlen(sg_SearchStation[i].mac) == 0)
                {
                     dPrint(DERROR,"没有找到充电桩的mac地址\n");  
                     continue; 
                }
                //不在这里设置在线，应该根据心跳包设置在线离线
                //获取充电桩序列号
                SendPlcDataByDestMac(sg_SearchStation[i].mac,SELECT_CHARGING_STATION_SN,strlen(SELECT_CHARGING_STATION_SN));
                //获取充电桩版本号
                SendPlcDataByDestMac(sg_SearchStation[i].mac,SELECT_CHARGING_STATION_FIRMWARE,strlen(SELECT_CHARGING_STATION_FIRMWARE));
                //一次性查询电流电压/能量
                SendPlcDataByDestMac(sg_SearchStation[i].mac,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));

            }
        }
        dPrint(DEBUG,"搜索到的充电桩的数量为:%d\n",count);
        //sg_chargingStationCount = count;
        //根据配置去更新搜索列表的已添加状态
        UpdateAllSearchStationByConfig();
 
    }
    free(rcv_buf);
    return;
}
//暂停充电桩查询线程
void SuspendStationTaskThread()
{
    vTaskSuspend(sg_xSelectStationTaskHandle);
}
//恢复充电桩查询线程
void ResumeStationTaskThread()
{
    vTaskResume(sg_xSelectStationTaskHandle);
}
extern SearchStation * SelectAllSearchStation(int *stationCount)
{
    int count = 0;
    for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
    {
        if(strlen(sg_SearchStation[i].mac) != 0)
        {
            count++;
        }
    }
    *stationCount = count;
    return &sg_SearchStation[0];
}
ChargingStation *SelectAllChargeStation(int *stationCount)
{
    *stationCount = sg_chargingStationCount;
    return &sg_chargingStationArray[0];
}

/**
 * @brief 处理充电时间和能量记录
 * @param station 充电桩指针
 * @description 通过比较lastenumStatus和当前enumStatus来判断充电开始和结束，
 *              并记录相应的时间、能量和充电时长
 */
void HandleChargingTimeRecord(ChargingStation *station)
{
    if (station == NULL) {
        return;
    }

    // 检查系统时间是否已经校准
    BOOL isTimeCalibrated = IsSystemTimeCalibrated();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long currentTime = (unsigned long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // 检查是否从非Charging状态变为Charging状态（充电开始）
    if (station->lastenumStatus != Charging && station->lastenumStatus != SuspendEvse && station->enumStatus == Charging) 
    {
        // 记录开始时间和能量
        station->startTime = currentTime;
        station->lastEnergy = station->energy;
        station->duration = 0;
        // 记录开始时的时间校准状态
        station->isStartTimeCalibrated = isTimeCalibrated;
        dPrint(INFO, "设备%s开始充电,此时能量为%.2f Wh (lastEnergy=%.2f Wh)\n", station->serialNum, station->energy, station->lastEnergy);
        if (isTimeCalibrated) {
            dPrint(INFO ,"开始充电，此时时间已经校准， 开始时间为%lld\n", station->startTime);
            // 时间已校准
        } else {
            int index = GetStationIndexByMac(station->mac);
            dPrint(INFO ,"开始充电，此时时间未校准， 采用定时器计时记录时间\n");
            if(index == 0) {
                AddHwTimer(HW_TIMER_CHARGING_DURING_ID_0, 1, HW_TIMER_MODE_PERIODIC,
                           ChargingDurationHwTimer, station->mac, strlen(station->mac) + 1);
            } else {
                AddHwTimer(HW_TIMER_CHARGING_DURING_ID_1, 1, HW_TIMER_MODE_PERIODIC,
                           ChargingDurationHwTimer, station->mac, strlen(station->mac) + 1);
            }
        }
    }
    // 检查是否从Charging/SuspendEvse状态变为其他状态（充电结束）
    // 注意：Charging <-> SuspendEvse 互相切换不算结束
    else if ((station->lastenumStatus == Charging || station->lastenumStatus == SuspendEvse) &&
             (station->enumStatus != Charging && station->enumStatus != SuspendEvse)) 
    {
        // 记录结束时间
        station->endTime = currentTime;
        //检测到充电结束，清空充电方式
        station->chargingMethod = FALSE;
        dPrint(INFO, "设备%s结束充电,此时能量为%.2f Wh, lastEnergy=%.2f Wh, 能量差值=%.2f Wh\n",
               station->serialNum, station->energy, station->lastEnergy, station->energy - station->lastEnergy);
        if (station->isStartTimeCalibrated) 
        {
            // 开始时已校准：使用时间戳计算精确的充电时长
            dPrint(INFO, "结束充电，开始时间已校准， 结束时长为%lld\n", station->endTime);
            if (station->startTime > 0) {
                station->duration = (int)((station->endTime - station->startTime) / 1000);
                dPrint(INFO, "充电时长为%d秒\n", station->duration);
            }
        } 
        else 
        {
            // 开始时未校准：停止硬件定时器，保留累加的 duration
            int index = GetStationIndexByMac(station->mac);
            if(index == 0) {
                DelHwTimer(HW_TIMER_CHARGING_DURING_ID_0);
            } else {
                DelHwTimer(HW_TIMER_CHARGING_DURING_ID_1);
            }
            dPrint(INFO, "结束充电，开始时间未校准， 累计时长为%lld\n", station->duration);

        }
        // 记录充电时长和能量写入到flash中
        OrderStorage_SaveFromStation(station);
    }
    // 更新上一次状态
    station->lastenumStatus = station->enumStatus;
}

void HandleChargeStationEvent(EVENT_TYPE eventType,char *args,int argLen)
{
    if(argLen != sizeof(ChargingStation))
    {
        dPrint(DERROR,"argLen != sizeof(ChargingStation)\n");
        return;
    }
    //获取锁
    static BOOL isLock = FALSE;
    while(isLock == TRUE)
    {
        vTaskDelay(1);  // 每次延时
    }

    isLock = TRUE;
    //xSemaphoreTake(g_mutex, portMAX_DELAY);
    EVENT_TYPE event = eventType;
    ChargingStation *station = (ChargingStation *)args;
    //如果是主动上报状态
    switch(station->enumStatus)
    {
        //只有刚上电的时候会走Poweron
        case Poweron:
        {
            //未插枪，充电桩未准备标识可用状态
            if(event == EVENT_REPORT_STATUS && station->connectStatus == SUB_DEVICE_CONNECT_STATUS_OFFLINE)
            {
                dPrint(INFO, "充电桩主动拔掉线 station->subDeviceId:%s\n",station->serialNum);
            }
            else if (station->isPlugged != TRUE && station->isEvseReady != TRUE) 
            {		
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
                
            }
            else if(station->isPlugged == -1 || station->isEvseReady == -1)
            {
                //只要有一个未赋值标识
                station->enumStatus = Unavailable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Unavailable",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == FALSE)
            {
                //已插枪，充电桩未准备好,标识暂停
                station->enumStatus = SuspendEv;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"SuspendEv",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady != TRUE)
            {
                station->enumStatus = Preparing;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Preparing",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady == TRUE)
            {
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,未知的状态 Poweron failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            dPrint(INFO,"subdevId:%s,Poweron刚上电收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,EVStatus:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->EVStatus);
            break;
        }
        case Availiable:
        {
            //电流清0
            station->acCurrentL1 = 0;
            //如果充电桩主动上报了插枪状态，就变为准备状态
            if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE)
            {
                station->enumStatus = Preparing;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Preparing",sizeof(station->EVStatus));
                dPrint(INFO,"subdevId:%s,Availiable收到充电桩的状态上报isPlugged:%d,充电桩状态从Availiable -> Preparing 变化\n",station->serialNum,station->isPlugged);
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s, Availiable failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            break;
        }
        case Preparing:
        {
            //电流清0
            station->acCurrentL1 = 0;
            //如果充电桩上报了拔枪状态，就变为可用状态
            if(event == EVENT_REPORT_STATUS && station->isPlugged == FALSE)
            {
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady == TRUE)
            {
                //如果状态上报都准备好了，就是充电中
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));

            }
            //else if(event == EVENT_AUTO_START || event == EVENT_REMOTE_START)
            else if(event == EVENT_REMOTE_START)
            {
                //如果远端发送了启动充电，就是充电中的状态
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
                //发送启动充电命令
                dPrint(INFO,"收到自动控制模块或者手机APP端开启充电指令");
                SendPlcDataByDestMac(station->mac ,REMOTE_START_CHARGING_CMD,strlen(REMOTE_START_CHARGING_CMD));
                SendPlcDataByDestMac(station->mac ,REMOTE_START_TX_CMD,strlen(REMOTE_START_TX_CMD));
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,未知的状态 Preparing failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }

            dPrint(INFO,"subdevId:%s,Preparing收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            
            break;
        }
        case Charging:
        {
            if(event == EVENT_REPORT_STATUS && station->isEvReady == FALSE)
            {
                //负载跳停
                station->enumStatus = Finish;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Finish",sizeof(station->EVStatus));
                //结束之后，电流清0
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == FALSE)
            {
                //拔枪
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REMOTE_STOP)
            {
                //远程停止就结束交易
                station->enumStatus = Finish;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Finish",sizeof(station->EVStatus));
                dPrint(INFO,"收到自动控制模块或者手机APP端停止充电指令\n");
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_CHARGING_CMD,strlen(REMOTE_STOP_CHARGING_CMD));
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_TX_CMD,strlen(REMOTE_STOP_TX_CMD));
            }
            else if(event == EVENT_AUTO_SUSPEND)
            {
                //自动控制模块暂停事件
                station->enumStatus = SuspendEvse;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"SuspendEvse",sizeof(station->EVStatus));
                
            }
            else if(event == EVENT_AUTO_SET_LIMIT_CUUR)
            {
                //只有在充电状态才能设置电流
                
                //先把整形转换为16进制字符串
                char original[256] = {0};
                char replacement[10] = {0};
                unsigned int limitCurrent = Ntohl((unsigned int)station->limitCurrent);
                //赋值原始字符串
                strncpy(original,UPDATE_CHARGING_STATION_LIMIT_CURRENT,sizeof(original));
                //将整形值转16进制字符串
                HexArrayToStr(replacement,(unsigned char *)&limitCurrent,sizeof(limitCurrent));
                //设置电流应该把电流的数值替换掉
                StringReplace(original,strlen("AT+SEND=4080E1346353,34,7E00110000020104010104"),8,replacement);
                dPrint(INFO,"自动控制模块设置限制电流:%d\n",station->limitCurrent);
                SendPlcDataByDestMac(station->mac ,original,strlen(original));
                DelTimer(TIMER_ID_SELECT_STATION_REAL_DATA);
                //定时器立马查询一次
                AddTimer(TIMER_ID_SELECT_STATION_REAL_DATA,100*8,GetChargeStationRealDataTimerFunc);
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,Charging failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            dPrint(INFO,"subdevId:%s,Charging收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            
            //dPrint(DEBUG,"充电中立马查询一次电压电流\n");
            //SendPlcDataByDestMac(station->mac,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));
            break;    
        }
        case Finish:
        {
            //收到拔枪通知
            if(event == EVENT_REPORT_STATUS && station->connectStatus == SUB_DEVICE_CONNECT_STATUS_OFFLINE)
            {
                dPrint(INFO, "充电桩主动拔掉线\n");
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == FALSE)
            {
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
                dPrint(INFO,"subdevId:%s,Finish收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady == TRUE)
            {
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
                //dPrint(INFO,"Finish收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,Finish failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            
            break;    
        }
        case SuspendEv:
        {
            //收到拔枪通知
            if(event == EVENT_REPORT_STATUS && station->isPlugged == FALSE)
            {
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady != TRUE && station->isEvReady != TRUE)
            {
                //插枪，但是其他状态都不是TRUE,说明是结束了
                station->enumStatus = Finish;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Finish",sizeof(station->EVStatus));
           
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady != TRUE)
            {
                dPrint(INFO,"是暂停状态\n");
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady == TRUE)
            {
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
                dPrint(INFO,"subdevId:%s,从SuspendEv到Charging\n",station->serialNum);
            }
            else if(event == EVENT_AUTO_START)
            {
                station->enumStatus = Charging;  
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REMOTE_STOP)
            {
                dPrint(INFO,"收到自动控制模块或者手机APP端停止充电指令");
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_CHARGING_CMD,strlen(REMOTE_STOP_CHARGING_CMD));
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_TX_CMD,strlen(REMOTE_STOP_TX_CMD));
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,SuspendEv failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            dPrint(INFO,"subdevId:%s,SuspendEv收到充电桩的状态上报isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            break;    
        }
        case SuspendEvse:
        {
            //收到拔枪通知
            if(event == EVENT_REPORT_STATUS && station->isPlugged == FALSE)
            {
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady != TRUE && station->isEvReady != TRUE)
            {
                //插枪，但是其他状态都不是TRUE,说明是结束了
                station->enumStatus = Finish;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Finish",sizeof(station->EVStatus));
              
            }
            else if(event == EVENT_AUTO_START)
            {
                station->enumStatus = Charging;  
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
            }
            else if(event == EVENT_REPORT_STATUS && station->isPlugged == TRUE && station->isEvseReady != TRUE && station->isEvReady == TRUE)
            {
                dPrint(INFO,"是暂停状态\n");
            }
            else if(event == EVENT_REMOTE_STOP)
            {
                dPrint(INFO,"收到自动控制模块或者手机APP端停止充电指令\n");
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_CHARGING_CMD,strlen(REMOTE_STOP_CHARGING_CMD));
                SendPlcDataByDestMac(station->mac ,REMOTE_STOP_TX_CMD,strlen(REMOTE_STOP_TX_CMD));
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,SuspendEvse failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            dPrint(INFO,"subdevId:%s,SuspendEvse收到充电桩的状态上报 event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d,充电桩状态:%s\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady,station->EVStatus);
            break;    
        }
        case Reserved:
        {
            dPrint(INFO,"状态是Reserved\n");
            break;    
        }
        case Unavailable:
        {
            dPrint(DEBUG,"状态是Unavailable\n");
            //未插枪，充电桩未准备标识可用状态
            if (station->isPlugged != TRUE && station->isEvseReady != TRUE) 
            {		
                station->enumStatus = Availiable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Availiable",sizeof(station->EVStatus));
                
            }
            else if(station->isPlugged == -1 || station->isEvseReady == -1)
            {
                //只要有一个未赋值标识
                station->enumStatus = Unavailable;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Unavailable",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == FALSE)
            {
                //已插枪，充电桩未准备好,标识暂停
                station->enumStatus = SuspendEv;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"SuspendEv",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady != TRUE)
            {
                station->enumStatus = Preparing;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Preparing",sizeof(station->EVStatus));
            }
            else if(station->isPlugged == TRUE && station->isEvseReady == TRUE && station->isEvReady == TRUE)
            {
                station->enumStatus = Charging;
                memset(station->EVStatus,0,sizeof(station->EVStatus));
                strncpy(station->EVStatus,"Charging",sizeof(station->EVStatus));
            }
            else
            {
                 dPrint(DERROR,"subdevId:%s,未知的状态 Poweron failed event:%d,isPlugged:%d,isEvseReady:%d,isEvReady:%d\n",station->serialNum,event,station->isPlugged,station->isEvseReady,station->isEvReady);   
            }
            dPrint(INFO,"subdevId:%s,Unavailable到充电桩的状态上报isPlugged:%d,isEvseReady:%d,EVStatus:%s\n",station->serialNum,station->isPlugged,station->isEvseReady,station->EVStatus);
            break;    
        }
        case Faulted:
        {
            dPrint(DEBUG,"状态是Faulted\n");
            break;    
        }
        default:
        {
            dPrint(WARN,"没有找到状态:%d\n",station->enumStatus);
            break;          
        }
    }
    // 处理充电时间和能量记录
    HandleChargingTimeRecord(station);
    //xSemaphoreGive(g_mutex);  // 释放锁
    isLock = FALSE;
    return;
}

void GetChargeStationRealDataTimerFunc(TIMER_ID timerId, char *arg, int argLen)
{
    //如果发送线程暂停了，也不需要去查询
    //dPrint(DEBUG,"充电桩获取实时数据定时器\n");
    BOOL bRet;
    static int s_count = 0;
    bRet = IsRuningPlcTaskThread();
    if(!bRet)
    {
        dPrint(DEBUG,"PLC任务线程暂停中\n");
    }
    else
    {
        for(int i = 0;i<sg_chargingStationCount;i++)
        {
            //先判断是否在充电状态
            if(sg_chargingStationArray[i].enumStatus != Charging 
                && sg_chargingStationArray[i].enumStatus != SuspendEv
                && sg_chargingStationArray[i].enumStatus != SuspendEvse
                && sg_chargingStationArray[i].enumStatus != Finish)
            {
                //一次查询充电桩的状态,先放入到队列
                if(s_count ==6)
                {
                    //查询充电桩状态
                    dPrint(INFO ,"进入查询充电桩的状态---\n");
                    SendPlcDataByDestMac(sg_chargingStationArray[i].mac,SELECT_CHARGING_STATION_STATUS_CMD,strlen(SELECT_CHARGING_STATION_STATUS_CMD)); 
                    
                    s_count = 0;  
                }
                s_count++;
                continue;
            }
            
            if(s_count ==6)
            {
                //查询充电桩状态
                dPrint(INFO ,"进入查询充电桩的状态---\n");
                SendPlcDataByDestMac(sg_chargingStationArray[i].mac,SELECT_CHARGING_STATION_STATUS_CMD,strlen(SELECT_CHARGING_STATION_STATUS_CMD));
                s_count = 0;
            }
            s_count++;
            dPrint(DEBUG,"定时器一次性查询电压/电流/能量\n");
            SendPlcDataByDestMac(sg_chargingStationArray[i].mac ,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));
  
        }
    }
    //每隔6秒查询一次
    AddTimer(TIMER_ID_SELECT_STATION_REAL_DATA,100*6,GetChargeStationRealDataTimerFunc);
    return;
}

void UpdateAllSearchStationByConfig()
{
    //立马更新一次，把搜索列表的已添加/未添加更新一次
    for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
    {
        sg_SearchStation[i].isAdded = FALSE;
    }
    //读取本地文件中已经存储过的充电桩
    char json_string[512] = {0};
    ConfigStatus_t status = GetEVConfigJSON(json_string,sizeof(json_string));
    if(status != CONFIG_OK || strlen(json_string) == 0)
    {
        dPrint(WARN,"GetEVConfigJSON failed\n");
        return;
    }
    //获取到配置之后，读取配置更新内存中的是否已添加/充电桩最大限制电流
    // 验证JSON格式
    cJSON *array = cJSON_Parse(json_string);
    if (!array) 
    {
        dPrint(DERROR, "Invalid JSON format\n");
        return ;
    }

    if (!cJSON_IsArray(array)) 
    {
        dPrint(DERROR, "JSON must be an array\n");
        cJSON_Delete(array);
        return;
    }
    
    //int i;
    cJSON *item;
    cJSON_ArrayForEach(item, array)
    {
        if (!cJSON_HasObjectItem(item, "mac") || !cJSON_HasObjectItem(item, "name") || !cJSON_HasObjectItem(item, "subDevId"))
        {
            dPrint(DERROR,"mac, name, subDevId not exist\n");
        }
        else
        {
            //根据mac地址查找充电桩对象
            SearchStation* station = GetSearchStation(cJSON_GetObjectItem(item, "mac")->valuestring);
            //ChargingStation* station = GetChargingStation(cJSON_GetObjectItem(item, "mac")->valuestring);
            if(station == NULL)
            {
                dPrint(WARN,"在内存中没有找到已保存过的充电桩 mac:%s,subDevId:%s\n",cJSON_GetObjectItem(item, "mac")->valuestring,cJSON_GetObjectItem(item, "subDevId")->valuestring);
            }
            else
            {
                dPrint(DEBUG,"找到了充电桩mac: %s, subDevId:%s\n", cJSON_GetObjectItem(item, "mac")->valuestring, cJSON_GetObjectItem(item, "subDevId")->valuestring);
                //更新内存中的是否已添加标识
                station->isAdded = TRUE;
                station->maxlimitCurrent = 32;
                //更新充电桩名称
                //strcpy(station->name,cJSON_GetObjectItem(item, "name")->valuestring);
            }
        }
    }
    cJSON_Delete(array);
    return;
}

int StartCharging(char *mac,unsigned long long startTime)
{
    //启动定时器开始计时,1秒定时器
    // 注意：传递 strlen(mac)+1 以包含字符串结束符 '\0'
    //获取充电桩状态
    ChargingStation* station = GetChargingStation(mac);
    if(station == NULL)
    {
        dPrint(DERROR,"找不到充电桩\n");
        return RTN_FAIL;
    }
    //开始充电之前先查一次能量
    SendPlcDataByDestMac(mac,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));
    vTaskDelay((1000 / portTICK_PERIOD_MS));
    //收到启动充电请求，就发送PLC指令启动充电
    SendPlcDataByDestMac(mac ,REMOTE_START_CHARGING_CMD,strlen(REMOTE_START_CHARGING_CMD));
    SendPlcDataByDestMac(mac ,REMOTE_START_TX_CMD,strlen(REMOTE_START_TX_CMD));

    //设置充电方式为手动启动（在发送指令后立即设置）
    station->chargingMethod = TRUE;

    dPrint(INFO,"接收到了开始充电指令 休眠5秒\n");
    //vTaskDelay((1000 / portTICK_PERIOD_MS)*12);
     for(int i = 0; i < 25; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
    }
    if(station->enumStatus != Charging && station->enumStatus != SuspendEvse)
    {
        dPrint(DERROR,"虽然启动充电了，但是充电桩不在充电中 mac:%s\n",station->mac);
        //启动失败，重置充电方式标志
        station->chargingMethod = FALSE;
        return RTN_FAIL;
    }
    dPrint(INFO,"开始充电成功了 mac:%s,chargingMethod:%d\n",mac,station->chargingMethod);
    return RTN_SUCCESS;
}
int StopCharging(char *mac,unsigned long long endTime)
{
    ChargingStation* station = GetChargingStation(mac);
    if(station == NULL)
    {
        dPrint(DERROR,"找不到充电桩\n");
        return RTN_FAIL;
    }
    //收到停止充电请求，就发送PLC指令停止充电
    SendPlcDataByDestMac(mac ,REMOTE_STOP_CHARGING_CMD,strlen(REMOTE_STOP_CHARGING_CMD));
    SendPlcDataByDestMac(mac ,REMOTE_STOP_TX_CMD,strlen(REMOTE_STOP_TX_CMD));
    station->chargingMethod = FALSE;
    //获取充电桩状态
    //vTaskDelay((1000 / portTICK_PERIOD_MS)*13);
    for(int i = 0; i < 25; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
    }
    //获取充电桩状态
    if(station->enumStatus == Charging)
    {
        dPrint(WARN,"虽然停止充电了，但是充电桩还在充电中,mac:%s\n",station->mac);
        return RTN_FAIL;
    }
    dPrint(INFO,"停止充电成功了 mac:%s,chargingMethod:%d\n",station->mac,station->chargingMethod);
    return RTN_SUCCESS;
}

void ChargingDurationHwTimer(HW_TIMER_ID timerId, void *arg, int argLen)
{
    dPrint(INFO,"ChargingDurationHwTimer callback: timerId=%d, argLen=%d\n", timerId, argLen);

    // 打印接收到的MAC地址（注意可能没有\0结尾）
    if(arg != NULL && argLen > 0)
    {
        char macBuf[32] = {0};
        memcpy(macBuf, arg, argLen < 31 ? argLen : 31);
        //dPrint(INFO,"Received MAC: %s (len=%d)\n", macBuf, argLen);
    }

    ChargingStation* station = GetChargingStation(arg);
    //只有是充电中才计时
    if(station != NULL && (station->enumStatus == Charging || station->enumStatus == SuspendEv))
    {
        station->duration++;
        //dPrint(INFO,"Duration updated: %d seconds, MAC: %s\n", station->duration, station->mac);
    }
    else
    {
      
        //dPrint(DERROR,"GetChargingStation returned NULL! arg=%p, argLen=%d\n", arg, argLen);
        
    }
    return;
}
/*
char* GetStationCmdByName(const char* cmdIdx)
{
    if(strncmp(cmdIdx,"set_curr_32A"))
    {
        return "AT+SEND=4080E1346353,34,7E0011000002010401010400000C8056CB\r\n";
    }
    else if(strncmp(cmdIdx,"set_curr_16A"))
    {
        return "AT+SEND=4080E1346353,34,7E0011000002010401010400000640A6CD\r\n";
    }
    else if(strncmp(cmdIdx,"set_curr_6A"))
    {
        return "AT+SEND=4080E1346353,34,7E00110000020104010104000002586CCF\r\n";
    }
    else if(strncmp(cmdIdx,"set_curr_0A"))
    {
        return "AT+SEND=4080E1346353,34,7E0011000002010401010400000000F6CF\r\n";
    }
    else if(strncmp(cmdIdx,"acVoltageL1"))
    {
        return  SELECT_CHARGING_STATION_L1_VOLTAGE;   
    }
    else if(strncmp(cmdIdx,"acCurrentL1"))
    {
        return  SELECT_CHARGING_STATION_L1_CURRENT;   
    }
    else if(strncmp(cmdIdx,"Power"))
    {
        return  SELECT_CHARGING_STATION_POWER;   
    }
    else if(strncmp(cmdIdx,"Energy"))
    {
        return  SELECT_CHARGING_STATION_ENERGY;   
    }
    else if(strncmp(cmdIdx,"StationStatus"))
    {
        return  SELECT_CHARGING_STATION_STATUS_CMD;   
    }
    else if(strncmp(cmdIdx,"errorCode"))
    {
        return  SELECT_CHARGING_STATION_ERROR_CODE_CMD;   
    }
    else if(strncmp(cmdIdx,"mdcFwVersion"))
    {
        return  SELECT_CHARGING_STATION_FIRMWARE;   
    }
    else if(strncmp(cmdIdx,"serialnumber"))
    {
        return  SELECT_CHARGING_STATION_SN;   
    }
    else
    {
        dPrint(DERROR,"未知的命令\n");
        return SELECT_CHARGING_STATION_SN;
    }
}
 */

 void CheckStationConnectTimerFunc(TIMER_ID timerId, char *arg, int argLen)
 {
    //dPrint(DEBUG,"检测在离线定时器\n");
    for(int i = 0;i<CHAEGING_STATION_MAX_NUM;i++)
    {
        if(FALSE == sg_chargingStationArray[i].isAdded)
        {
            //dPrint(DEBUG,"没有添加充电桩,不需要检测在离线mac:%s,subDevId:%s\n",sg_chargingStationArray[i].mac,sg_chargingStationArray[i].serialNum);
            continue;
        }
        sg_chargingStationArray[i].heart_beat_num--;
        if(sg_chargingStationArray[i].heart_beat_num<=0)
        {
            //设置为离线状态
            sg_chargingStationArray[i].connectStatus = SUB_DEVICE_CONNECT_STATUS_OFFLINE;
            dPrint(DEBUG,"充电桩mac:%s,离线状态,subDevId:%s\n",sg_chargingStationArray[i].mac,sg_chargingStationArray[i].serialNum);
            //离线之后，充电桩的数据应该清除掉
            sg_chargingStationArray[i].enumStatus = Poweron;
            memset(sg_chargingStationArray[i].EVStatus,0,sizeof(sg_chargingStationArray[i].EVStatus));
            strncpy(sg_chargingStationArray[i].EVStatus,"Poweron",strlen("Poweron"));
            sg_chargingStationArray[i].acVoltageL1 = 0;
            sg_chargingStationArray[i].acCurrentL1 = 0;
            sg_chargingStationArray[i].chargingMethod = FALSE;
            //sg_chargingStationArray[i].lastenumStatus = Poweron;
            //删除正在充电计时的定时器
            if(i == 0)
            {
                DelHwTimer(HW_TIMER_CHARGING_DURING_ID_0);   
            }
            else if(i==1)
            {
                DelHwTimer(HW_TIMER_CHARGING_DURING_ID_1);       
            }
            PublishEvent(EVENT_REPORT_STATUS, (char *)&sg_chargingStationArray[i], sizeof(ChargingStation));
        }
        else if(sg_chargingStationArray[i].heart_beat_num>0)
        {
            //设置在线状态
            //如果是从离线状态变为在线状态，需要主动查询一次充电桩的状态
            if(sg_chargingStationArray[i].connectStatus == SUB_DEVICE_CONNECT_STATUS_OFFLINE)
            {

                //一次查询充电桩的状态,先放入到队列
                SendPlcDataByDestMac(sg_chargingStationArray[sg_chargingStationCount].mac,SELECT_CHARGING_STATION_STATUS_CMD,strlen(SELECT_CHARGING_STATION_STATUS_CMD));
            }
            sg_chargingStationArray[i].connectStatus = SUB_DEVICE_CONNECT_STATUS_ONLINE;
            dPrint(DEBUG,"充电桩mac:%s,在线状态,subDevId:%s\n",sg_chargingStationArray[i].mac,sg_chargingStationArray[i].serialNum);
        }
        if(sg_chargingStationArray[i].heart_beat_num<0)
        {
            sg_chargingStationArray[i].heart_beat_num = 0;
        }
    }
    AddTimer(TIMER_ID_CEHCK_STATION_CONNECT_STATUS,100*10,CheckStationConnectTimerFunc);
 }
 //打印充电桩的所有数据
void PrintChargingStationData()
{
    for(int i = 0;i<sg_chargingStationCount;i++)
    {
        if(sg_chargingStationArray[i].isAdded == FALSE)
        {
            continue;
        }
        const char *connectStatus = (sg_chargingStationArray[i].connectStatus == SUB_DEVICE_CONNECT_STATUS_ONLINE) ? "online" : "offline";
        dPrint(DEBUG,"第[%d]个充电桩\n mac:%s\n,序列号:%s\n,版本:%s\n,"
            "name:%s\n,状态枚举:%d\n,连接:%s\n,状态:%s\n,电压:%d\n,电流:%d\n,"
            "能量wh:%f\n,是否插枪:%d\n,充电枪是否准备好:%d\n,汽车准备:%d\n,自动控制下发的电流:%d\n,"
            "开始充电时间:%llu\n,结束充电时间:%llu\n,充电时长:%d\n,"
            "心跳次数:%d\n",
            i,
            sg_chargingStationArray[i].mac,
            sg_chargingStationArray[i].serialNum,
            sg_chargingStationArray[i].mdcFwVersion,
            sg_chargingStationArray[i].name,
            sg_chargingStationArray[i].enumStatus,
            connectStatus,
            sg_chargingStationArray[i].EVStatus,
            sg_chargingStationArray[i].acVoltageL1,
            sg_chargingStationArray[i].acCurrentL1,
            sg_chargingStationArray[i].energy,
            sg_chargingStationArray[i].isPlugged,
            sg_chargingStationArray[i].isEvseReady,
            sg_chargingStationArray[i].isEvReady,
            sg_chargingStationArray[i].limitCurrent,
            sg_chargingStationArray[i].startTime,
            sg_chargingStationArray[i].endTime,
            sg_chargingStationArray[i].duration,
            sg_chargingStationArray[i].heart_beat_num
        );

    }
}

void chargingWorkMode(char *mac, char *workmode)
{
    if (mac == NULL || workmode == NULL)
    {
        dPrint(INFO, "chargingWorkMode error;\n");
        return;
    }

    char modeCmd[128] = {0};
    const char *modeCode = NULL;
    
    // 确定工作模式代码
    if (strncmp(workmode, "app", strlen("app")) == 0)
    {
        modeCode = "010000";
    }
    else if (strncmp(workmode, "ocpp", strlen("ocpp")) == 0)
    {
        modeCode = "020000";
    }
    else if (strncmp(workmode, "plc", strlen("plc")) == 0)
    {
        modeCode = "030000";
    }
    else
    {
        dPrint(INFO, "Unknown work mode: %s\n", workmode);
        return;
    }

    // 一次性构建完整命令
    int ret = snprintf(modeCmd, sizeof(modeCmd), 
                       "AT+SEND=%s,29,7E000E0000020101010E01%s>\r\n", 
                       mac, modeCode);
    
    // 检查是否截断
    if (ret < 0 || ret >= sizeof(modeCmd))
    {
        dPrint(INFO, "Command buffer too small or format error\n");
        return;
    }

    dPrint(INFO, "工作模式设置 cmd = %s\n", modeCmd);
    
    int sendRet = SendPlcDataByDestMac(mac, modeCmd, strlen(modeCmd));
    if (sendRet == RTN_SUCCESS) 
    {
        dPrint(INFO, "send plc cmd ok\n");
    }
    else
    {
        dPrint(INFO, "send plc cmd failed: %d\n", sendRet);
    }
}