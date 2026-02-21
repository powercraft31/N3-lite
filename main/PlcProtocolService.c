#include "PlcProtocolService.h"
#include "DeBug.h"
#include "ChargingStationManager.h"
#include "HexUtils.h"
#include "StringUtils.h"
#include "PlcModule3121.h"
#include "PlcManager.h"

void ProcessHexPduPacket(char *nvMac,char *hexPduPacket, int outlen)
{
    ChargingStation *station = NULL;
    char bodyBuff[256] = {0};    //PDU的数据部分
    char *PDUDataBuff = (char *)bodyBuff;
    if(outlen > 256)
    {
        dPrint(DERROR,"ProcessHexPduPacket error outlen > 256\n");
        return;
    }
    //处理PDU的packet
    dPrint(DEBUG,"ProcessHexPduPacket nvMac:%s\n",nvMac);
    //获取搜索到的充电桩
    SearchStation* searchStation = GetSearchStation(nvMac);

    //根据mac地址找到对应的已添加的充电桩对象
    station = GetChargingStation(nvMac);
    if(station != NULL)
    {
        //有数据包了之后，心跳次数设置为4
        station->heart_beat_num = SUB_DEVICE_DEFAULT_HEART_BEAT_NUM;
    }
    if(station != NULL && station->energy == 0)
    {
        //主动查询一次实时数据
        SendPlcDataByDestMac(station->mac ,SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY,strlen(SELECT_CHARGING_STATION_L1_VOLTAGE_AND_CURRENT_AND_ENERGY));
        dPrint(INFO,"编号%s的energy==0 所以主动查询一次实时数据energy\n", station->serialNum);
    }

    if (station != NULL && station->workMode == Undefine)
    {
        SendPlcDataByDestMac(station->mac ,SELECT_CHRGING_WORK_MODE_CMD, strlen(SELECT_CHRGING_WORK_MODE_CMD));
        dPrint(INFO, "编号%s的workmode == undefine , 主动查询一次实时数据workmode\n", station->serialNum);
    }
    HexPrint(hexPduPacket,outlen);
    /*
	*标识符	长度   序列号	类型	  命令ID	  数据	CRC			
	*0x7E 	2		1	 1	   1		1	 2
	*/
    PDU_HEAD *phead = (PDU_HEAD *)hexPduPacket;
    //长度需要大小端转换,把数据部分拆出来
	phead->len =  Ntohs(phead->len);
    //data的长度 = 总长度 - 头部长度 - CRC
	int dataLen = outlen-sizeof(PDU_HEAD)-2;
    //如果等于0说明data 没有内容，就把头部字段当做信息传入
	if (dataLen == 0)
    {
		memcpy(PDUDataBuff,hexPduPacket, sizeof(PDU_HEAD));
		dataLen = sizeof(PDU_HEAD);
	} 
    else 
    {
		memcpy(PDUDataBuff,hexPduPacket+sizeof(PDU_HEAD),dataLen);
	}

    //目前PDUData的数据是：
    /*
	*count	    component   instance	id	  result   length	value			
	* 1 			1	       1	     1		1	    1         n
	*/
    //先判断数据长度，
    if(dataLen <6)
    {
        dPrint(DERROR,"ProcessHexPduPacket failed dataLen <6\n");
        return;
    }
    PDU_DATA *PDUData = (PDU_DATA *)PDUDataBuff;
    //跳过count变量
    PDU_DATA_HEAD *PDUDataHead = (PDU_DATA_HEAD *)(PDUDataBuff+1);
    //通知变量结构体
    PDU_DATA_STATUS_NOTIFY *PDUDataStatusNotify = (PDU_DATA_STATUS_NOTIFY *)PDUDataBuff;
    //判断长度
    /*
    if(PDUData->length < 0)
    {
        dPrint(DERROR,"ProcessHexPduPacket failed PDUData->length <= 0\n");
        return;
    }
    */
   int count = PDUData->count;
    //先通过phead->cmd区分数据类型
    switch(phead->cmd)
    {
        case Heartbeat:
        {
            dPrint(DEBUG,"接收到了心跳包协议\n");
            break;
        }
        case GetVariables:
        {
            int value = 0;
            //需要判断有几帧数据一起来的

            dPrint(DEBUG,"接收到了获取变量协议包 数据帧个数count:%d\n",count);
            for(int i = 0;i<count;i++)
            {
                int skipcount = 0;
                if(i == 0)
                {
                    skipcount = sizeof(PDU_DATA);
                }
                else
                {
                    skipcount =  sizeof(PDU_DATA_HEAD); 
                }
                
                if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_mdcFwVersion)
                {
                    if(station != NULL)
                    {
                          //获取数据
                        memset(station->mdcFwVersion,0,sizeof(station->mdcFwVersion));
                        strncpy(station->mdcFwVersion,PDUDataBuff+skipcount,PDUDataHead->length);     
                    }
                    if(searchStation != NULL)
                    {
                        memset(searchStation->mdcFwVersion,0,sizeof(searchStation->mdcFwVersion));
                        strncpy(searchStation->mdcFwVersion,PDUDataBuff+skipcount,PDUDataHead->length);   
                        dPrint(DEBUG,"获取到了充电桩的版本,mdcFwVersion:%s\n",searchStation->mdcFwVersion);     
                    }
                      
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_serialNumber)
                {
                    if(station != NULL)
                    {
                     //获取数据
                        memset(station->serialNum,0,sizeof(station->serialNum));
                        strncpy(station->serialNum,PDUDataBuff+skipcount,PDUData->length);
                    }
                    if(searchStation != NULL)
                    {
                        memset(searchStation->serialNum,0,sizeof(searchStation->serialNum));
                        strncpy(searchStation->serialNum,PDUDataBuff+skipcount,PDUData->length);
                        //获取到了序列号，打印出来
                        dPrint(DEBUG,"获取到了充电桩的序列号是:%s\n",searchStation->serialNum);
                    }
                    
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_vendor)
                {
                     dPrint(DEBUG,"获取到了充电桩的品牌\n");   
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_model)
                {
                     dPrint(DEBUG,"获取到了充电桩的model\n");   
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_meterSerialNumber)
                {
                     dPrint(DEBUG,"获取到了充电桩的电表序列号\n");   
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_meterType)
                {
                     dPrint(DEBUG,"获取到了充电桩的电表类型\n");   
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_numberOfConnectors)
                {
                     dPrint(DEBUG,"获取到了充电桩的电表枪的个数\n");   
                }
                else if(PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_numberPhases)
                {
                     dPrint(DEBUG,"获取到了充电桩的电表枪有几相\n");   
                }
                else if (PDUDataHead->component == Basic_Component && PDUDataHead->id == PDU_DATA_ID_workMode)
                {
                    if(station != NULL)
                    {
                     //获取数据
                        char workMode = 0;
                        strncpy(&workMode,PDUDataBuff+skipcount,PDUData->length);
                        if (workMode == 0x01) {
                            station->workMode = App;
                        } else if (workMode == 0x02) {
                            station->workMode = Ocpp;
                        } else if (workMode == 0x03) {
                            station->workMode = Plc;
                        }
                        dPrint(DEBUG,"获取到了充电桩的工作模式, %d\n", workMode);
                    }
                }
                //电压
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_acVoltageL1)
                {
                    if(station != NULL)
                    {
                        unsigned short svalue =  *(unsigned short *)(PDUDataBuff+skipcount);
                        svalue = Ntohs (svalue);
                        value = svalue;
                        //电压
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,电压:%d\n",station->mac,station->serialNum,value);
                        station->acVoltageL1 = value;
                    }
                }
                //电流
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_acCurrentL1)
                {
                    if(station != NULL)
                    {
                        unsigned short svalue =  *(unsigned short *)(PDUDataBuff+skipcount);
                        svalue = Ntohs (svalue);
                        value = svalue;
                        //电流
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,电流:%d\n",station->mac,station->serialNum,value);
                        station->acCurrentL1 = value;
                    }
                }
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_energy)
                {
                    if(station != NULL)
                    {
                        unsigned int intvalue =  *(unsigned int *)(PDUDataBuff+skipcount);
                        intvalue = Ntohl (intvalue);
                        value = intvalue;
                        station->energy = (float)intvalue*1.0;
                        //能量
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,能量:%f\n",station->mac,station->serialNum,station->energy);
                        
                    }
                }
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_isPlugged)
                {
                    if(station != NULL)
                    {
                        int intvalue =  (char)*(PDUDataBuff+skipcount);
                        value = intvalue;
                        //是否插枪
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,是否插枪:%d\n",station->mac,station->serialNum,value);
                        station->isPlugged = value;
                        PublishEvent(EVENT_REPORT_STATUS,(char*)station,sizeof(ChargingStation));
                    }
                }
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_isEvseReady)
                {
                    if(station != NULL)
                    {
                        char charvalue =  (char)*(PDUDataBuff+skipcount);
                        value = charvalue;
                        //充电枪是否准备好
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,充电枪是否准备好:%d\n",station->mac,station->serialNum,value);
                        station->isEvseReady = value;
                        PublishEvent(EVENT_REPORT_STATUS,(char*)station,sizeof(ChargingStation));
                    }
                }
                else if(PDUDataHead->component == Connector_Component && PDUDataHead->id == PDU_DATA_ID_isEvReady)
                {
                    if(station != NULL)
                    {
                        char charvalue =  (char)*(PDUDataBuff+skipcount);
                        value = charvalue;
                        //汽车是否准备好
                        dPrint(DEBUG,"接收到了充电桩mac:%s,subDevId:%s,汽车是否准备好:%d\n",station->mac,station->serialNum,value);
                        station->isEvReady = value;
                        PublishEvent(EVENT_REPORT_STATUS,(char*)station,sizeof(ChargingStation));
                    }
                }
                else
                {
                    dPrint(DERROR,"unkown component:%d,id:%d\n",PDUDataHead->component,PDUDataHead->id);    
                }
                if(i != count-1)
                {
                    //缓冲区跳过头部信息+数据
                    PDUDataBuff = PDUDataBuff + skipcount;
                    PDUDataBuff = PDUDataBuff +  PDUDataHead->length;    
                    PDUDataHead = (PDU_DATA_HEAD *)(PDUDataBuff);
                    HexPrint(PDUDataHead,sizeof(PDU_DATA_HEAD));
                }
            }
            break;
        }
        case SetVariables:
        {
            dPrint(DEBUG,"接收到了设置变量协议包\n");
            break;
        }
        case StatusNotify:
        {
            if(station != NULL)
            {
                //由于状态通知数据包少一个结果字段，所有PDU_DATA的长度需要减去1
                int value = (char)*(PDUDataBuff+sizeof(PDU_DATA_STATUS_NOTIFY));
                dPrint(DEBUG,"接收到了状态通知协议包:%d\n",value);
                ProcessChargeStationStatusNotify(PDUDataStatusNotify,station,value);
            }
            break;
        }
        default:
        {
            dPrint(DEBUG,"未知的数据协议包类型:cmd:%d\n",phead->cmd);
            break;
        }
    }
    return;
}


int ProcessChargeStationStatusNotify(PDU_DATA_STATUS_NOTIFY *PDUData,ChargingStation *station,int value)
{

    if(PDUData->component == Connector_Component && PDUData->id == PDU_DATA_ID_isPlugged)
    {
        dPrint(DEBUG,"接收到了充电桩插枪状态isPlugged:%d\n",value);
        station->isPlugged = value;
    }
    else if(PDUData->component == Connector_Component && PDUData->id == PDU_DATA_ID_isEvseReady)
    {
        //充电枪是否准备好
        dPrint(DEBUG,"接收到了充电枪状态 isEvseReady:%d\n",value);
        station->isEvseReady = value;
    }
    else if(PDUData->component == Connector_Component && PDUData->id == PDU_DATA_ID_isEvReady)
    {
        //汽车是否准备好
        dPrint(DEBUG,"接收到了汽车是否准备好 isEvReady:%d\n",value);
        station->isEvReady = value;
    }
    //发布事件
    PublishEvent(EVENT_REPORT_STATUS,(char*)station,sizeof(ChargingStation));
    return RTN_SUCCESS;
}
int CombineMacAndCmd(char *mac,char *srcCmd,char *destCmd)
{
    int iRet = 0;
	strncpy(destCmd,srcCmd,strlen(srcCmd));
	iRet = StringReplace(destCmd,strlen("AT+SEND="),strlen("4080E1346353"),mac);
	if(iRet != RTN_SUCCESS)
	{
		dPrint(DERROR,"StringReplace failed\r\n");	
		return RTN_FAIL;
	}     
    return RTN_SUCCESS;
}
//获取充电桩序列号
/*
int GetChargingStationSerialNumber(char *mac,ChargingStation *station)
{

    int iRet;
    char response[256] = {0};   //原始字符串
    char rcv_buf[256] = {0};    //接收到的16进制字符串
    char outHex[128] = {0};     //获取到的整体16进制数据
    char PDUDataBuff[64] = {0};    //PDU的数据部分
    unsigned int outLen = 0;    //获取到的整体16进制数据长度

    //需要把mac地址替换掉
    CombineMacAndCmd(mac,(char *)SELECT_CHARGING_STATION_SN,response);
    //发送数据
    iRet = SendDataWithReadResult(response,strlen(response),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_2S);
    if(iRet < 0)
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed mac:%s\n",mac);
        return RTN_FAIL;
    }
    if(strlen(rcv_buf) == 0 || NULL != strstr(rcv_buf,"err"))
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed mac:%s,send:%s,rcv_buf:%s\n",mac,response,rcv_buf);  
        return RTN_FAIL;
    }
    if(rcv_buf[0] == '\r')
	{
        dPrint(INFO,"AT+SEND返回的结果中也有回车字符\n");
		delete_chars_from_position(rcv_buf,0,1);
	}
    iRet = AnalyseStringDataToHex(rcv_buf,outHex, &outLen);
    if(RTN_SUCCESS != iRet)
    {
        dPrint(DERROR,"AnalyseStringDataToHex failed rcv_buf:%s,outLen:%d\n",rcv_buf,outLen);
        return RTN_FAIL;
    }
    */
    //获取到16进制数据了，再进一步解析出序列号
    //HexPrint(outHex,outLen);
    /*
	*标识符	长度   序列号	类型	  命令ID	  数据	CRC			
	*0x7E 	2		1	 1	   1		1	 2
	*/
    /*
    PDU_HEAD *phead = (PDU_HEAD *)outHex;
    //长度需要大小端转换,把数据部分拆出来
	phead->len =  Ntohs(phead->len);
    //data的长度 = 总长度 - 头部长度 - CRC
	int dataLen = outLen-sizeof(PDU_HEAD)-2;
    //如果等于0说明data 没有内容，就把头部字段当做信息传入
	if (dataLen == 0)
    {
		memcpy(PDUDataBuff,outHex, sizeof(PDU_HEAD));
		dataLen = sizeof(PDU_HEAD);
	} 
    else 
    {
		memcpy(PDUDataBuff,outHex+sizeof(PDU_HEAD),dataLen);
	}
    */
    //目前PDUData的数据是：
    /*
	*count	    component   instance	id	  result   length	value			
	* 1 			1	       1	     1		1	    1         n
	*/
    //先判断数据长度，
    /*
    if(dataLen <6)
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed dataLen <6\n");
        return RTN_FAIL;
    }
    PDU_DATA *PDUData = (PDU_DATA *)PDUDataBuff;
    //判断长度
    if(PDUData->length <= 0)
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed PDUData->length <= 0\n");
        return RTN_FAIL;
    }
    //获取数据
    memset(station->serialNum,0,sizeof(station->serialNum));
    strncpy(station->serialNum,PDUDataBuff+sizeof(PDU_DATA),PDUData->length);
    //获取到了序列号，打印出来
    dPrint(DEBUG,"获取到了充电桩的序列号是:%s\n",station->serialNum);
    return RTN_SUCCESS;
    
}
*/
/*
int GetChargingStationFirmwareVersion(char *mac,ChargingStation *station)
{
 
    
    int iRet;
    char response[256] = {0};   //原始字符串
    char rcv_buf[256] = {0};    //接收到的16进制字符串
    char outHex[128] = {0};     //获取到的整体16进制数据
    char PDUDataBuff[64] = {0};    //PDU的数据部分
    unsigned int outLen = 0;    //获取到的整体16进制数据长度

    //需要把mac地址替换掉
    CombineMacAndCmd(mac,(char *)SELECT_CHARGING_STATION_SN,response);
    //发送数据
    iRet = SendDataWithReadResult(response,strlen(response),rcv_buf,sizeof(rcv_buf),RECEIVE_BUF_WAIT_2S);
    if(iRet < 0)
    {
        dPrint(DERROR,"GetChargingStationFirmwareVersion failed mac:%s\n",mac);
        return RTN_FAIL;
    }
    if(strlen(rcv_buf) == 0 || NULL != strstr(rcv_buf,"err"))
    {
        dPrint(DERROR,"GetChargingStationFirmwareVersion failed mac:%s,send:%s,rcv_buf:%s\n",mac,response,rcv_buf);  
        return RTN_FAIL;
    }
    if(rcv_buf[0] == '\r')
	{
        dPrint(INFO,"AT+SEND返回的结果中也有回车字符\n");
		delete_chars_from_position(rcv_buf,0,1);
	}
    iRet = AnalyseStringDataToHex(rcv_buf,outHex, &outLen);
    if(RTN_SUCCESS != iRet)
    {
        dPrint(DERROR,"AnalyseStringDataToHex failed rcv_buf:%s,outLen:%d\n",rcv_buf,outLen);
        return RTN_FAIL;
    }
    //获取到16进制数据了，再进一步解析出序列号
    HexPrint(outHex,outLen);
    */
    /*
	*标识符	长度   序列号	类型	  命令ID	  数据	CRC			
	*0x7E 	2		1	 1	   1		1	 2
	*/
   
    /*
    PDU_HEAD *phead = (PDU_HEAD *)outHex;
    //长度需要大小端转换,把数据部分拆出来
	phead->len =  Ntohs(phead->len);
    //data的长度 = 总长度 - 头部长度 - CRC
	int dataLen = outLen-sizeof(PDU_HEAD)-2;
    //如果等于0说明data 没有内容，就把头部字段当做信息传入
	if (dataLen == 0)
    {
		memcpy(PDUDataBuff,outHex, sizeof(PDU_HEAD));
		dataLen = sizeof(PDU_HEAD);
	} 
    else 
    {
		memcpy(PDUDataBuff,outHex+sizeof(PDU_HEAD),dataLen);
	}
    */
    //目前PDUData的数据是：
    /*
	*count	    component   instance	id	  result   length	value			
	* 1 			1	       1	     1		1	    1         n
	*/
    //先判断数据长度，
    /*
    if(dataLen <6)
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed dataLen <6\n");
        return RTN_FAIL;
    }
    PDU_DATA *PDUData = (PDU_DATA *)PDUDataBuff;
    //判断长度
    if(PDUData->length <= 0)
    {
        dPrint(DERROR,"GetChargingStationSerialNumber failed PDUData->length <= 0\n");
        return RTN_FAIL;
    }
    //获取数据
    memset(station->mdcFwVersion,0,sizeof(station->mdcFwVersion));
    strncpy(station->mdcFwVersion,PDUDataBuff+sizeof(PDU_DATA),PDUData->length);
    //获取到了序列号，打印出来
    dPrint(DEBUG,"获取到了充电桩的版本号是:%s\n",station->mdcFwVersion);
    return RTN_SUCCESS;
    
}
*/
