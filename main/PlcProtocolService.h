#ifndef __PLC_PROTOCOL_SERVICE_H__
#define __PLC_PROTOCOL_SERVICE_H__

#include <stdio.h>
#include "ChargingStation.h"
#include "CEvent.h"
#include "CTimer.h"

/********************************************************
*@Function name:处理充电桩主动上报的充电桩回复的数据
*@Description:ProcessHexPduPacket,
*@output param:
*@Return:有几个充电桩,-1失败
********************************************************************************/
void ProcessHexPduPacket(char *nvMac,char *hexPduPacket, int outlen);

/********************************************************
*@Function name:ProcessChargeStationStatusNotify
*@Description:处理充电桩的状态通知回复
*@input param:PDUData 
*@input param:station 充电桩对象
*@input param:value   数值
*@Return:
********************************************************************************/
int ProcessChargeStationStatusNotify(PDU_DATA_STATUS_NOTIFY *PDUData,ChargingStation *station,int value);

/********************************************************
*@Function name:
*@Description:组合充电桩指令
*@input param:mac 充电桩mac地址
*@input param:srcCmd 源指令
*@output param:destCmd 组合后的指令
*@Return:
********************************************************************************/
int CombineMacAndCmd(char *mac,char *srcCmd,char *destCmd);

/********************************************************
*@Function name:GetChargingStationSerialNumber
*@Description:获取充电桩序列号
*@input param:station 充电桩对象
*@Return:
********************************************************************************/
//int GetChargingStationSerialNumber(char *mac,ChargingStation *station);
/********************************************************
*@Function name:GetChargingStationFirmwareVersion
*@Description:获取充电桩版本号
*@input param:station 充电桩对象
*@Return:
********************************************************************************/
//int GetChargingStationFirmwareVersion(char *mac,ChargingStation *station);


#endif
