#include "PlcModule3121.h"
#include "types.h"
#include "StringUtils.h"
#include "DeBug.h"
#include "serial.h"

static char sg_selfMac[13] = {0};

//启动AT串口
int StartAtSerial()
{
	//ESP自身的串口已经初始化过了,与PLC模组的一样，所以不需要特别做什么
	//进入AT 配置模式
	//EnterAtMode();
	//EXITAtMode();
	//打开回显
	OpenECHO();
	//设置默认AT模式
	SetAtMode();
	//清除白名单
	ClearWhiteList();
	//禁用白名单
	DisableWhiteList();
	//开启入网通知
	EnableNotify();
	//查询波特率的信息
	QueryPlcSerialConfig();
    return RTN_SUCCESS;
}
//关闭串口连接
int StopAtSerial()
{

    return RTN_SUCCESS;
}
//重启串口
int RestartAtSerial()
{

    return RTN_SUCCESS;
}
//获取AT 串口文件描述符
uart_port_t GetConnectFd()
{
    return SERIAL_UART1;
}
/*
*SendPlcData :IN 发送的数据缓冲区指针,有外部申请内存的。
*iSendLen:要发送的数据长度
*/
int SendPlcData(char *pchSendData, int iSendLen)
{
	serial_write(GetConnectFd(), (const uint8_t *)pchSendData,(size_t)iSendLen);
    return RTN_SUCCESS;
}
/*
*接收数据到缓冲区
return true 成功false 失败
*/	
int SendDataWithReadResult(char *send_buf, int sendlen, char *rcv_buf, int recvLen, int rcv_wait)
{
	int iRet = 0;
	iRet  = serial_read_with_result(GetConnectFd(), (uint8_t*)send_buf,sendlen,
                            (uint8_t*)rcv_buf,recvLen,rcv_wait);
	return iRet;
}
int RecvPlcData(uint8_t *buffer, size_t length)
{
	 return serial_read(GetConnectFd(),buffer,length,RECEIVE_BUF_WAIT_100MS);
}
//获取整包数据
/*
int GetPackData(char *pchPackData, int iPackBufSize)
{

    return RTN_SUCCESS;
}
*/
int OpenECHO(void)
{
	//关闭回显
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_OPEN_ECHO),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_OPEN_ECHO\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	//如果第一个字符是回车符，应该删除掉
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"打开回显成功response success rcv_buf:%s\n",rcv_buf);	
		return RTN_SUCCESS;
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
		return RTN_FAIL;
	}
	return RTN_SUCCESS;
}
int CloseECHO(void)
{
	//关闭回显
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_CLOSE_ECHO),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_CLOSE_ECHO\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	//如果第一个字符是回车符，应该删除掉
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"response success rcv_buf:%s\n",rcv_buf);	
		return RTN_SUCCESS;
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
		return RTN_FAIL;
	}
	return RTN_SUCCESS;
}
//查询工作模式
const char* QueryAtMode(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
	strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_DEFAULT_MODE),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_DEFAULT_AT_MODE\n");
		return NULL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		return NULL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(ERR_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DEBUG,"response success rcv_buf:%s\n",rcv_buf);	
	}
	if(strstr(rcv_buf,"2")!= NULL)
	{
		return "AT";
	}
	else if(strstr(rcv_buf,"0") != NULL)
	{
		return "DATA";
	}
	else
	{
		dPrint(WARN,"unkown mode:%s\n",rcv_buf);
		return NULL;
	}
    return NULL;
}
//进入AT 配置模式
int EnterAtMode(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_ENTER),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_ENTER\n");
        return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	//dPrint(INFO,"EnterAtMode:%s\n",AtcmdStr.c_str());
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
    strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	//dPrint(INFO,"EnterAtMode:rcv_buf:%s\n",rcv_buf);
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	

	}
    return RTN_SUCCESS;
}
//设置默认AT 配置模式
int SetAtMode(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_DEFAULT_AT_MODE),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_DEFAULT_AT_MODE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	//如果第一个字符是回车符，应该删除掉
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"设置默认AT模式response success rcv_buf:%s\n",rcv_buf);	
		return RTN_SUCCESS;
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
		return RTN_FAIL;
	}
}
//退出AT 配置模式
int EXITAtMode(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_DEFAULT_DATA_MODE),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_LEAVE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"EXITAtMode SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"EXITAtMode response success rcv_buf:%s\n",rcv_buf);
         return RTN_SUCCESS;	
	}
	else
	{

		dPrint(DERROR,"EXITAtMode response error rcv_buf:%s\n",rcv_buf);	
        return RTN_FAIL;
	}
   
}
//配置透传模式
int DataMode(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_DEFAULT_DATA_MODE),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_DEFAULT_AT_MODE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"response success rcv_buf:%s\n",rcv_buf);	
		return RTN_SUCCESS;
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
        return RTN_FAIL;
	}
    
}
//查询PLC串口信息
SerialConfig_T QueryPlcSerialConfig()
{
	int iRet;
    int count = 0;
	SerialConfig_T serial_config_t = {0};
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_CONFIG_BAUD),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_DEFAULT_AT_MODE\n");
		return serial_config_t;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		return serial_config_t;
	}
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	//AtcmdAckStr = m_plcAtcmd3121->GetAtCmdAckByIndex(OK_RELAY);
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)"+ok")!= NULL))
	{
		dPrint(DEBUG,"查询串口信息response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
		return serial_config_t;
	}
	//先用:分割
    char** result = Split(rcv_buf,'=',&count);
	if(count == 0)
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
        free(result);
		return serial_config_t;
	}
	strncpy(AtcmdAckStr,result[1],sizeof(AtcmdAckStr));
    for(int i = 0;i<count;i++)
    {
        free(result[i]);  
    }
    free(result);
    count = 0;
    result = Split(AtcmdAckStr,',',&count);
	if(count < 4)
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
        for(int i = 0;i<count;i++)
        {
            free(result[i]);  
        }
        free(result);
		return serial_config_t;
	}
	serial_config_t.iBaud = atoi(result[0]);
	serial_config_t.dataBit = atoi(result[1]);
	serial_config_t.stopBit = atoi(result[2]);
	int checkbit = atoi(result[3]);
	switch(checkbit)
	{
		case 0:
		{
			serial_config_t.checkBit = 'n';
			break;
		}
		case 1:
		{
			serial_config_t.checkBit = 'o';
			break;
		}
		case 2:
		{
			serial_config_t.checkBit = 'e';
			break;
		}
	}
    for(int i = 0;i<count;i++)
    {
        free(result[i]);  
    }
    free(result);
	dPrint(DEBUG,"iBaud:%d,dataBit:%d,stopBit:%d,checkbit:%c\n",serial_config_t.iBaud,serial_config_t.dataBit,serial_config_t.stopBit,serial_config_t.checkBit);
	return serial_config_t;
}
//配置PLC 波特率
int ConfigPlcBaud(SerialConfig_T* serilaConfig)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    char checkbit = 2;
	//校验位
	if(serilaConfig->checkBit == 'n')
	{
		checkbit = 0;
	}
	else if(serilaConfig->checkBit == 'o')
	{
		checkbit = 1;
	}
	else if(serilaConfig->checkBit == 'e')
	{
		checkbit = 2;
	}
    snprintf(AtcmdStr,sizeof(AtcmdStr),"%s%d%c%d%c%d%c%d%s","AT+UART=",serilaConfig->iBaud,',',serilaConfig->dataBit,',',serilaConfig->stopBit,',',checkbit,"\r\n");
	//AtcmdStr = "AT+UART=" + serialbaud + "," + databit + "," + stopbit + "," + checkbit + "\r\n";
	dPrint(DEBUG,"ConfigPlcBaud AtcmdStr:%s \n",AtcmdStr);

	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"ConfigPlcBaud response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"ConfigPlcBaud response error rcv_buf:%s\n",rcv_buf);	
	}
    return RTN_SUCCESS;
}
//重启PLC
int RebootPlc(void)
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_REBOOT),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_REBOOT\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{

		dPrint(DEBUG,"RebootPlc response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"RebootPlc response error rcv_buf:%s\n",rcv_buf);	
	}
    return RTN_SUCCESS;
}
//获取PLC电网的节点数量，包含CCO节点
int GetPlcNodeCount()
{
	int iRet;
    int count = 0;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QURY_NODE_NUM),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_QUERY_NODES\n");
		return 0;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return 0;
	}
	rcv_buf[iRet] = 0;
	if (strstr(rcv_buf, "TOPONUM") != NULL)
	{
		dPrint(DEBUG,"get success TOPOINFO= %s \n",rcv_buf);
	}
	else
	{
		dPrint(DERROR,"get failed TOPOINFO= %s \n",rcv_buf);
		return 0;
	}

    memset(AtcmdStr,0,sizeof(AtcmdStr));
    memset(AtcmdAckStr,0,sizeof(AtcmdAckStr));
    SplitTwoString(rcv_buf,":",AtcmdStr,AtcmdAckStr);
    if(strlen(AtcmdAckStr) == 0)
    {
        return 0;
    }
	//获取出数量
    count = atoi(AtcmdAckStr);
    return count;
}

//查询白名单状态
int QueryWhiteStatus()
{
    return RTN_SUCCESS;
}
//开启白名单
int EnableWhiteList()
{

    return RTN_SUCCESS;
}
//关闭白名单
int DisableWhiteList()
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
    char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_DISABLE_WHITE),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_ENABLE_WHITE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"禁用白名单response success rcv_buf:%s\n",rcv_buf);	

	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
		
	}
    return RTN_SUCCESS;
}

//清除白名单
int ClearWhiteList()
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_CLR_WHINFO),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_ENABLE_WHITE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_1S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	strncpy(AtcmdAckStr,GetAtCmdAckByIndex(OK_RELAY),sizeof(AtcmdAckStr));
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)AtcmdAckStr)!= NULL))
	{
		dPrint(DEBUG,"清除白名单成功 response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"清除白名单失败 response error rcv_buf:%s\n",rcv_buf);	
	}
    return RTN_SUCCESS;
}
//查询自身MAC地址
const char* GetNvMac()
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	//char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_MAC),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_LEAVE\n");
		return NULL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	// 修改：只读取64字节（足够容纳响应 "+ok=XXXXXXXXXXXX\r\n"）
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,64,RECEIVE_BUF_WAIT_1S);
	//dPrint(DEBUG,"GetNvMac SendDataWithReadResult returned: %d bytes\n", iRet);
	if(iRet<0)
	{
		dPrint(DERROR,"GetNvMac SendDataWithReadResult error AtcmdStr:%s ,rcv_buf:%s\n",AtcmdStr,rcv_buf);
		//如果发送失败了，就重启串口，跳出循环
		return NULL;
	}
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	// 调试：打印接收到的原始数据（十六进制）
	// 确保字符串以 null 结尾
	if(iRet > 0 && iRet < MAX_RECEI_BUFF_LEN) {
		rcv_buf[iRet] = '\0';
	}
	//获取到了回复，需要解析出mac地址
	if(strlen(rcv_buf) == 0 || strstr(rcv_buf,"ok") == NULL)
	{
		dPrint(DEBUG,"GetNvMac failed not find ok, rcv_buf len=%zu, content:%s\n", strlen(rcv_buf), rcv_buf);
		return NULL;
	}
	//dPrint(DEBUG,"GetNvMac rcv_buf:%s\n",rcv_buf);
    char part1[50] = {0};
    char part2[50] = {0};
    SplitTwoString(rcv_buf,AT_CMD_SPLITER_FLAG,part1,part2);
	//分割为了2部分，第二部分是mac地址
	if(strlen(part2) <12)
	{
		dPrint(DERROR,"GetNvMac error,vec.size() < 2\n"); 
		return NULL;
	}
	//只要前12个字节
    SubString(sg_selfMac,part2,0,12);
	//转换为大写字母
	ToUpperCase(sg_selfMac);
	dPrint(DEBUG,"GetNvMac mac addr:%s\n",sg_selfMac);
	return sg_selfMac;
}
const char* GetSelfMac()
{
	return sg_selfMac;
}
//判断是否是系统指令回复
BOOL IsPlcSysCmdAck(const char* request)
{
    //char AtcmdAckStr[50] = {0};    
    if(request == NULL || strlen(request) == 0)
	{
		return FALSE;
	}
	if(request[0] == '+')
	{
		//dPrint(DEBUG,"是系统指令\n");
		return TRUE;
	}
	//dPrint(DEBUG,"是充电桩发送过来的数据:%s\n",request);
    return FALSE;
}
//查询是否开启入网通知
int QueryNotifyStatus()
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	//char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_NOTIFY),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_QUERY_NOTIFY\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_2S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		//如果发送失败了，就重启串口，跳出循环
		//RestartAtSerial();
		return RTN_FAIL;
	}
	rcv_buf[iRet] = 0;
	//发送成功了，就解析
	//AtcmdAckStr = m_plcAtcmd3121->GetAtCmdAckByIndex(OK_RELAY);
	//说明能找到OK 回复
	if((strstr(rcv_buf, (char *)"+ok")!= NULL))
	{
		dPrint(DEBUG,"response success rcv_buf:%s\n",rcv_buf);	
	}
	else
	{
		dPrint(DERROR,"response error rcv_buf:%s\n",rcv_buf);	
        return RTN_FAIL;
	}
	if(strstr(rcv_buf,"1")!= NULL)
	{
		return RTN_SUCCESS;
	}
	else if(strstr(rcv_buf,"0") != NULL)
	{
		return RTN_FAIL;
	}
	else
	{
		dPrint(WARN,"unkown status:%s\n",rcv_buf);
		return RTN_FAIL;
	}
    return RTN_SUCCESS;
}
//开启入网通知
int EnableNotify()
{
	int iRet;
	char rcv_buf[MAX_RECEI_BUFF_LEN] = {0};
	//先获取指令
	char AtcmdStr[50] = {0};
	//char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_OPEN_NOTIFY),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_LEAVE\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_2S);
	if(iRet<0)
	{
		dPrint(DERROR,"EXITAtMode SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet] = 0;
	dPrint(DEBUG,"开启入网通知EnableNotify response:%s\n",rcv_buf);
    return RTN_SUCCESS;
}

int GetMACadress(char *rcv_buf,SearchStation *stationArray)
{
	int count = 0;
	char macBuff[13] = {0};
	int iRet;

	//dPrint(DEBUG,"GetMACadress: 进入函数\n");

	if(stationArray == NULL)
	{
		dPrint(DERROR,"stationArray == NULL\n");
		return RTN_FAIL;
	}
	//先判断有没有获取到自身的mac地址，如果没有自身的mac返回错误
	if(strlen(sg_selfMac) == 0)
	{
		dPrint(DERROR,"没有获取到自身的mac地址\r\n");
		return RTN_FAIL;
	}
	//然后获取所有的mac
	//先获取指令
	char AtcmdStr[50] = {0};
	//char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_NODES),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_QUERY_NODES\n");
		return RTN_FAIL;
	}
	// 应该使用实际的缓冲区大小 MAX_RECEI_BUFF_LEN
	dPrint(DEBUG,"GetMACadress: 准备发送命令: %s\n", AtcmdStr);
	//memset(rcv_buf,0,MAX_RECEI_BUFF_LEN);
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,MAX_RECEI_BUFF_LEN,RECEIVE_BUF_WAIT_1S);
	dPrint(DEBUG,"GetMACadress: SendDataWithReadResult返回 iRet=%d\n", iRet);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	if(rcv_buf[0] == '\r')
	{
		delete_chars_from_position(rcv_buf,0,1);
	}
	rcv_buf[iRet+1] = 0;
	dPrint(DEBUG,"GetMACadress: 长度=%d, 内容:%s\n", iRet, rcv_buf);
	const char* okRelay = GetAtCmdAckByIndex(OK_RELAY);
	if (strstr(rcv_buf, okRelay) == NULL)
	{
		dPrint(DERROR,"get failed TOPOINFO= %s \n",rcv_buf);
		return 0;
	}
	if(strlen(rcv_buf) < 12)
	{
		dPrint(DERROR,"扫描到的充电桩数量为0:%s\n",rcv_buf);
		return 0;
	}
	
	//先组织模拟数据
	//strncpy(rcv_buf,"AT+TOPOINFO=1,3\n+TOPOINFO:4080E1346353,1,0,0,4,127,127,1\n+TOPOINFO:0000C0A80120,2,1,1,1,25,62,1\n+TOPOINFO:0000C0A80130,3,1,1,1,23,67,1\nOK\n",strlen("AT+TOPOINFO=1,3\n+TOPOINFO:0000C0A80100,1,0,0,4,127,127,1\n+TOPOINFO:0000C0A80120,2,1,1,1,25,62,1\n+TOPOINFO:0000C0A80130,3,1,1,1,23,67,1\nOK\n"));
	char *ptr = rcv_buf;  // 指向字符串起始位置
    int line_start = 1; // 标记是否为行首
    // 遍历整个字符串
    while (*ptr != '\0') 
	{
        // 检查是否为新行开始且匹配"+TOPOINFO:"
        if (line_start && strstr(ptr, "+ok=") != NULL) 
		{
			memset(macBuff,0,sizeof(macBuff));			
			if(ptr[0] == '\r')
			{
				ptr = ptr + 1;	
			}	
			char *mac = ptr + strlen("+ok=");				// 跳过前缀部分，获取<mac>字符串
			strncpy(macBuff, mac, 12); 						// 复制<mac>字符串到数组中
			macBuff[12] = '\0';								// 添加字符串结束标记
			//转换为大写字母
			ToUpperCase(macBuff);
            // 找到匹配行，打印直到换行符或字符串结束
            char *line_end = strchr(ptr, '\n');
			//dPrint(DEBUG,"line_end:%s\n",line_end);
            if (line_end != NULL) 
			{
                // 计算当前行长度并打印
                int line_length = line_end - ptr;
                char line[256] = {0};
                strncpy(line, ptr, line_length);
                line[line_length] = '\0';
				//需要把自己的mac地址去除掉
				if(strncmp(sg_selfMac,macBuff,strlen(sg_selfMac)) == 0)
				{
					dPrint(DEBUG,"自身的mac地址:%s\n",sg_selfMac);	
				}
				else
				{
					dPrint(DEBUG,"macBuff:%s,GetMACadress:%s\n",macBuff, line);
					//获取mac/信噪比/信号强度等信息
					GetChargingStationTopoInfo(line,stationArray,count);
					count++;
				}
                if(count>SEARCH_STATION_MAX_NUM)
				{
					dPrint(DEBUG,"充电桩的数量大于10,不支持\n");
					break;
				}
                ptr = line_end; // 移动到下一行开始
            } 
			else 
			{
                // 最后一行（没有换行符）
                dPrint(DEBUG,"最后一行:%s\n", ptr);
                break;
            }
        }
        
        // 移动到下一个字符
        if (*ptr == '\n') 
		{
            line_start = 1; // 下一字符是行首
        } 
		else 
		{
            line_start = 0; // 当前行中间
        }
        ptr++;
    }

	dPrint(DEBUG,"GetMACadress: 解析完成，找到 %d 个充电桩，准备返回\n", count);
	return count;
	/*
	int iRet;
	int count = 0;
	if(stations == NULL)
	{
		dPrint(DERROR,"stations == NULL\n");
		return RTN_FAIL;
	}
	//先判断有没有获取到自身的mac地址，如果没有自身的mac返回错误
	if(strlen(sg_selfMac) == 0)
	{
		dPrint(DERROR,"没有获取到自身的mac地址\r\n");
		return RTN_FAIL;
	}
	//然后获取所有的mac
	//先获取指令
	char AtcmdStr[50] = {0};
	//char AtcmdAckStr[50] = {0};
    strncpy(AtcmdStr,GetAtCmdByIdx(AT_CMD_QUERY_NODES),sizeof(AtcmdStr));
	if(strlen(AtcmdStr) == 0)
	{
		dPrint(DERROR,"GetAtCmdByIdx error AT_CMD_QUERY_NODES\n");
		return RTN_FAIL;
	}
	memset(rcv_buf,0,sizeof(rcv_buf));
	iRet = SendDataWithReadResult((char *)AtcmdStr,strlen(AtcmdStr),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_2S);
	if(iRet<0)
	{
		dPrint(DERROR,"SendDataWithReadResult error AtcmdStr:%s \n",AtcmdStr);	
		return RTN_FAIL;
	}
	if (strstr(rcv_buf, "TOPOINFO") != NULL)
	{
		dPrint(DEBUG,"get success TOPOINFO= %s \n",rcv_buf);
	}
	else
	{
		dPrint(DERROR,"get failed TOPOINFO= %s \n",rcv_buf);
		return RTN_FAIL;
	}
	// 提取MAC地址并赋给二维数组
	char macBuff[13] = {0};
	char *mactoken = strtok(rcv_buf, "\n");
	//int index = 0; // 用于记录<mac>字符串在数组中的索引
	dPrint(DEBUG,"token =%s\n", mactoken);
	while (mactoken != NULL)
	{
		// 检查前缀是否匹配
		if (strncmp(mactoken, "+TOPOINFO:", strlen("+TOPOINFO:")) == 0)
		{												
			memset(macBuff,0,sizeof(macBuff));				
			char *mac = mactoken + 10;						// 跳过前缀部分，获取<mac>字符串
			strncpy(macBuff, mac, sizeof(macBuff)); 		// 复制<mac>字符串到数组中
			macBuff[12] = '\0';								// 添加字符串结束标记
			
			StrDelSpaceWrap(macBuff);
			//需要把自己的mac地址去除掉
			if(strncmp(sg_selfMac,macBuff,strlen(sg_selfMac)) == 0)
			{
				dPrint(DEBUG,"自身的mac地址:%s\n",sg_selfMac);	
			}
			else
			{
				//获取mac/信噪比/信号强度等信息
				GetChargingStationTopoInfo(mactoken,stationArray,count)	
				count++;
				dPrint(DEBUG,"获取到一个mac地址:%s\n",macBuff);	
			}
			
			if(count >2)
			{
				dPrint(WARN,"最多只能是2个充电桩\n");	
				break;
			}
		}
		mactoken = strtok(NULL, "\n");
	}
	return count;
	*/
}

int GetChargingStationTopoInfo(char *topinfo,SearchStation *stationArray,int index)
{
	//使用,分割
	char macBuff[13] = {0};
    int resultCount = 0;
	memset(macBuff,0,sizeof(macBuff));				
	char *mac = topinfo +strlen("+ok=");			// 跳过前缀部分，获取<mac>字符串
	strncpy(macBuff, mac, 12); 						// 复制<mac>字符串到数组中
	macBuff[12] = '\0';								// 添加字符串结束标记		
	ToUpperCase(macBuff);							
	//mac地址转换为大写字母
    char** result = Split(topinfo,',',&resultCount);
    if(resultCount <6)
    {
        dPrint(DERROR,"GetChargingStationTopoInfo failed resultCount:%d\n",resultCount);
    }
    else
    {
		memset(stationArray[index].mac,0,sizeof(stationArray[index].mac));
		strncpy(stationArray[index].mac,macBuff,sizeof(stationArray[index].mac));
		stationArray[index].snr = atoi(result[5]);
        stationArray[index].atten = atoi(result[6]);
        dPrint(DEBUG,"获取到[%d]个mac地址:%s,信噪比:%s,衰减:%s\n",index,stationArray[index].mac,result[5],result[6]);	
	}
    for(int i = 0;i<resultCount;i++)
    {
        free(result[i]);
    }
    free(result);

	return RTN_SUCCESS;
}