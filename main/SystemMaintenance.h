#ifndef __SYSTEM_MAINTENANCE_H__
#define __SYSTEM_MAINTENANCE_H__
#include "types.h"
#include <stdio.h>
#include <string.h>

/********************************************************
	*@Function name:GetSystemInfo
	*@Description:获取系统信息
********************************************************************************/
int GetSystemInfo(char *method,char *request,int requestLen,char *response,int *responseLen);

#endif