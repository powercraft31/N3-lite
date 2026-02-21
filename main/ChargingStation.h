#ifndef __CHARGE_STATION_H__
#define __CHARGE_STATION_H__
#include <stdio.h>
#include "types.h"

//定义可以搜索到的充电桩数量
#define SEARCH_STATION_MAX_NUM    10
//支持内存中存储的最大充电桩数量
#define CHAEGING_STATION_MAX_NUM    2
//心跳次数
#define SUB_DEVICE_DEFAULT_HEART_BEAT_NUM	4
//充电桩状态枚举
typedef enum EnumStationStatus
{
		Poweron = 0,
		Availiable,
		Preparing,
		Charging,
		Finish,
		SuspendEv,
		SuspendEvse,
		Reserved,
		Unavailable,
		Faulted
}EnumStationStatus;

typedef enum EnumStationWorkMode
{
		App = 1,
		Ocpp,
		Plc,
        Undefine
}EnumStationWorkMode;

typedef enum EnumConnectStatus
{
    SUB_DEVICE_CONNECT_STATUS_OFFLINE = 0,
    SUB_DEVICE_CONNECT_STATUS_ONLINE
}EnumConnectStatus;
//搜索到的充电桩结构体
typedef struct SearchStation{
    char mac[20];                       //mac地址
    char mdcFwVersion[12];              //版本信息
    char serialNum[64];                 //充电桩序列号
    int snr;							//信噪比
    int atten;							//信号强度
    BOOL isAdded;
    int maxlimitCurrent;                //最大限制电流
}__attribute__((packed)) SearchStation;

//充电桩实体结构体
typedef struct ChargingStation{
    char mac[20];                       //mac地址
    char mdcFwVersion[12];              //版本信息
    char serialNum[64];                 //充电桩序列号
    char name[64];                      //充电桩名称
    EnumStationStatus enumStatus;       //充电桩状态枚举,一个状态对应一个EVStatus字符串
    EnumConnectStatus connectStatus;    //充电桩在线离线状态
    char phase;                         //充电桩相位
    char EVStatus[12];                  //充电桩状态字符串
    unsigned short acVoltageL1;         //电表电压0.1v
    unsigned short acCurrentL1;         //电表电流0.1A
    float lastEnergy;                   //上次获取到的能量w/h
    float energy;                        //能量w/h
    int snr;							//信噪比
    int atten;							//信号强度
    int isPlugged;                     //是否插枪，       1:插枪 0:未插枪      -1:未知状态
    int isEvseReady;                   //充电枪是否准备好 1:准备好,0:未准备好   -1:未知状态
    int isEvReady;                     //汽车是否准备好   1::准备好,0:未准备好  -1:未知状态
    EnumStationWorkMode workMode;                    //充电桩工作模式

    int limitCurrent;                  //自动控制模块设置的限制电流
    int maxlimitCurrent;                //最大限制电流
    BOOL isAdded;                       //是否已经添加过充电桩   TRUE:已经添加过，FALSE:未添加
    unsigned long long startTime;       //开始时间（毫秒时间戳，需要64位）
    unsigned long long endTime;         //结束时间（毫秒时间戳，需要64位）
    int duration;                       //充电时长,单位秒
    int heart_beat_num;                 //心跳次数
    BOOL chargingMethod;                //0-iCharge, 1-Charger
    EnumStationStatus lastenumStatus;       //上一次的充电桩状态
    BOOL isStartTimeCalibrated;         //开始充电时系统时间是否已校准
}__attribute__((packed)) ChargingStation;

//获取充电桩vender
#define SELECT_CHARGING_STATION_VENDOR "AT+SEND=4080E1346353,25,7E000C00000101010103E13F>\r\n" 		//vender
//获取充电桩mode
#define SELECT_CHARGING_STATION_MODE "AT+SEND=4080E1346353,25,7E000C00000101010104237E>\r\n" 		//mode
//获取充电桩sn序列号
#define SELECT_CHARGING_STATION_SN "AT+SEND=4080E1346353,25,7E000C0000010101010221FE>\r\n" 		//sn

//获取充电桩firmware version
#define SELECT_CHARGING_STATION_FIRMWARE "AT+SEND=4080E1346353,25,7E000C0000010101010120BE>\r\n"       //firmware version



//设置限制电流
#define UPDATE_CHARGING_STATION_LIMIT_CURRENT "AT+SEND=4080E1346353,35,7E0011000002010401010400000C8056CB>\r\n"  //设置限制电流
//AT+SEND=4080E1346353,17,7E0011 000002 0104010104 00000C80 56CB,0 // 设置限制电流 00000C80 这4个字节是电流值,位置是第46个字节

//开启充电连续输入以下两条命令
#define REMOTE_START_CHARGING_CMD  "AT+SEND=4080E1346353,29,7E000E000002010401140106E4BE>\r\n"   //RemoteStart
#define REMOTE_START_TX_CMD        "AT+SEND=4080E1346353,29,7E000E000002010401140108203F>\r\n"   //startTx

//关闭充电连续输入以下两条命令
#define REMOTE_STOP_CHARGING_CMD   "AT+SEND=4080E1346353,29,7E000E000002010401140107247F>\r\n"  //RemoteStop
#define REMOTE_STOP_TX_CMD         "AT+SEND=4080E1346353,29,7E000E000002010401140109E0FE>r\n"  //StopTx

#define SELECT_CHARGING_STATION_L1_VOLTAGE "AT+SEND=4080E1346353,25,7E000C00000101040106E3EF>\r\n"              //查询L1相电压
//AT+SEND=4080E1346353,12,7E000C00000101040107232E,0              //查询L2相电压
//AT+SEND=4080E1346353,12,7E000C00000101040108276E,0              //查询L3相电压


#define SELECT_CHARGING_STATION_L1_CURRENT "AT+SEND=4080E1346353,25,7E000C00000101040109E7AF>\r\n"             //查询L1相电流
//AT+SEND=4080E1346353,12,7E000C0000010104010AE6EF ,0 //查询L2相电流
//AT+SEND=4080E1346353,12,7E000C0000010104010B262E,0 //查询L3相电流

//获取充电桩energy 单位wh 功耗
#define SELECT_CHARGING_STATION_ENERGY      "AT+SEND=4080E1346353,25,7E000C0000010104010D24AE>\r\n"  		        //获取energy  单位wh 功耗
//查询充电桩功率
#define SELECT_CHARGING_STATION_POWER       "AT+SEND=4080E1346353,25,7E000C0000010104010CE46F>\r\n"
//一次性查询ev evse plugged
#define SELECT_CHARGING_STATION_STATUS_CMD  "AT+SEND=4080E1346353,37,7E00120000010304010E04020F040310F0DE>\r\n" //一次性查询ev evse plugged
//一次性查询电压电流
#define SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT "AT+SEND=4080E1346353,31,7E000F0000010204010604020926C4>\r\n" //查询L1 电压 电流
//一次性查询电压电流能量
#define SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY "AT+SEND=4080E1346353,37,7E00120000010304010D040206040309A62F>\r\n" 
//查询错误码
#define SELECT_CHARGING_STATION_ERROR_CODE_CMD      "AT+SEND=4080E1346353,25,7E000C00000101040111EDAF>\r\n"
//查询当前充电桩工作模式
#define SELECT_CHRGING_WORK_MODE_CMD   "AT+SEND=4080E1346353,25,7E000C0000010101010E0000>\r\n"


/*
//获取充电桩vender
#define SELECT_CHARGING_STATION_VENDOR "AT+SEND=4080E1346353,12,7E000C00000101010103E13F,0\r\n" 		//vender
//获取充电桩mode
#define SELECT_CHARGING_STATION_MODE "AT+SEND=4080E1346353,12,7E000C00000101010104237E,0\r\n" 		//mode
//获取充电桩sn序列号
#define SELECT_CHARGING_STATION_SN "AT+SEND=4080E1346353,12,7E000C00000101010105E3BF,0\r\n" 		//sn

//获取充电桩firmware version
#define SELECT_CHARGING_STATION_FIRMWARE "AT+SEND=4080E1346353,12,7E000C0000010101010120BE,0\r\n"       //firmware version



//设置限制电流
#define UPDATE_CHARGING_STATION_LIMIT_CURRENT "AT+SEND=4080E1346353,17,7E0011000002010401010400000C8056CB,0\r\n"  //设置限制电流
//AT+SEND=4080E1346353,17,7E0011 000002 0104010104 00000C80 56CB,0 // 设置限制电流 00000C80 这4个字节是电流值,位置是第46个字节

//开启充电连续输入以下两条命令
#define REMOTE_START_CHARGING_CMD  "AT+SEND=4080E1346353,14,7E000E000002010401140106E4BE,0\r\n"   //RemoteStart
#define REMOTE_START_TX_CMD        "AT+SEND=4080E1346353,14,7E000E000002010401140108203F,0\r\n"   //startTx

//关闭充电连续输入以下两条命令
#define REMOTE_STOP_CHARGING_CMD   "AT+SEND=4080E1346353,14,7E000E000002010401140107247F,0\r\n"  //RemoteStop
#define REMOTE_STOP_TX_CMD         "AT+SEND=4080E1346353,14,7E000E000002010401140109E0FE,0\r\n"  //StopTx

#define SELECT_CHARGING_STATION_L1_VOLTAGE "AT+SEND=4080E1346353,12,7E000C00000101040106E3EF,0\r\n"              //查询L1相电压
//AT+SEND=4080E1346353,12,7E000C00000101040107232E,0              //查询L2相电压
//AT+SEND=4080E1346353,12,7E000C00000101040108276E,0              //查询L3相电压

//设置充电桩工作模式
//AT+SEND=4080E1346353,29,7E000E0000020101010E01010000>\r\n


#define SELECT_CHARGING_STATION_L1_CURRENT "AT+SEND=4080E1346353,12,7E000C00000101040109E7AF,0\r\n"             //查询L1相电流
//AT+SEND=4080E1346353,12,7E000C0000010104010AE6EF ,0 //查询L2相电流
//AT+SEND=4080E1346353,12,7E000C0000010104010B262E,0 //查询L3相电流

//获取充电桩energy 单位wh 功耗
#define SELECT_CHARGING_STATION_ENERGY      "AT+SEND=4080E1346353,12,7E000C0000010104010D24AE,0\r\n"  		        //获取energy  单位wh 功耗
//一次性查询ev evse plugged
#define SELECT_CHARGING_STATION_STATUS_CMD  "AT+SEND=4080E1346353,18,7E00120000010304010E04020F040310F0DE,0\r\n" //一次性查询ev evse plugged
//一次性查询电压电流
#define SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT "AT+SEND=4080E1346353,15,7E000F0000010204010604020926C4,0\r\n" //查询L1 电压 电流
*/

//PDU整体数据结构
typedef struct PDU_HEAD
{
    unsigned char sig;      //标识符
    unsigned short len;     //全部数据的长度
    unsigned char sn;       //序列
    unsigned char type;     //类型
    unsigned char cmd;      //命令ID
}__attribute__((packed)) PDU_HEAD;

//PDU数据内部数据包,带数量
typedef struct PDU_DATA
{
    unsigned char count;	    //组件数量   
    unsigned char component;    //组件标识
    unsigned char instance;	    //实例
    unsigned char id;	        //id
    unsigned char result;       //结果
    unsigned char length;	    //长度
}__attribute__((packed)) PDU_DATA;

//PDU数据内部数据包，不带数量
typedef struct PDU_DATA_HEAD
{
    unsigned char component;    //组件标识
    unsigned char instance;	    //实例
    unsigned char id;	        //id
    unsigned char result;       //结果
    unsigned char length;	    //长度
}__attribute__((packed)) PDU_DATA_HEAD;

//PDU数据内部数据包,状态通知用这个
typedef struct PDU_DATA_STATUS_NOTIFY
{
    unsigned char count;	    //组件数量   
    unsigned char component;    //组件标识
    unsigned char instance;	    //实例
    unsigned char id;	        //id
    unsigned char length;	    //长度
}__attribute__((packed)) PDU_DATA_STATUS_NOTIFY;
//PLC应用层协议定义
typedef enum PLC_PROTOCOL_CMD
{
		Heartbeat = 0x00,
		GetVariables = 0x01,
		SetVariables = 0x02,
		StatusNotify = 0x03,
		Transmission = 0x04,
		OtaInitial = 0x05,
		OtaData = 0x06	
}PLC_PROTOCOL_CMD;

//Component
typedef enum PDU_DATA_Component
{
    Basic_Component = 0x01,
    Connector_Component = 0x04
}PDU_DATA_Component;

//PDU_DATA_ID
typedef enum PDU_DATA_ID
{
    PDU_DATA_ID_mdcFwVersion = 0x01,        //版本
    PDU_DATA_ID_serialNumber = 0x02,        //序列号
    PDU_DATA_ID_vendor = 0x03,              //品牌
    PDU_DATA_ID_model = 0x04,               //商家
    PDU_DATA_ID_meterSerialNumber = 0x05,   //电表序列号
    PDU_DATA_ID_meterType = 0x06,           //电表类型
    PDU_DATA_ID_numberOfConnectors = 0x07,  //枪的个数
    PDU_DATA_ID_numberPhases = 0x08,        //几相
    PDU_DATA_ID_workMode = 0x0E,            //工作模式

    PDU_DATA_ID_acVoltageL1 = 0x06,     //电压
    PDU_DATA_ID_acCurrentL1 = 0x09,     //电流
    PDU_DATA_ID_energy = 0x0d,          //能量
    PDU_DATA_ID_isPlugged = 0x0e,       //是否插枪
    PDU_DATA_ID_isEvseReady = 0x0f,     //充电枪是否准备好
    PDU_DATA_ID_isEvReady = 0x10        //汽车是否准备好
}PDU_DATA_ID;

#endif
