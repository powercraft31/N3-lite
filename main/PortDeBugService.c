#include "PortDeBugService.h"
#include "BL0942Meter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "PlcRecvQueue.h"
#include "StringUtils.h"
#include "ConfigManager.h"
#include "DeBug.h"
#include "PlcModule3121.h"
#include "RPCServer.h"
#include "GPIOManager.h"
#include "PlcManager.h"
#include "ChargingStationManager.h"

// AT
#define AT_CMD_BUFFER_SIZE 128
//是否开始监控PLC指令标志
static BOOL isStartMonitorPlc = FALSE;
static BOOL isStartMonitorAuto = FALSE;
/**
 * @brief 循环从 UART0 (stdin) 读取数据。
 */
void uart0_interaction_task(void *pvParameters) {
    char command_buffer[AT_CMD_BUFFER_SIZE];

    while (1) {
        // 阻塞等待
        if (fgets(command_buffer, AT_CMD_BUFFER_SIZE, stdin) != NULL) {
            handle_user_command(command_buffer);
        } else {
            //读取失败
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void handle_user_command(char *command_line) {
    command_line[strcspn(command_line, "\n\r")] = 0;
    debug_at_cmd_process((const char *)command_line); 

}

/*
void HandleDebugSerialData(char data)
{

    if(at_cmd_index < AT_CMD_BUFFER_SIZE - 1) {
        at_cmd_buffer[at_cmd_index++] = data;

        // 检查是否收到AT命令终止符（\r\n）
        if(at_cmd_index >= 2) 
				{
            //if(at_cmd_buffer[at_cmd_index - 2] == '\r' &&
            //   at_cmd_buffer[at_cmd_index - 1] == '\n') {
					  if(at_cmd_buffer[at_cmd_index - 1] == '\n') {
                // ????????(??\n)
                at_cmd_buffer[at_cmd_index - 2] = '\0';

                // ??AT??
                debug_at_cmd_process((const char *)at_cmd_buffer);

                // ?????
                at_cmd_index = 0;
                memset(at_cmd_buffer, 0, AT_CMD_BUFFER_SIZE);
            }
        }
    } 
		else 
		{
        // ?????,??
        at_cmd_index = 0;
        memset(at_cmd_buffer, 0, AT_CMD_BUFFER_SIZE);
        debug_at_cmd_send_response("ERROR: Buffer overflow\r\n");
    }
}
*/

/*!
    \brief      ??AT????
    \param[in]  response: ?????
    \param[out] none
    \retval     none
*/
void debug_at_cmd_send_response(const char *response)
{
    printf("%s", response);
}

/*!
    \brief      ??AT??
    \param[in]  cmd: AT?????
    \param[out] none
    \retval     none
*/
void debug_at_cmd_process(const char *cmd)
{
    //DisableEasyLog();
    // ?????
    if(cmd == NULL || strlen(cmd) == 0) 
    {
        //EnableEasyLog();
        return;
    }
    // AT+HELP? - ???????AT??
    if(strcmp(cmd, "AT+HELP?") == 0) {
        debug_at_cmd_send_response("\r\n=== Supported AT Commands ===\r\n");
        debug_at_cmd_send_response("AT+HELP?       - Show all supported AT commands\r\n");
        debug_at_cmd_send_response("AT+VER?        - Get firmware version\r\n");
        debug_at_cmd_send_response("AT+RESET       - Reset the system\r\n");
        debug_at_cmd_send_response("AT+STATUS?     - Get system status\r\n");
        debug_at_cmd_send_response("AT+METER?      - Get meter information\r\n");
        debug_at_cmd_send_response("==============================\r\n");
        debug_at_cmd_send_response("ok\r\n");
    }
    // AT+VER? - ??????
    else if(strcmp(cmd, "AT+VER?") == 0) {
        debug_at_cmd_send_response("\r\nFirmware Version: V1.0.0\r\n");
        debug_at_cmd_send_response("Build Date: " __DATE__ " " __TIME__ "\r\n");
        debug_at_cmd_send_response("ok\r\n");
    }
    // AT+RESET - ????
    else if(strcmp(cmd, "AT+RESET") == 0) {
        debug_at_cmd_send_response("\r\nSystem will reset in 1 second...\r\n");
        debug_at_cmd_send_response("ok\r\n");
        //delay_1ms(1000);
        //NVIC_SystemReset();
    }
    // AT+STATUS? - ??????
    else if(strcmp(cmd, "AT+STATUS?") == 0) {
        debug_at_cmd_send_response("\r\n=== System Status ===\r\n");
        debug_at_cmd_send_response("MCU: ESP32-D0WDR2-V3\r\n");
        debug_at_cmd_send_response("Status: Running\r\n");
        debug_at_cmd_send_response("=====================\r\n");
        debug_at_cmd_send_response("ok\r\n");
    }
    // AT+METER? - 获取电表数据
    else if(strcmp(cmd, "AT+METER?") == 0) 
    {
        BL0942_Data_t meter_data;
        char response[512] = {0};
        //debug_at_cmd_send_response("\r\n=== BL0942 Meter Data ===\r\n");
        // 获取 BL0942 电表数据
        if (bl0942_get_data(&meter_data)) 
        {
            // 输出计算后的值（使用参考系数，实际使用需要校准）
            float ct_current = meter_data.current;
            float ct_voltage = meter_data.voltage;
            float power = meter_data.power;
            float energy = meter_data.energy;
            float frequency = meter_data.frequency;

            sprintf(response,"%s%f%s%f%s%f%s%f%s%f%s","+METER={\"ct_current\":\"",ct_current,"\",\"ct_voltage\":\"",ct_voltage,"\",\"power\":\"",power,"\",\"energy\":\"",energy,"\",\"frequency\":\"",frequency,"\"}\r\n");
            debug_at_cmd_send_response(response);
        }
    }
    // AT - ????
    else if(strcmp(cmd, "AT") == 0)
    {
        debug_at_cmd_send_response("\r\nok\r\n");
    }
    else if(strstr(cmd, "AT+RECV") != NULL)
    {
        //把指令放入到队列中,在函数之前去掉了\r\n,现在应该加上去
        strcat((char *)cmd,"\r\n");
        debug_at_cmd_send_response("PutCCirQueueData cmd\r\n");
        PutPlcRecvQueueData((char *)cmd,strlen(cmd));
    }
    else if(strstr(cmd, "AT+N3LiteSN=") != NULL)
    {
        //写入N3Lite序列号,先用=号分割出两部分，然后写入配置
        char part1[25] = {0};
        char part2[64] = {0};
        SplitTwoString(cmd,"=", part1, part2);
        //序列号写入配置
        ConfigStatus_t status = SetConfigString("subDevId",part2);
        if(status == CONFIG_OK)
        {

            memset(part1,0,sizeof(part1));
            //先把序列号拷贝到part1
            strncpy(part1,part2,sizeof(part1));
            memset(part2,0,sizeof(part2));
            
            snprintf(part2,sizeof(part2),"%s%s%s","+N3LiteSN=",part1,"\r\n");
            debug_at_cmd_send_response(part2);
        }
        else
        {
            debug_at_cmd_send_response("err\r\n");
        }
    }
    else if(strcmp(cmd, "AT+N3LiteSN?")  == 0)
    {
        //获取N3Lite的序列号,返回N3lite的序列号
        char response[64] = {0};
        char subDevId[25] = {0};
        
        ConfigStatus_t status = GetConfigString("subDevId", subDevId, sizeof(subDevId));
        if(status == CONFIG_OK)
        {
            snprintf(response,sizeof(response),"%s%s%s","+N3LiteSN=",subDevId,"\r\n");
            debug_at_cmd_send_response(response);
        }
    }
    else if(strcmp(cmd, "AT+MAC")  == 0)
    {
        //从内存获取自身的mac地址，返回回去
        char response[64] = {0};
        snprintf(response,sizeof(response),"%s%s%s","+PLCMAC=",GetSelfMac(),"\r\n");
        debug_at_cmd_send_response(response);
    }
    else if(strstr(cmd, "AT+MAC=") != NULL)
    {
        //修改PLC序列号,发送给PLC模组
        SendPlcATData((char *)cmd,strlen(cmd));

    }
	else if(strcmp(cmd, "AT+TOPOINFO=0,99")  == 0)
    {
    	SuspendAllPlcTaskThread();
		
        //查询所有的子节点
        char AtcmdStr[64] = {0};
		char rcv_buf[256] = {0};
		int iRet = 0;
		strncpy(AtcmdStr,"AT+TOPOINFO=0,99\r\n",sizeof(AtcmdStr));
		iRet = SendDataWithReadResult(AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf), RECEIVE_BUF_WAIT_1S);
        //接收到数据了返回
        //为了方便调试，先返回发送的数据
        if(iRet >0)
        {
            rcv_buf[iRet] = 0;
            debug_at_cmd_send_response(rcv_buf);
        }
        else
        {
            debug_at_cmd_send_response("error");
        }
		
		ResumeAllPlcTaskThread();

    }
    else if(strcmp(cmd, "AT+METER_STATUS")  == 0)
    {
        char response[64] = {0};
        BL0942_Data_t meter_data;
        bl0942_get_data(&meter_data);
        if(meter_data.isConnected)
        {
            snprintf(response,sizeof(response),"%s%s%s","+METER_STATUS=","connected","\r\n");
            debug_at_cmd_send_response("+ok=connected\r\n");
        }
        else
        {
            debug_at_cmd_send_response("+err\r\n");
        }
    }
    else if(strcmp(cmd, "AT+InflowMaxCurrent")  == 0)
    {
        //获取家庭最大流入电流
        char response[64] = {0};
        uint8_t inflowMaxCurrent = GPIOManager_GetInletCurrent();
        snprintf(response,sizeof(response),"%s%d%s","+InflowMaxCurrent=",inflowMaxCurrent,"\r\n");
        debug_at_cmd_send_response(response);
    }
    else if(strstr(cmd, "AT+InflowMaxCurrent=") != NULL)
    {
        //设置家庭最大流入电流,获取=右边的数值
        char part1[64] = {0};
        char part2[12] = {0};
        SplitTwoString(cmd,"=", part1,part2);
        uint8_t  inflowMaxCurrent = atoi(part2);
        //设置家庭最大流入电流
        GPIOManager_SetInletCurrent(inflowMaxCurrent);
    }
    else if(strstr(cmd, "AT+PLCSEND=") != NULL)
    {
        SuspendAllPlcTaskThread();
        int iRet = 0;
        //AT指令调试
        char part1[25] = {0};
        char part2[25] = {0};
        char AtcmdStr[50] = {0};
        char rcv_buf[256] = {0};
        //char response[512] = {0};
        SplitTwoString(cmd,"=", part1,part2);
        //=号后面的就是调试指令的名称

        strncpy(AtcmdStr,GetAtCmdByIdx(part2),sizeof(AtcmdStr));
        //获取到了指令，就发送出去,先停止线程
        //发送AT指令,接收回复数据
        iRet = SendDataWithReadResult(AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf), RECEIVE_BUF_WAIT_1S);
        //接收到数据了返回
        //为了方便调试，先返回发送的数据
        if(iRet >0)
        {
            rcv_buf[iRet] = 0;
            debug_at_cmd_send_response(rcv_buf);
        }
        else
        {
            debug_at_cmd_send_response(AtcmdStr);
        }
        ResumeAllPlcTaskThread();

    }
    else if(strstr(cmd, "AT+STATIONSEND=") != NULL)
    {
        SuspendAllPlcTaskThread();
        int iRet = 0;
        char part1[25] = {0};
        char part2[256] = {0};
        char rcv_buf[256] = {0};
        SplitTwoString(cmd,"=", part1,part2);
        //需要再最后加上\r\n
        strcat(part2,"\r\n");
        iRet = SendDataWithReadResult((char *)part2,strlen(part2),rcv_buf,sizeof(rcv_buf), RECEIVE_BUF_WAIT_1S);
        //为了方便调试，先返回发送的数据
        if(iRet >0)
        {
            rcv_buf[iRet] = 0;
            memset(rcv_buf,0,sizeof(rcv_buf));
            //先读取到的是OK
            RecvPlcData((uint8_t *)rcv_buf,sizeof(rcv_buf));
            if(strlen(rcv_buf) == 0)
            {
               debug_at_cmd_send_response(part2);      
            }
            else
            {
                debug_at_cmd_send_response(rcv_buf); 
            }
        }
        else
        {
             debug_at_cmd_send_response(part2); 
        }
        ResumeAllPlcTaskThread();
    }
    else if(strcmp(cmd, "AT+SELECT_SEARCH_STATION?")  == 0)
    {
        
        char response[512] = {0};
        int len = 0;
        //查询已搜索到的充电桩列表
        char *request = "{\"method\":\"SubDeviceManager.SearchChargeStationRequest\",\"data\":{}}";
        ProcessRPCRequest(request,strlen(request),response,&len);
        prepend_string(response,"+SELECT_SEARCH_STATION=");   
        strcat(response,"\r\n"); 
        //返回已搜索到的充电桩列表
        debug_at_cmd_send_response(response);    
    }
    else if(strstr(cmd, "AT+LOG_LEVEL=")  != NULL)
    {
        char part1[25] = {0};
        char part2[25] = {0};
        SplitTwoString(cmd,"=", part1,part2);
        //字符串转整形
        int level = atoi(part2);
        SetDebugLevel(level);
    }
    else if(strcmp(cmd, "AT+LOG_LEVEL?")  == 0)
    {
        char response[64] = {0};
        schedule_debug_level level = GetDebugLevel();
        snprintf(response,sizeof(response),"%s%d","+LOG_LEVEL=",level);
        debug_at_cmd_send_response(response);    
    }
    else if(strcmp(cmd, "AT+PLC_START_MONITOR")  == 0)
    {
        isStartMonitorPlc = TRUE;
    }
    else if(strcmp(cmd, "AT+PLC_STOP_MONITOR")  == 0)
    {
        isStartMonitorPlc = FALSE;
    }
    else if(strcmp(cmd, "AT+AUTO_START_MONITOR")  == 0)
    {
        isStartMonitorAuto = TRUE;
    }
    else if(strcmp(cmd, "AT+AUTO_STOP_MONITOR")  == 0)
    {
        isStartMonitorAuto = FALSE;
    }
    else if(strcmp(cmd, "AT+SELECT_CHARGE01")  == 0)
    {
        char response[768] = {0};
        int stationCount = 0;
        ChargingStation *stationArray = SelectAllChargeStation(&stationCount);
        ChargingStation *station = &stationArray[0];
        //获取第一个充电桩的信息
        snprintf(response,sizeof(response),"%s%s%s%s%s%s%s%llu%s%llu%s%s%s%f%s%d%s%d%s%s%s%d%s%d%s%d%s%d%s%d%s%d%s%d%s%d%s%s%s%d%s",
            "+AT+SELECT_CHARGE01={\"subDevId\":\"",station->serialNum,
            "\",\"name\":\"",station->name,
            "\",\"mac\":\"",station->mac,
            "\",\"startTime\":\"",station->startTime,
            "\",\"endTime\":\"",station->endTime,
            "\",\"EVStatus\":\"",station->EVStatus,
            "\",\"energy\":\"",(station->energy)/1000,
            "\",\"duration\":\"",station->duration,
            "\",\"isPlugged\":\"",station->isPlugged,

            "\",\"EVStatus\":\"",station->EVStatus,
            "\",\"acVoltageL1\":\"",station->acVoltageL1,
            "\",\"acCurrentL1\":\"",station->acCurrentL1,
            "\",\"isPlugged\":\"",station->isPlugged,
            "\",\"isEvseReady\":\"",station->isEvseReady,
            "\",\"isEvReady\":\"",station->isEvReady,
            "\",\"limitCurrent\":\"",station->limitCurrent,
            "\",\"isAdded\":\"",station->isAdded,
            "\",\"connectStatus\":\"",station->connectStatus,
            "\",\"mdcFwVersion\":\"",station->mdcFwVersion,
            "\",\"heart_beat_num\":\"",station->heart_beat_num,
            "\"}\r\n");
            debug_at_cmd_send_response(response);
    }
    else if(strcmp(cmd, "AT+SELECT_CHARGE02")  == 0)
    {
        //获取第2个充电桩的信息
        char response[768] = {0};
        int stationCount = 0;
        ChargingStation *stationArray = SelectAllChargeStation(&stationCount);
        ChargingStation *station = &stationArray[1];
        //获取第一个充电桩的信息
        snprintf(response,sizeof(response),"%s%s%s%s%s%s%s%llu%s%llu%s%s%s%f%s%d%s%d%s%s%s%d%s%d%s%d%s%d%s%d%s%d%s%d%s%d%s%s%s%d%s",
            "+AT+SELECT_CHARGE02={\"subDevId\":\"",station->serialNum,
            "\",\"name\":\"",station->name,
            "\",\"mac\":\"",station->mac,
            "\",\"startTime\":\"",station->startTime,
            "\",\"endTime\":\"",station->endTime,
            "\",\"EVStatus\":\"",station->EVStatus,
            "\",\"energy\":\"",(station->energy)/1000,
            "\",\"duration\":\"",station->duration,
            "\",\"isPlugged\":\"",station->isPlugged,

            "\",\"EVStatus\":\"",station->EVStatus,
            "\",\"acVoltageL1\":\"",station->acVoltageL1,
            "\",\"acCurrentL1\":\"",station->acCurrentL1,
            "\",\"isPlugged\":\"",station->isPlugged,
            "\",\"isEvseReady\":\"",station->isEvseReady,
            "\",\"isEvReady\":\"",station->isEvReady,
            "\",\"limitCurrent\":\"",station->limitCurrent,
            "\",\"isAdded\":\"",station->isAdded,
            "\",\"connectStatus\":\"",station->connectStatus,
            "\",\"mdcFwVersion\":\"",station->mdcFwVersion,
            "\",\"heart_beat_num\":\"",station->heart_beat_num,
            "\"}\r\n");
            debug_at_cmd_send_response(response);
    }
    else 
    {
        debug_at_cmd_send_response("\r\nERROR: Unknown command\r\n");
        debug_at_cmd_send_response(cmd);
        debug_at_cmd_send_response("Type AT+HELP? for help\r\n");
    }
    //EnableEasyLog();
}

//PLC发送和接收回调
void PlcDeBugDataCallBack(EVENT_TYPE event,char *arg,int len)
{
    //接收到事件之后，加上前缀，发送出去
    if(isStartMonitorPlc)
    {
        char *dest = (char *)malloc(512);
        memset(dest,0,512);
        strncpy(dest,arg,len);
        //加上前缀
        prepend_string(dest,"+PLC_MONITOR_DATA=");
        //加上\r\n
        strcat(dest,"\r\n");
        debug_at_cmd_send_response(dest);
        free(dest);
    }
}
//自动控制监控回调
void AutoControllMonitorCallBack(EVENT_TYPE event,char *arg,int len)
{
    if(isStartMonitorAuto)
    {
        char *dest = (char *)malloc(512);
        memset(dest,0,512);
        strncpy(dest,arg,len);
        //加上前缀
        prepend_string(dest,"+AUTO_MONITOR_DATA=");
        //加上\r\n
        strcat(dest,"\r\n");
        debug_at_cmd_send_response(dest);
        free(dest);
    }
    return;
}