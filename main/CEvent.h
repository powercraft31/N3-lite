#ifndef __CEVENT_H__
#define __CEVENT_H__

#include <stdio.h>

typedef enum EVENT_TYPE
{
	EVENT_USART2_RX_COMPLETE,   		// USART2接收完成事件

	EVENT_REPORT_STATUS,      			//充电桩主动上报状态事件
	EVENT_REMOTE_START,           		//服务器远程启动充电事件
	EVENT_REMOTE_STOP,            		//服务器远程停止充电事件
	EVENT_AUTO_SUSPEND,              	//自动控制模块暂停事件
	EVENT_AUTO_START,					//自动控制模块重新启动充电
	EVENT_AUTO_SET_LIMIT_CUUR,			//自动控制模块设置限制电流
	EVENT_PLC_SEND_RECV_DATA,			//PLC发送接收数据事件
	EVENT_AUTO_CONTROL_MONITOR,			//自动控制监控事件
	EVENT_MAX
}EVENT_TYPE;

typedef void (*eventFuncProc)(EVENT_TYPE event,char *,int); 

typedef struct CEVENT_STRUCT
{
		EVENT_TYPE event;
		eventFuncProc func;
}CEVENT_STRUCT;
	
/********************************************************
	*@Function name:SubscribeEvent
	*@Description:订阅关心的事件
	*@input param:事件名称
	*@input param:传入的函数指针，有事件触发会自动回调
	*@Return:
********************************************************************************/
void SubscribeEvent(EVENT_TYPE event,eventFuncProc func);
/********************************************************
	*@Function name:PublishEvent
	*@Description:发布事件
	*@input param:事件名称
	*@input param:具体数据
	*@input param:长度
	*@Return:
********************************************************************************/
void PublishEvent(EVENT_TYPE event,char* data, int len);


#endif

