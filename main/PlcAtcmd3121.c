#include "PlcAtcmd3121.h"


char* GetAtCmdByIdx(const char* cmdIdx)
{
	//打开回显
	if(strncmp(cmdIdx,AT_CMD_OPEN_ECHO,strlen(cmdIdx)) == 0)
	{
		return "AT+ECHO=1\r\n";
	}
	//关闭回显
	else if(strncmp(cmdIdx,AT_CMD_CLOSE_ECHO,strlen(cmdIdx)) == 0)
	{
		return "AT+ECHO=0\r\n";
	}
	//查询默认工作模式
	else if(strncmp(cmdIdx,AT_CMD_QUERY_DEFAULT_MODE,strlen(cmdIdx)) == 0)
	{
		return "AT+MODE\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_DEFAULT_DATA_MODE,strlen(cmdIdx)) == 0)
	{
		//设置默认工作透传模式
		return "AT+MODE=0\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_DEFAULT_AT_MODE,strlen(cmdIdx)) == 0)
	{
		////设置默认工作AT模式
		return "AT+MODE=2\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_MID,strlen(cmdIdx)) == 0)
	{
		//查询模组硬件型号
		return "AT+MID\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_SWVER,strlen(cmdIdx)) == 0)
	{
		//查询模组软件版本
		return "AT+GMR\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_RESET_DEFAULT,strlen(cmdIdx)) == 0)
	{
		//回复出厂设置
		return "AT+RESTORE\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_MAC,strlen(cmdIdx)) == 0)
	{
		//查询模组的MAC 地址
		return "AT+MAC\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_SET_MAC,strlen(cmdIdx)) == 0)
	{
		//设置模组的MAC 地址
		return "AT+MAC=\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QURY_SUPPORT_AT,strlen(cmdIdx)) == 0)
	{
		//查询当前支持的AT 指令
		return "AT+HELP\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QURY_NODE_NUM,strlen(cmdIdx)) == 0)
	{
		//查询网络节点数量
		return "AT+TOPONUM\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QURY_WHITE_NUM,strlen(cmdIdx)) == 0)
	{
		//获取白名单数量
		return "AT+WHNUM\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_NOTIFY,strlen(cmdIdx)) == 0)
	{
		//查询是否已经开启入网
		return "AT+NOTIFY\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_OPEN_NOTIFY,strlen(cmdIdx)) == 0)
	{
		//开启入网通知
		return "AT+NOTIFY=1\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_CLOSE_NOTIFY,strlen(cmdIdx)) == 0)
	{
		//关闭入网通知
		return "AT+NOTIFY=0\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_CCO_MAC,strlen(cmdIdx)) == 0)
	{
		//获取CCOMAC 地址
		return "AT+CCOMAC\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_ENTER,strlen(cmdIdx)) == 0)
	{
		//进入AT指令模式
		return "+++\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_LEAVE,strlen(cmdIdx)) == 0)
	{
		//退出AT指令模式
		return "AT+EXIT\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_CONFIG_BAUD,strlen(cmdIdx)) == 0)
	{
		//查询模组串口信息
		return "AT+UART\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_CONFIG_BAUD,strlen(cmdIdx)) == 0)
	{
		//配置波特率
		return "AT+UART=115200,8,1,2\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_REBOOT,strlen(cmdIdx)) == 0)
	{
		//PLC重启指令
		return "AT+UART=115200,8,1,2\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_NODES,strlen(cmdIdx)) == 0)
	{
		//查询网络全部节点信息
		return "AT+TOPOINFO=0,99\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_QUERY_WHITE_STATUS,strlen(cmdIdx)) == 0)
	{
		//查询白名单状态
		return "AT+WHSTATUS\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_ENABLE_WHITE,strlen(cmdIdx)) == 0)
	{
		//启用白名单
		return "AT+WHSTATUS=1\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_DISABLE_WHITE,strlen(cmdIdx)) == 0)
	{
		//关闭白名单
		return "AT+WHSTATUS=0\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_GET_WHINFO,strlen(cmdIdx)) == 0)
	{
		//获取全部白名单
		return "AT+WHINFO=1,99\r\n";
	}
	else if(strncmp(cmdIdx,AT_CMD_ADD_WHINFO,strlen(cmdIdx)) == 0)
	{
		//添加白名单
		return "AT+WHADD=";
	}
	else if(strncmp(cmdIdx,AT_CMD_DEL_WHINFO,strlen(cmdIdx)) == 0)
	{
		//删除白名单
		return "AT+WHDEL=";
	}
	else if(strncmp(cmdIdx,AT_CMD_CLR_WHINFO,strlen(cmdIdx)) == 0)
	{
		//清除白名单
		return "AT+WHCLR\r\n";
	}
	return "unkown";
}

char* GetAtCmdAckByIndex(const char* ackIndex)
{
	if(strncmp(ackIndex,OK_RELAY,strlen(ackIndex)) == 0)
	{
		return "ok";	
	}
	else if(strncmp(ackIndex,ERR_RELAY,strlen(ackIndex)) == 0)
	{
		return "err";	
	}
	else if(strncmp(ackIndex,AT_CMD_SYS_START_EVENT,strlen(ackIndex)) == 0)
	{
		//系统启动通知
		return "+SystemIsReady";	
	}
	else if(strncmp(ackIndex,AT_CMD_ONLINE_EVENT,strlen(ackIndex)) == 0)
	{
		//入网通知事件
		return "+online";	
	}
	else if(strncmp(ackIndex,AT_CMD_OFFLINE_EVENT,strlen(ackIndex)) == 0)
	{
		//下网通知事件es
		return "+offline";	
	}
	else if(strncmp(ackIndex,AT_CMD_TOPONUM_EVENT,strlen(ackIndex)) == 0)
	{
		//节点数量回复
		return "+TOPONUM";	
	}
	else if(strncmp(ackIndex,AT_CMD_TOPOINFO_EVENT,strlen(ackIndex)) == 0)
	{
		//节点信息回复
		return "+TOPOINFO";	
	}
	else if(strncmp(ackIndex,AT_CMD_NV_MAC_EVENT,strlen(ackIndex)) == 0)
	{
		//自己的MAC地址
		return "+MAC";	
	}
	return "";
}
//根据回复的字符串内容
char* GetAtCmdIdByAckString(const char* ackString)
{
	if(strstr(ackString,"ok") != NULL)
	{
		return OK_RELAY;	
	}
	else if(strstr(ackString,"err") != NULL)
	{
		return ERR_RELAY;	
	}
	else if(strstr(ackString,"SystemIsReady") != NULL)
	{
		return AT_CMD_SYS_START_EVENT;	
	}
	else if(strstr(ackString,"online")!= NULL)
	{
		return AT_CMD_ONLINE_EVENT;	
	}
	else if(strstr(ackString,"offline") != NULL)
	{
		return AT_CMD_OFFLINE_EVENT;	
	}
	else if(strstr(ackString,"TOPONUM") != NULL)
	{
		return AT_CMD_TOPONUM_EVENT;	
	}
	else if(strstr(ackString,"TOPOINFO") != NULL)
	{
		return AT_CMD_TOPOINFO_EVENT;	
	}
	else if(strstr(ackString,"MAC") != NULL)
	{
		return AT_CMD_NV_MAC_EVENT;	
	}
	return ERR_RELAY;
}