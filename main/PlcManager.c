#include "PlcManager.h"
#include "DeBug.h"
#include "PlcModule3121.h"
#include "StringUtils.h"
#include "PlcSendQueue.h"
#include "PlcRecvQueue.h"
#include "Crc16Xom.h"
#include "StringUtils.h"
#include "CEvent.h"
#include "HexUtils.h"
#include "led.h"

PATPlcDataProFunc sg_pATPlcDataProFunc;
// PLC发送线程获取任务句柄（需要在创建任务时保存）
TaskHandle_t sg_xPlcSendTaskHandle;
//读取PLC串口数据线程
TaskHandle_t sg_xPlcMonitorTaskHandle;

// PLC线程暂停标志（使用volatile确保多线程可见性）
static volatile bool g_plc_should_pause = false;



int PlcInit()
{
    //初始化循环缓冲区,处理\n结尾的数据
    PlcRecvQueue_Init();
    //PLC发送消息队列初始化
    queue_init();
    //启动PLC串口
    char selfMac[13] = {0};
    int iRet = 0;
    iRet = StartAtSerial();
    if(RTN_SUCCESS != iRet)
    {
        dPrint(DERROR,"StartAtSerial failed\n");
        return RTN_FAIL;
    }
    //获取自身的mac地址
    //先判断有没有获取到mac地址
    //没有获取到mac地址就卡在获取mac地址的地方
    while(GetNvMac() == NULL)
    {
         for(int i = 0; i < 10; i++) {
        vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
        }
        //dPrint(DEBUG,"没有获取到mac地址\n");
        led_rgb_set_color(1, 0, 0);
    }
    if(GetSelfMac() != NULL)
    {
        strncpy(selfMac,GetNvMac(),sizeof(selfMac));
        dPrint(INFO,"获取到了自身的mac地址:%s\n",selfMac);
    }
    //启动数据监控和业务处理线程
    xTaskCreate(&PlcMonitorThreadFunc, "plc_monitor_task", 5120, NULL, 5, &sg_xPlcMonitorTaskHandle);
    //启动发送队列线程
    xTaskCreate(&PlcSendDataThread, "plc_send_data_task", 4096, NULL, 5, &sg_xPlcSendTaskHandle);
    return RTN_SUCCESS;
}

void SuspendAllPlcTaskThread()
{
    //dPrint(DEBUG,"SuspendAllPlcTaskThread: 设置暂停标志，等待线程主动停止...\n");

    // 设置暂停标志
    g_plc_should_pause = true;

    // 等待一段时间，让线程有机会检查标志并主动停止
    // PlcMonitorThreadFunc每次循环休眠50ms，等待150ms确保线程检查到标志
    vTaskDelay(pdMS_TO_TICKS(150));

    //dPrint(DEBUG,"SuspendAllPlcTaskThread: PLC线程应已暂停活动\n");
    return;
}

void ResumeAllPlcTaskThread()
{
    //dPrint(DEBUG,"ResumeAllPlcTaskThread: 清除暂停标志，恢复PLC线程活动\n");

    // 清除暂停标志，线程会自动恢复运行
    g_plc_should_pause = false;

    //dPrint(DEBUG,"ResumeAllPlcTaskThread: PLC线程已恢复\n");
    return;
}
BOOL IsRuningPlcTaskThread()
{
    // 现在使用标志位判断，而不是检查任务状态
    // 如果标志为true（应该暂停），返回FALSE（未运行）
    // 如果标志为false（不需要暂停），返回TRUE（正在运行）
    return !g_plc_should_pause;
}
void RegChargeStationDataFunc(PATPlcDataProFunc pfnDataFunc)
{
    sg_pATPlcDataProFunc = pfnDataFunc;
}
int SendPlcDataByDestMac(char* destMac ,char *pData,int len)
{
    int iRet = 0;
    char response[512] = {0};
    memset(response,0,sizeof(response));
    if(destMac == NULL || pData == NULL)
    {
        dPrint(DERROR,"destMac == NULL || pData == NULL\n");
        return RTN_FAIL;
    }
    //下面这一段代码不能被多线程同时调用
    static BOOL s_flag = TRUE;
    while(!s_flag)
    {
        vTaskDelay(pdMS_TO_TICKS(1));  // 每次延时1ms
        taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
    }
    s_flag = FALSE;
    //AT+SEND=4080E1346353,12,7E000C000001 01010104 237E,0\r\n
    int count = 0;
    //先计算CRC，使用逗号把中间的数据部分截取出来
    //dPrint(DEBUG,"计算CRC之前的原始数据是：%s\n",pData);
    //先把\r\n去掉
    strncpy(response,pData,sizeof(response));
    StrDelSpaceWrap(response);
    //dPrint(DEBUG,"原始数据部分是:%s\n",response);
    char** result = Split(response,',',&count);
    if(count>=3)
    {
        char crc16Buff[128] = {0};
        strncpy(crc16Buff,result[2],strlen(result[2])-1);
        //dPrint(DEBUG,"计算CRC前的数据是:%s\n",result[2]);
        modbusCRC16StdToString(crc16Buff);
        memset(result[2],0,strlen(result[2]));
        strncpy(result[2],crc16Buff,strlen(crc16Buff));
        //最后再加上结束符
        result[2][strlen(crc16Buff)] = PACKET_END_FLAG_DEFAULT;
    }
    else
    {
        dPrint(DERROR,"count<3\n");
    }
    //dPrint(DEBUG,"计算CRC后的result[2]:%s\n",result[2]);
    memset(response,0,sizeof(response));
    //转换为16进制数据，重新计算CRC
    //重新组合数据
    for(int i = 0;i<count;i++)
    {
        //dPrint(DEBUG,"result[%d]:%s\n",i,result[i]);
        strcat(response,result[i]);
        if(i < count-1)
        {
            strcat(response,",");
        }
    }
    //最后再加上\r\n
    strcat(response,"\r\n");
    //dPrint(DEBUG,"组装后的数据respons:%s\n",response);
    //释放空间
    for(int i = 0;i<count;i++)
    {
        free(result[i]);
    }
    free(result);
    s_flag = TRUE;
    //dPrint(DEBUG,"重新计算CRC之后的数据是：%s\n",response);
    //把Mac地址替换就可以了
    //由于是多线程调用，所以用栈上空间
    iRet = StringReplace(response,strlen("AT+SEND="),strlen("4080E1346353"),destMac);
    if(iRet != RTN_SUCCESS)
    {
        dPrint(DERROR,"StringReplace failed\r\n");	
        return RTN_FAIL;
    }
    //dPrint(DEBUG,"response queue_push_front:%s\n",response);
    queue_push_front(response,strlen(response));
    return RTN_SUCCESS;
}

int SendPlcATData(char *pData,int len)
{
    queue_push_front(pData,len);
    dPrint(DEBUG,"SendPlcATData queue_push_front:%s\n",pData);
    return RTN_SUCCESS;
}


void PlcMonitorThreadFunc(void *pvParameter)
{
    int iRet = 0;
    const int buffsize = 1024;
    //char jsonData[384] = {0};
    char *buffer = (char *)malloc(buffsize);
    while (1)
    {
        // 检查是否应该暂停
        while (g_plc_should_pause) {
            vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
            taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
        }
        //然后获取数据，获取到了完整的数据包，就去处理
        memset(buffer,0,buffsize);
        while(GetPlcRecvQueueData(buffer,buffsize) > 0 )
        {
            //需要把>字符去掉
            buffer[iRet-1] = 0;
            //把最前面的<也去去掉
            delete_chars_from_position(buffer,0,1);
            //需要加上\0
            //buffer[iRet] = 0;
            //dPrint(DEBUG,"接收到了完整的PLC数据包:%s\n",buffer);
            //处理数据包
            ProcessDataFunc(buffer,iRet);
            //判断是什么数据包,就通过事件发送出去
            //处理之后，把指令中的\r\n全部去除
            StrDelSpaceWrap(buffer);
            
            if(strstr(buffer,"7E00200001010304010D0104") != NULL)
            {
                dPrint(DEBUG,"接收到到了充电桩，电流/电压/能量的回复数据\n");
            }
            /*
            //版本号
            if(strstr(buffer,"7E001800010101010101010A") != NULL)
            {
                //版本号
                dPrint(INFO,"接收到了版本号:%s\n",buffer);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","recv",
                "\",\"method\":\"","接收版本号",
                "\",\"content\":\"",buffer,
                "\",\"value\":\"","0",
                "\"}");            

            }
            else if(strstr(buffer,"7E001000010101040106") != NULL)
            {
                //L1相电流
                dPrint(INFO,"接收到了L1相电流:%s\n",buffer);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","recv",
                "\",\"method\":\"","接收L1相电流",
                "\",\"content\":\"",buffer,
                "\",\"value\":\"","0",
                "\"}");            
            }
            else if(strlen(buffer) == 16)
            {
                dPrint(INFO,"接收到了充电桩的心跳包:%s\n",buffer);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","recv",
                "\",\"method\":\"","接收到心跳包",
                "\",\"content\":\"",buffer,
                "\",\"value\":\"","0",
                "\"}");               
            }
            else
            {
                //未知的指令
                dPrint(INFO,"接收到了未知的指令:%s\n",buffer);    
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","recv",
                "\",\"method\":\"","接收到未知的指令",
                "\",\"content\":\"",buffer,
                "\",\"value\":\"","0",
                "\"}");               
            }
            PublishEvent(EVENT_PLC_SEND_RECV_DATA,jsonData,strlen(jsonData));
            */
        }
        //只有监控线程运行时，才去接收串口数据
        memset(buffer,0,buffsize);
        //memset(jsonData,0,sizeof(jsonData));
        //读取PLC串口中的数据
        iRet = RecvPlcData((uint8_t *)buffer,buffsize);
        if(iRet >0)
        {
            printf("\r\n");
            dPrint(DEBUG,"PlcData:%s\r\n",buffer);
            //先去掉所有的\r\n
            StrDelSpaceWrap(buffer);
            //如果数据里面由\rok就先把\rok去掉
            if(strstr(buffer,"+ok") || strstr(buffer,"+err"))
            {
                remove_substring_preserve(buffer,"+ok");
                //remove_substring_preserve(buffer,"+ok");
                remove_substring_preserve(buffer,"+err");
                dPrint(INFO,"把+ok或者+err去掉\n");  
            }
            //处理系统指令
            if(IsPlcSysCmdAck(buffer))
            {
                // 查找是否包含数据包（以'<'开始）
                char *data_start = strchr(buffer, '<');
                if(data_start != NULL)
                {
                    // 分离系统指令和数据包
                    // 1. 先处理系统指令部分
                    char sys_cmd[256] = {0};
                    int sys_cmd_len = data_start - buffer;
                    strncpy(sys_cmd, buffer, sys_cmd_len);
                    dPrint(INFO,"process plc sys cmd:%s\n",sys_cmd);
                    ProcessPlcSysCmdFunc(sys_cmd, strlen(sys_cmd));

                    // 2. 再将数据包部分放入队列
                    dPrint(DEBUG,"\nPutPlcRecvQueueData buffer:%s\n",data_start);
                    PutPlcRecvQueueData(data_start, strlen(data_start));
                }
                else
                {
                    // 纯系统指令，没有数据包
                    dPrint(INFO,"process plc sys cmd:%s\n",buffer);
                    ProcessPlcSysCmdFunc(buffer, strlen(buffer));
                    continue;
                }
            }
            else
            {
                dPrint(DEBUG,"\nPutPlcRecvQueueData buffer:%s\n",buffer);
                //接收到了数据开始组装，以>结尾为一整包数据
                PutPlcRecvQueueData(buffer,strlen(buffer));
            }
        }

        //休眠50毫秒
        //vTaskDelay(50 / portTICK_PERIOD_MS);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    free(buffer);
    return;
}


void PlcSendDataThread(void *pvParameter)
{
    int iRet;	
    char pData[384] = {0};
    int len = sizeof(pData);
    //需要组装json发送出去数据
    char jsonData[512] = {0};
    //向串口发送出去
    while (1)
    {
        // 检查是否应该暂停
        while (g_plc_should_pause) {
            //dPrint(DEBUG,"PlcSendDataThread: 暂停中...\n");
            vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
            taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
        }

        //只有PLC线程运行时，才执行发送任务
        len = sizeof(pData);
        memset(pData,0,sizeof(pData));
        //清空缓冲区
        memset(jsonData,0,sizeof(jsonData));
        //每隔1秒获取一次，然后发送一帧数据
        iRet = queue_pop_back(pData,&len);
        if(iRet != RTN_SUCCESS)
        {
            //dPrint(WARN,"queue_pop_back error\n");
        }
        else
        {
            //dPrint(DEBUG,"queue_pop_back data:%s\n",pData);
            SendPlcData(pData,len);
            //发送完之后，去掉指令中的\r\n
            StrDelSpaceWrap(pData);
            //判断是什么指令，打印出来
            if(strstr(pData,"7E000C00000101010105E3BF") != NULL)
            {
                dPrint(DEBUG,"发送查询充电桩序列号指令 data:%s\n",pData);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select SerialNumber",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");
            }
            else if(strstr(pData,"7E000C0000010101010120BE")!= NULL)
            {
                dPrint(DEBUG,"发送查询充电桩版本指令 data:%s\n",pData);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select FirmWare",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");        
            }
            else if(strstr(pData,"7E000E000002010401140106E4BE") != NULL || strstr(pData,"7E000E000002010401140108203F") != NULL)
            {
                dPrint(DEBUG,"发送电桩启动充电的指令 data:%s\n",pData);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Start Charging",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");              
            }
            else if(strstr(pData,"7E000E000002010401140107247F") != NULL || strstr(pData,"7E000E000002010401140109E0FE") != NULL)
            {
                dPrint(DEBUG,"发送充电桩停止充电的指令 data:%s\n",pData);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Stop Charging",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");                    
            }
            else if(strstr(pData,"7E000C00000101040106E3EF") != NULL)
            {
                dPrint(DEBUG,"查询L1相电压 data:%s\n",pData);  
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select L1 voltage",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");                      
            }
            else if(strstr(pData,"7E000C00000101040109E7AF") != NULL)
            {
                dPrint(DEBUG,"查询L1相电流 data:%s\n",pData);  
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select L1 current",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");                         
            }
            else if(strstr(pData,"7E000C0000010104010D24AE")!= NULL)
            {
                dPrint(DEBUG,"查询充电桩功耗 data:%s\n",pData);
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select energy",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");              
            }
            else if(strstr(pData,"7E00120000010304010E04020F040310F0DE")!= NULL)
            {
                dPrint(DEBUG,"一次性查询充电桩状态ev evse plugged data:%s\n",pData);	
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Select EvStatus",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");              
            }
            else if(strstr(pData,"7E00110000020104010104")!= NULL)
            {
                dPrint(DEBUG,"设置充电桩限制电流指令:%s\n",pData);	
                char dest[10] = {0};
                unsigned char out[4] = {0};
                unsigned int outlen = 0;
                substr_from_end(dest,pData,4,8);
                //转换为16进制数据
                StrToHexArray(dest, out, &outlen);
                //大小端转换
                
                int value = big_char_to_int((char *)out);
                char str[20];
                itoa(value, str, 10);  // 参数：整数值、目标字符串、进制基数
                //获取到下发的电流值
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","Set Charging Limit",
                "\",\"content\":\"",pData,
                "\",\"value\":\"",str,
                "\"}");            
            }
            else
            {
                dPrint(DEBUG,"未知的指令：%s\n",pData);	
                sprintf(jsonData,"%s%s%s%s%s%s%s%s%s",
                "{\"type\":\"","send",
                "\",\"method\":\"","send unkown cmd",
                "\",\"content\":\"",pData,
                "\",\"value\":\"","0",
                "\"}");            
            }
            
            //发送事件,方便调试查看
            PublishEvent(EVENT_PLC_SEND_RECV_DATA,jsonData,strlen(jsonData));

        }
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        for(int i = 0; i < 2; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(500));  // 每次延时500ms
            taskYIELD();  // 主动让出CPU，给低优先级任务（包括IDLE）运行机会
        }
    }
    return;
}
int ProcessPlcSysCmdFunc(char *pchPackData, int iPackLen)
{
    char event[20] = {0};

    strncpy(event,GetAtCmdIdByAckString(pchPackData),sizeof(event));
    if(strncmp(OK_RELAY,event,strlen(OK_RELAY)) == 0)
    {
        dPrint(INFO,"ProcessPlcSysCmdFunc OK_RELAY\n");
    }
    else if(strncmp(ERR_RELAY,event,strlen(ERR_RELAY)) == 0)
    {
        dPrint(DERROR,"ProcessPlcSysCmdFunc ERR_RELAY:%s\n",pchPackData);
    }
    else if(strncmp(AT_CMD_SYS_START_EVENT,event,strlen(AT_CMD_SYS_START_EVENT)) == 0)
    {
        
        dPrint(INFO,"ProcessPlcSysCmdFunc AT_CMD_SYS_START_EVENT plc system start\n");	
    }
    else if(strncmp(AT_CMD_ONLINE_EVENT,event,strlen(AT_CMD_ONLINE_EVENT)) == 0)
    {
       
        dPrint(INFO,"ProcessPlcSysCmdFunc AT_CMD_ONLINE_EVENT plc station online pchPackData:%s\n",pchPackData);
    }
    else if(strncmp(AT_CMD_OFFLINE_EVENT,event,strlen(AT_CMD_OFFLINE_EVENT)) == 0)
    {
        
        dPrint(INFO,"ProcessPlcSysCmdFunc AT_CMD_OFFLINE_EVENT plc station offline,pchPackData:%s\n",pchPackData);
    
    }
    else if(strncmp(AT_CMD_TOPONUM_EVENT,event,strlen(AT_CMD_TOPONUM_EVENT)) == 0)
    {
        
        dPrint(INFO,"lzy ProcessPlcSysCmdFunc AT_CMD_TOPONUM_EVENT \n");
    }
    else if(strncmp(AT_CMD_TOPOINFO_EVENT,event,strlen(AT_CMD_TOPOINFO_EVENT)) == 0)
    {
        dPrint(INFO,"lzy ProcessPlcSysCmdFunc AT_CMD_TOPOINFO_EVENT \n");
    }
    else if(strncmp(AT_CMD_NV_MAC_EVENT,event,strlen(AT_CMD_NV_MAC_EVENT)) == 0)
    {
        dPrint(INFO,"AT_CMD_NV_MAC_EVENT mac addr AT_CMD_NV_MAC_EVENT \n");
    }
    else
    {
        dPrint(INFO,"ProcessPlcSysCmdFunc AT_CMD_MAX_ACK not find\n");
    }
    return RTN_SUCCESS;
}
int ProcessDataFunc(char *pchPackData, int iPackLen)
{
    //char request[512] = {0};
    char nvMac[13] = {0};
    if(NULL == pchPackData || iPackLen == 0)
    {
           dPrint(DEBUG,"ProcessGateCmdData has null pointer. \n");
           return RTN_FAIL;
    }
    //dPrint(DEBUG,"获取到充电桩的原始数据是:Process PLC DataFunc = %s\n", pchPackData);
    //需要把数据扒掉前面的0x7DMAC地址，然后转为16进制数据抛到上层
    if(pchPackData == NULL || strlen(pchPackData) == 0)
    {
        return RTN_FAIL;
    }
    //strncpy(request,pchPackData,sizeof(request));
    if(strlen(pchPackData) < 2 )
    {
        dPrint(DERROR,"strlen(pchPackData) < 2:%s\n",pchPackData);
        return RTN_FAIL;
    }
    /*
    //处理AT模式下的数据传输格式
    char *posPtr = strstr(request,",");
    if (posPtr != NULL) 
    {
        RemoveBeforeComma(request,',');
    }
    posPtr = strstr(request,",");
    if (posPtr != NULL)
    {
        RemoveBeforeComma(request,',');
    }
    
    //处理AT模式下的数据传输格式 --- end
    if(strlen(request) < 2 || (request[0] != '7' && request[1] != 'D') )
    {
        dPrint(DERROR,"request first not 7D protocol error request:%s\n",request);
        return RTN_FAIL;
    }
    if(strlen(request) < 14)
    {
        dPrint(DERROR, "request.length() < 14\n");
        return RTN_FAIL;
    }
    */
    //AT模式下去掉\r\n
    //RemoveSubstring(request,"\r\n");
    //截取出mac地址
    //SubString(nvMac,request,2,12);
    SubString(nvMac,pchPackData,0,12);
    //从指定位置开始删除，删除指定长度
    //delete_chars_from_position(request,0,14);
    delete_chars_from_position(pchPackData,0,12);
    if(strlen(pchPackData) == 0)
    {
        dPrint(DERROR, "request.length() == 0\n");
        return RTN_FAIL;
    }
    if(pchPackData[0] != '7')
    {
        dPrint(DERROR,"ProcessDataFunc request.at(0) != 7E,%s\n",pchPackData);
        return RTN_FAIL;
    }
    //AT模式下需要砍掉,0
    //TruncateFromEnd(request,2);
    //字符串转16进制
    char recvBuff[256] = {0};
    unsigned int outlen = 0;
    StrToHexArray((char *)pchPackData,(unsigned char *)recvBuff,&outlen);

    dPrint(DEBUG,"finish hand AT request:%s,nvMac:%s,outlen:%d\n",pchPackData,nvMac,outlen);
    //HexPrint(recvBuff,outlen);
       sg_pATPlcDataProFunc(nvMac,recvBuff,outlen);
    return RTN_SUCCESS;
}
/*
int AnalyseStringDataToHex(char *request,char *outHex,unsigned int *outLen)
{
    
    if(NULL == request || NULL == outHex || outLen == NULL|| strlen(request) == 0)
    {
           dPrint(DEBUG,"AnalyseStringDataToHex has null pointer. \n");
           return RTN_FAIL;
    }
    dPrint(DEBUG,"AnalyseStringDataToHex request = %s\n", request);
    //需要把数据扒掉前面的0x7DMAC地址，然后转为16进制数据抛到上层

    //处理AT模式下的数据传输格式
    char *posPtr = strstr(request,",");
    if (posPtr != NULL) {
        RemoveBeforeComma(request,',');
    }
    posPtr = strstr(request,",");
    if (posPtr != NULL) {
        RemoveBeforeComma(request,',');
    }
    //处理AT模式下的数据传输格式 --- end
    if(strlen(request) < 2 || (request[0] != '7' && request[1] != 'D') )
    {
        dPrint(DERROR,"request first not 7D protocol error request:%s\n",request);
        return RTN_FAIL;
    }
    if(strlen(request) < 14)
    {
        dPrint(DERROR, "request.length() < 14\n");
        return RTN_FAIL;
    }

    //AT模式下去掉\r\n
    RemoveSubstring(request,"\r\n");
    //截取出mac地址
    
    char nvMac[13] = {0};
    SubString(nvMac,request,2,12);
    //截取字符串
    delete_chars_from_position(request,0,14);

    if(strlen(request) == 0)
    {
        dPrint(DERROR, "request.length() == 0\n");
        return RTN_FAIL;

    }
    if(request[0] != '7')
    {
        dPrint(DERROR,"ProcessDataFunc request.at(0) != 7E,%s\n",request);
        return RTN_FAIL;
    }
    dPrint(DEBUG,"finish resize 2 = :%s\n",request);
    //AT模式下需要砍掉,0
    //request.resize(request.length() - 2);
    TruncateFromEnd(request,2);
    dPrint(DEBUG,"finish hand AT request:%s\n",request);
    
    //字符串转16进制

    StrToHexArray((char *)request,(unsigned char *)outHex,outLen);

    dPrint(DEBUG, "nvMac:%s, outlen = %d\n",nvMac, *outLen);
    
    return RTN_SUCCESS;
}
*/