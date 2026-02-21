#ifndef __PLC_MODULE_3121_H__
#define __PLC_MODULE_3121_H__
#include "PlcAtcmd3121.h"
#include "serial.h"
#include "ChargingStation.h"


#define RECEIVE_BUF_WAIT_2S             2000  // 2秒 = 2000毫秒
#define RECEIVE_BUF_WAIT_1S             1000  // 1秒 = 1000毫秒

#define RECEIVE_BUF_WAIT_100MS          100  // 100ms

#define MAX_RECEI_BUFF_LEN 	512 		//20160919 160->512

typedef struct SerialConfig
{
	char achSrlPtNm[0x80];             // 串口名
	int iBaud;                          // 串口波特率
	unsigned char dataBit;              //数据位 默认8
	/*
	设备端的串口校验位,有n,o,e,s几个选项
	n: 无奇偶校验位。
	o: 设置为奇校验
	e: 设置为偶校验
	s: 设置为空格
	*/
	unsigned char checkBit;//校验位 
	unsigned char stopBit;  //默认1，代表1个停止位
	unsigned char flow;	//流控，默认0禁止，1开启流控
}SerialConfig_T;

//启动AT串口
int StartAtSerial();
//关闭串口连接
int StopAtSerial();
//重启串口
int RestartAtSerial();
//获取AT 串口文件描述符
uart_port_t GetConnectFd();
/*
*pchSendData :IN 发送的数据缓冲区指针,有外部申请内存的。
*iSendLen:要发送的数据长度
*/
int SendPlcData(char *pchSendData, int iSendLen); 
/*
*接收数据到缓冲区
return true 成功false 失败
*/	
int SendDataWithReadResult(char *send_buf, int sendlen, char *rcv_buf, int recvLen, int rcv_wait);
/********************************************************
*@Function name:RecvPlcData
*@Description:获取PLC的数据
*@input param:buffer缓冲区
*@input param:length 缓冲区长度
*@Return:返回获取到的实际数据长度
********************************************************************************/
int RecvPlcData(uint8_t *buffer, size_t length);
//获取整包数据
//int GetPackData(char *pchPackData, int iPackBufSize);
//打开回显
int OpenECHO(void);
//关闭回显
int CloseECHO(void);
//查询工作模式
const char* QueryAtMode(void);
//进入AT 配置模式
int EnterAtMode(void);
//设置默认AT 配置模式
int SetAtMode(void);
//退出AT 配置模式
int EXITAtMode(void);
//配置透传模式
int DataMode(void);
//查询PLC串口信息
SerialConfig_T QueryPlcSerialConfig();
//配置PLC 波特率
int ConfigPlcBaud(SerialConfig_T* serilaConfig);
//重启PLC
int RebootPlc(void);
//获取PLC电网的节点数量，包含CCO节点
int GetPlcNodeCount();

//查询白名单状态
int QueryWhiteStatus();
//开启白名单
int EnableWhiteList();
//关闭白名单
int DisableWhiteList();

//清除白名单
int ClearWhiteList();
//查询自身MAC地址
const char* GetNvMac();
//从内存中直接获取mac地址，只有测试工具在使用
extern const char* GetSelfMac();
//判断是否是系统指令回复
BOOL IsPlcSysCmdAck(const char* request);
//查询是否开启入网通知
int QueryNotifyStatus();
//开启入网通知
int EnableNotify();

/********************************************************
*@Function name:GetMACadress
*@Description:获取所有充电桩的mac地址信噪比/信号强度等信息,外部提供字符串数组内存
*@output param:rcv_buf 接收的全部数据
*@input/output param:stations 存放mac地址
*@Return:返回有几个充电桩，失败返回-1
********************************************************************************/
int GetMACadress(char *rcv_buf,SearchStation *stationArray);


/********************************************************
*@Function name:GetChargingStationTopoInfo
*@Description:获取信号强度/信噪比等信息
*@input param:topinfo 单个+TOPOINFO原始字符串
*@input/output param:stations 存放信号强度/信噪比等信息
*@input param:index 索引
*@Return:返回
********************************************************************************/
int GetChargingStationTopoInfo(char *topinfo,SearchStation *stationArray,int index);

#endif
