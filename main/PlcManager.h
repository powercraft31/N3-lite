#ifndef __PLC_MANAGER_H__
#define __PLC_MANAGER_H__
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "types.h"

typedef void (*PATPlcDataProFunc)(char *nvMac,char *hexPdu, int outlen);
/********************************************************
*@Function name:PlcInit
*@Description:PLC初始化,ChargeStation调用
*@Return:
********************************************************************************/
int PlcInit();
/********************************************************
*@Function name:SuspendAllPlcTaskThread
*@Description:暂停所有PLC任务线程
*@Return:
********************************************************************************/
void SuspendAllPlcTaskThread();

/********************************************************
*@Function name:ResumeAllPlcTaskThread
*@Description:恢复所有PLC任务线程
*@Return:
********************************************************************************/
void ResumeAllPlcTaskThread();

/********************************************************
*@Function name:IsRuningPlcTaskThread
*@Description:判断PLC任务线程是否正在运行
*@Return:TRUE FALSE
********************************************************************************/
BOOL IsRuningPlcTaskThread();

/********************************************************
*@Function name:
*@Description:注册接收充电桩数据的函数
*@Return:
********************************************************************************/
void RegChargeStationDataFunc(PATPlcDataProFunc pfnDataFunc);

/********************************************************
*@Function name:SendPlcDataByDestMac
*@Description:根据Mac地址发送PLC数据
*@input param:destMac 要发送的目标mac地址
*@input param:pData 要发送的完整数据ChargingStation.h里面的宏定义
*@input param:len pData的长度
*@Return:
********************************************************************************/
int SendPlcDataByDestMac(char* destMac ,char *pData,int len);
/********************************************************
*@Function name:SendPlcData
*@Description:发送完整的指令数据过去,主要用于发送AT指令
*@input param:pData 要发送的完整指令
*@input param:len pData的长度
*@Return:
********************************************************************************/
int SendPlcATData(char *pData,int len);

/********************************************************
*@Function name:MonitorThreadFunc
*@Description:监控和处理线程
*@Return:
********************************************************************************/
void PlcMonitorThreadFunc(void *pvParameter);

/********************************************************
*@Function name:PlcSendDataThread
*@Description:PLC数据发送线程
*@Return:
********************************************************************************/
void PlcSendDataThread(void *pvParameter);

//处理PLC模组回复的系统启动通知等内置系统命令
int ProcessPlcSysCmdFunc(char *pchPackData, int iPackLen);
/*
*回调各个业务层回调函数
*devSeriStr :串口设备文件名称
* pchPackData :数据
*iPackLen 数据长度
*return true 成功 false 失败
*/	
int ProcessDataFunc(char *pchPackData, int iPackLen);

/********************************************************
*@Function name:AnalyseStringDataToHex
*@Description:解析出16进制协议数据，把AT报文头部信息全部砍掉
*@Return:
********************************************************************************/
//int AnalyseStringDataToHex(char *request,char *outHex,unsigned int *outLen);

#endif
