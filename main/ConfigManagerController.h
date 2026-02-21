#ifndef __CONFIG_MANAGER_CONTROLLER_H__
#define __CONFIG_MANAGER_CONTROLLER_H__
#include "types.h"
#include <string.h>
#include <stdio.h>
/********************************************************
	*@Function name:SetConfig
	*@Description:设置配置
********************************************************************************/
int SetConfig(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:GetConfig
	*@Description:获取配置
********************************************************************************/
int GetConfig(char *method,char *request,int requestLen,char *response,int *responseLen);

/********************************************************
	*@Function name:IsSystemTimeCalibrated
	*@Description:查询系统时间是否已经校准
	*@Return: TRUE-已校准, FALSE-未校准
*******************************************************************************/
BOOL IsSystemTimeCalibrated(void);

#endif
