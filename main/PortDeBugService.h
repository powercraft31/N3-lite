#ifndef __PORT_DEBUG_SERVICE_H__
#define __PORT_DEBUG_SERVICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CEvent.h"

void uart0_interaction_task(void *pvParameters);

void handle_user_command(char *command_line); 
// AT
void debug_at_cmd_process(const char *cmd);

void debug_at_cmd_send_response(const char *response);

void HandleDebugSerialData(char data);


//PLC发送和接收回调
void PlcDeBugDataCallBack(EVENT_TYPE event,char *arg,int len); 
//自动控制监控回调
void AutoControllMonitorCallBack(EVENT_TYPE event,char *arg,int len); 

#endif

