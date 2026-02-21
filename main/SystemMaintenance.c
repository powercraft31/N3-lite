#include "SystemMaintenance.h"
#include "BL0942Meter.h"
#include "ConfigManager.h"
#include "GPIOManager.h"
#include "PlcModule3121.h"

int GetSystemInfo(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    char subDevId[64] = {0};

    GetConfigString("subDevId",subDevId,sizeof(subDevId));
    //获取电表信息
    BL0942_Data_t meter_data;
    bl0942_get_data(&meter_data);

    int runtime = 0;

    uint8_t inflowMaxCurrent = GPIOManager_GetInletCurrent();
    /*
    {
	"ret":{"SystemTabService.GetSystemInfo":"success"},
	"data":
	{
		"subDevId":"",	//N3Lite序列号
		"plcmac":"",
	
		"ct_current":12.0,
		"ct_voltage":220.0
		"ct_power": 254.0
		"ct_energy":256.0
		"ct_frequency":12345.0

		"runtime":1245665,
		"InflowMaxCurrent":63
	}
    }
    */
    memset(response,0,strlen(response));
    sprintf(response,"%s%s%s%s%s%d%s%d%s%d%s%lu%s%d%s%d%s%d%s","{\"ret\":{\"SystemTabService.GetSystemInfo\":\"success\"},\"data\":{\"subDevId\":\"",subDevId,"\",\"plcmac\":\"",GetSelfMac(),"\",\"ct_current\":\"",meter_data.current,"\",\"ct_voltage\":\"",meter_data.voltage,"\",\"ct_power\":\"",meter_data.power,"\",\"ct_energy\":\"",meter_data.energy,"\",\"ct_frequency\":\"",meter_data.frequency,"\",\"runtime\":\"",runtime,"\",\"InflowMaxCurrent\":\"",inflowMaxCurrent,"\"}}");

    return RTN_SUCCESS;
}