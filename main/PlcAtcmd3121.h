#ifndef __PLC_AT_CMD_3121_H__
#define __PLC_AT_CMD_3121_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//AT回复指令分割符,钰威讯的模组分割符是=
#define AT_CMD_SPLITER_FLAG		"="

//AT 命令和IDX对应表
#define	AT_CMD_OPEN_ECHO		  "open_echo"	 			//打开回显功能
#define	AT_CMD_CLOSE_ECHO		  "close_echo" 			//关闭回显功能
#define	AT_CMD_OPEN_LOG			  "open_log"				//打开日志
#define	AT_CMD_CLOSE_LOG		  "close_log"				//关闭日志

#define	AT_CMD_QUERY_DEFAULT_MODE "query_default_mode"	//查询默认工作模式

#define	AT_CMD_DEFAULT_DATA_MODE	"set_default_data_mode"		//设置默认工作透传模式
#define	AT_CMD_DEFAULT_AT_MODE		"set_default_at_mode"		//设置默认工作AT模式
#define	AT_CMD_QUERY_MID			"query_mid"			//查询模组硬件型号
#define	AT_CMD_QUERY_SWVER			"query_swver"		//查询模组软件版本
#define	AT_CMD_RESET_DEFAULT		"reset_default"		//回复出厂设置
#define	AT_CMD_QUERY_MAC			"query_mac"			//查询模组的MAC 地址
#define	AT_CMD_SET_MAC				"set_mac"				//设置模组的MAC 地址
#define	AT_CMD_QURY_SUPPORT_AT		"query_support_at"		//查询当前支持的AT 指令
#define	AT_CMD_QURY_NODE_NUM		"query_node_num"		//查询网络节点数量
#define	AT_CMD_QURY_WHITE_NUM		"query_white_num"		//获取白名单数量

#define	AT_CMD_QUERY_NOTIFY			"query_notify"		//查询是否已经开启入网
#define	AT_CMD_OPEN_NOTIFY			"open_notify"		//开启入网通知
#define	AT_CMD_CLOSE_NOTIFY			"close_notify"		//关闭入网通知
#define	AT_CMD_QUERY_CCO_MAC		"query_ccomac"		//获取CCOMAC 地址
	
#define	AT_CMD_ENTER				"enter_at"			//进入AT指令模式
#define	AT_CMD_LEAVE				"leave_at"			//退出AT指令模式

#define	AT_CMD_QUERY_CONFIG_BAUD	"query_baud"		//查询模组串口信息
#define	AT_CMD_CONFIG_BAUD			"config_baud"		//配置波特率
#define	AT_CMD_REBOOT				"reboot"			//PLC重启指令
#define	AT_CMD_QUERY_NODES			"query_nodes"		//查询网络全部节点信息

#define	AT_CMD_QUERY_WHITE_STATUS	"query_white_status" 	//查询白名单状态
#define	AT_CMD_ENABLE_WHITE			"enable_white"			//启用白名单
#define	AT_CMD_DISABLE_WHITE		"disable_white"			//关闭白名单
#define	AT_CMD_GET_WHINFO			"get_whiinfo"			//获取全部白名单
#define	AT_CMD_ADD_WHINFO			"add_whiinfo"			//添加白名单
#define	AT_CMD_DEL_WHINFO			"del_whiinfo"			//删除白名单
#define	AT_CMD_CLR_WHINFO			"clr_whiinfo"			//清除白名单




#define	OK_RELAY					"ok"		//"OK"
#define	ERR_RELAY					"error"		//"ERROR"
//事件通知类
#define	AT_CMD_SYS_START_EVENT		"sys_start_event"		//系统启动通知
#define	AT_CMD_ONLINE_EVENT			"online_event"			//入网通知事件
#define	AT_CMD_OFFLINE_EVENT		"offline_event"			//下网通知事件es
#define	AT_CMD_TOPONUM_EVENT		"toponum_event"			//节点数量回复
#define	AT_CMD_TOPOINFO_EVENT		"topoinfo_event"		//节点信息回复
#define	AT_CMD_NV_MAC_EVENT			"nv_mac_event"			//自己的MAC地址

/********************************************************
	*@Function name:GetAtCmdByIdx
	*@Description:根据命令ID获取AT指令
	*@input param:cmdIdx 命令ID
	*@output param:
	*@Return:AT指令
********************************************************************************/
char* GetAtCmdByIdx(const char* cmdIdx);
/********************************************************
	*@Function name:GetAtCmdAckByIndex
	*@Description:根据命令ID获取返回的AT回复字符串
	*@input param:cmdIdx 命令ID
	*@output param:
	*@Return:AT指令
********************************************************************************/
char* GetAtCmdAckByIndex(const char* ackIndex);
/********************************************************
	*@Function name:GetAtCmdIdByAckString
	*@Description:根据回复的AT字符串获取ID
	*@input param:cmdIdx 命令ID
	*@output param:
	*@Return:AT指令
********************************************************************************/
char* GetAtCmdIdByAckString(const char* ackString);


#endif
