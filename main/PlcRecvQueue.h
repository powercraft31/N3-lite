#ifndef __CCIRQUEUE_H__
#define __CCIRQUEUE_H__
/********************************************************************************
* @File name:PlcRecvQueue
* @Author:shen
* @ModifyDate:2025-11-08
* @ModifyAuthor:shen
* @ModifyContent:
* @Description:PLC接收队列处理以+RECV开头的，以\n结尾的数据
********************************************************************************/
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 循环缓冲区默认大小 */
#define CCIR_QUEUE_SIZE         1024

/* 数据包起始标识（默认值） */
#define PACKET_START_FLAG_DEFAULT          '<'
#define PACKET_START_FLAG_LEN_DEFAULT      1

/* 数据包结束标识（默认值） */
#define PACKET_END_FLAG_DEFAULT         '>'

/* 数据包起始标识最大长度 */
#define PACKET_START_FLAG_MAX_LEN       32

/* 返回值定义 */
#define NOT_SET_BAG_FLAG        (-2)    // 没有设置包标识
#define BUF_EMPTY               (-3)    // 缓冲区空
#define BUF_NO_BAG              (-4)    // 缓冲区没有合法包
#define BUF_DATA_LESS           (-5)    // 缓冲区找到数据包，但是数据包不完整

/**
 * @brief 初始化循环缓冲区
 * @return TRUE 成功, FALSE 失败
 */
BOOL PlcRecvQueue_Init(void);

/**
 * @brief 销毁循环缓冲区，释放资源
 */
void PlcRecvQueue_Deinit(void);

/**
 * @brief 设置数据包标识
 * @param start_flag 数据包起始标识字符串
 * @param start_flag_len 起始标识长度（不超过PACKET_START_FLAG_MAX_LEN）
 * @param end_flag 数据包结束标识字符
 * @return TRUE 成功, FALSE 失败
 * @note 必须在CCirQueue_Init()之后调用，未调用则使用默认值
 */
BOOL PlcRecvQueue_SetPacketFlag(const char *start_flag, int start_flag_len, char end_flag);

/**
 * @brief 将数据写入缓冲区
 * @param pBuf 要写入的数据指针
 * @param nBufLen 数据长度
 * @return TRUE 成功, FALSE 失败
 * @note 缓冲区满时会循环覆盖旧数据
 */
BOOL PutPlcRecvQueueData(char *pBuf, int nBufLen);

/**
 * @brief 从缓冲区读出数据包
 * @param pBuf 读取的数据保存缓冲区（外部提供内存）
 * @param BufLen 输出缓冲区的长度
 * @return 成功返回包数据长度（>0），失败返回错误码（<0）
 *         NOT_SET_BAG_FLAG - 没有设置包标识
 *         BUF_EMPTY - 缓冲区空
 *         BUF_NO_BAG - 缓冲区没有合法包
 *         BUF_DATA_LESS - 缓冲区找到数据包，但是数据包不完整
 *         FALSE - pBuf为NULL或长度为0
 * @note 数据包格式：以"+RECV"开头，以"\n"结尾
 */
int GetPlcRecvQueueData(char *pBuf, int BufLen);

/**
 * @brief 清空循环缓冲区
 */
void PlcRecvQueue_Clear(void);

/**
 * @brief 获取缓冲区中的数据量
 * @return 缓冲区中的字节数
 */
int PlcRecvQueue_GetDataSize(void);

#ifdef __cplusplus
}
#endif

#endif /* __CCIRQUEUE_H__ */
