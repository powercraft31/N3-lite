#include "DeBug.h"
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "PortDeBugService.h"

//打印输出级别
schedule_debug_level g_uiPutOutLevel = DEBUG;

// 打印信息级别

char *PutOutLevel[] =
{
    (char *)"[FATEL ]",
    (char *)"[ALERT ]",
    (char *)"[DERROR ]",
    (char *)"[ WARM ]",
    (char *)"[NOTICE]",
    (char *)"[ INFO ]",
    (char *)"[DEBUG ]"
};

// 打印信息颜色
char *PutOutColor[] =
{
    (char *)LIGHT_RED, //FATEL
    (char *)PURPLE,  // ALERT
    (char *)RED,     // DERROR
    (char *)YELLOW,  // WARM
    (char *)BLUE,    // NOTICE
    (char *)CYAN,   // INFO
    (char *)GREEN     // DEBUG
};

void EasyLogInit()
{
	//初始化串口
	//uart3_init(115200);
	//uart3_register_recv_callback(processSerial3Data);
}


void sch_printf(const int level,const char *proName,const char *func,const int line,const char *format, ...)
{
    va_list args;

    if ((level > DEBUG) || (level < FATAL))
    {
        printf("tm printf input err level %d\n", level);
        return;
    }

    if (level > g_uiPutOutLevel)
   {
        return;
   }

    char timeBuf[32] = {0};

    va_start(args, format);
    printf("%s:%-8s: %s %s [%d]: ",PutOutColor[level],
            PutOutLevel[level],\
        	proName,func,line);
    vprintf(format, args);
    printf(NONE);
    va_end(args);
}



/*!
    \brief      处理串口3接收到的数据
    \param[in]  data: 接收到的字节
    \param[out] none
    \retval     none
*/
void processSerial3Data(char data)
{
	HandleDebugSerialData(data);
    return;
}

//禁用日志输出
void DisableEasyLog()
{
    g_uiPutOutLevel = FATAL;
}
//重新启用日志输出
void EnableEasyLog()
{
    g_uiPutOutLevel = DEBUG;
}
//设置日志打印级别
void SetDebugLevel(schedule_debug_level level)
{
    g_uiPutOutLevel = level;
}
//获取日志级别
schedule_debug_level GetDebugLevel()
{
    return g_uiPutOutLevel;
}