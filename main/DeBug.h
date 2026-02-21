#ifndef __DEBUG_H__
#define __DEBUG_H__

//COLOR define
#define NONE         "\033[m"
#define RED          "\033[0;32;31m"
#define LIGHT_RED    "\033[1;31m"
#define GREEN        "\033[0;32;32m"
#define LIGHT_GREEN  "\033[1;32m"
#define BLUE         "\033[0;32;34m"
#define LIGHT_BLUE   "\033[1;34m"
#define DARY_GRAY    "\033[1;30m"
#define CYAN         "\033[0;36m"
#define LIGHT_CYAN   "\033[1;36m"
#define PURPLE       "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN        "\033[0;33m"
#define YELLOW       "\033[1;33m"
#define LIGHT_GRAY   "\033[0;37m"
#define WHITE        "\033[1;37m"


typedef enum
{
	FATAL = 0,
  ALERT,
	DERROR,
	WARN,
	NOTICE,
	INFO,
	DEBUG
} schedule_debug_level;

#define PRO_NAME "#[N4]:"


void EasyLogInit();


#define dPrint(DLevel,format, ...) \
{ \
    sch_printf(DLevel, PRO_NAME,__FILE__, __LINE__,format,##__VA_ARGS__);\
}


#define HexPrint(_buf, _len) \
{\
	int _m_i = 0;\
	char *_m_buf = (char *)(_buf);\
	int _m_len = (int)(_len);\
	printf("[%s:%d] \r\n", __FUNCTION__, __LINE__);\
	printf("*****************************\n");\
	for(_m_i = 0; _m_i < _m_len; _m_i++)\
	{\
		printf("\033[32m%02x \033[0m", _m_buf[_m_i] & 0xff);\
		if(!((_m_i+1) % 10)) printf("\n");\
	}\
	printf("\nsize = %d\n*****************************\n", _m_len);\
}

void sch_printf(const int level,const char *proName,const char *func,const int line,const char *format, ...);

void processSerial3Data(char data);

//禁用日志输出
void DisableEasyLog();
//重新启用日志输出
void EnableEasyLog();
//设置日志打印级别
void SetDebugLevel(schedule_debug_level level);
//获取日志级别
schedule_debug_level GetDebugLevel();

#endif
