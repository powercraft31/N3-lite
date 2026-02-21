#include "PlcRecvQueue.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "DeBug.h"
/* 循环缓冲区结构 */
typedef struct {
    char buffer[CCIR_QUEUE_SIZE];                   // 缓冲区数组
    int write_pos;                                   // 写指针位置
    int read_pos;                                    // 读指针位置
    int data_size;                                   // 当前数据量
    char packet_start_flag[PACKET_START_FLAG_MAX_LEN]; // 数据包起始标识
    int packet_start_flag_len;                       // 起始标识长度
    char packet_end_flag;                            // 数据包结束标识
    SemaphoreHandle_t mutex;                         // 互斥锁
} ccir_queue_t;

/* 全局缓冲区实例 */
static ccir_queue_t g_queue = {0};

/**
 * @brief 初始化循环缓冲区
 */
BOOL PlcRecvQueue_Init(void)
{
    if (g_queue.mutex != NULL) {
        // 已经初始化
        return TRUE;
    }

    g_queue.mutex = xSemaphoreCreateMutex();
    if (g_queue.mutex == NULL) {
        return FALSE;
    }

    g_queue.write_pos = 0;
    g_queue.read_pos = 0;
    g_queue.data_size = 0;
    memset(g_queue.buffer, 0, CCIR_QUEUE_SIZE);

    // 设置默认的包标识
    //memcpy(g_queue.packet_start_flag, PACKET_START_FLAG_DEFAULT, PACKET_START_FLAG_LEN_DEFAULT);
    g_queue.packet_start_flag[0] = PACKET_START_FLAG_DEFAULT;
    g_queue.packet_start_flag_len = PACKET_START_FLAG_LEN_DEFAULT;
    g_queue.packet_end_flag = PACKET_END_FLAG_DEFAULT;

    return TRUE;
}

/**
 * @brief 销毁循环缓冲区，释放资源
 */
void PlcRecvQueue_Deinit(void)
{
    if (g_queue.mutex == NULL) {
        return;
    }

    vSemaphoreDelete(g_queue.mutex);
    g_queue.mutex = NULL;
    g_queue.write_pos = 0;
    g_queue.read_pos = 0;
    g_queue.data_size = 0;
}

/**
 * @brief 设置数据包标识
 */
BOOL PlcRecvQueue_SetPacketFlag(const char *start_flag, int start_flag_len, char end_flag)
{
    if (start_flag == NULL || start_flag_len <= 0 || start_flag_len > PACKET_START_FLAG_MAX_LEN) {
        return FALSE;
    }

    if (g_queue.mutex == NULL) {
        return FALSE;
    }

    xSemaphoreTake(g_queue.mutex, portMAX_DELAY);

    // 设置起始标识
    memset(g_queue.packet_start_flag, 0, PACKET_START_FLAG_MAX_LEN);
    memcpy(g_queue.packet_start_flag, start_flag, start_flag_len);
    g_queue.packet_start_flag_len = start_flag_len;

    // 设置结束标识
    g_queue.packet_end_flag = end_flag;

    xSemaphoreGive(g_queue.mutex);
    return TRUE;
}

/**
 * @brief 将数据写入缓冲区
 * @note 缓冲区满时会循环覆盖旧数据
 */
BOOL PutPlcRecvQueueData(char *pBuf, int nBufLen)
{
    if (pBuf == NULL || nBufLen <= 0) 
    {
        dPrint(DERROR,"pBuf == NULL || nBufLen <= 0\n");
        return FALSE;
    }

    if (g_queue.mutex == NULL) 
    {
        dPrint(DERROR,"g_queue.mutex == NULL\n");
        return FALSE;
    }

    xSemaphoreTake(g_queue.mutex, portMAX_DELAY);

    // 如果写入数据超过缓冲区大小，只保留最后的数据
    if (nBufLen >= CCIR_QUEUE_SIZE) 
    {
        // 清空缓冲区，只写入最后的CCIR_QUEUE_SIZE-1字节
        memcpy(g_queue.buffer, pBuf + (nBufLen - CCIR_QUEUE_SIZE + 1), CCIR_QUEUE_SIZE - 1);
        g_queue.write_pos = CCIR_QUEUE_SIZE - 1;
        g_queue.read_pos = 0;
        g_queue.data_size = CCIR_QUEUE_SIZE - 1;
        xSemaphoreGive(g_queue.mutex);
        return TRUE;
    }

    // 写入数据
    for (int i = 0; i < nBufLen; i++) {
        g_queue.buffer[g_queue.write_pos] = pBuf[i];
        g_queue.write_pos = (g_queue.write_pos + 1) % CCIR_QUEUE_SIZE;

        // 如果缓冲区满了，移动读指针（循环覆盖）
        if (g_queue.data_size >= CCIR_QUEUE_SIZE) {
            g_queue.read_pos = (g_queue.read_pos + 1) % CCIR_QUEUE_SIZE;
        } else {
            g_queue.data_size++;
        }
    }

    xSemaphoreGive(g_queue.mutex);
    return TRUE;
}

/**
 * @brief 在循环缓冲区中查找字符
 * @param start_pos 起始位置
 * @param search_len 搜索长度
 * @param ch 要查找的字符
 * @return 找到返回位置（相对于start_pos的偏移），未找到返回-1
 */
static int find_char_in_buffer(int start_pos, int search_len, char ch)
{
    for (int i = 0; i < search_len; i++) {
        int pos = (start_pos + i) % CCIR_QUEUE_SIZE;
        if (g_queue.buffer[pos] == ch) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 在循环缓冲区中比较字符串
 * @param start_pos 起始位置
 * @param str 要比较的字符串
 * @param str_len 字符串长度
 * @return 匹配返回TRUE，不匹配返回FALSE
 */
static BOOL compare_string_in_buffer(int start_pos, const char *str, int str_len)
{
    for (int i = 0; i < str_len; i++) {
        int pos = (start_pos + i) % CCIR_QUEUE_SIZE;
        if (g_queue.buffer[pos] != str[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief 从循环缓冲区中复制数据
 * @param dest 目标缓冲区
 * @param start_pos 起始位置
 * @param len 复制长度
 */
static void copy_from_buffer(char *dest, int start_pos, int len)
{
    for (int i = 0; i < len; i++) {
        int pos = (start_pos + i) % CCIR_QUEUE_SIZE;
        dest[i] = g_queue.buffer[pos];
    }
}

/**
 * @brief 从缓冲区读出数据包
 * @note 数据包格式：以"+RECV"开头，以"\n"结尾
 */
int GetPlcRecvQueueData(char *pBuf, int BufLen)
{
    if (pBuf == NULL || BufLen <= 0) 
    {
        dPrint(DERROR,"pBuf == NULL || BufLen <= 0\n");
        return FALSE;
    }

    if (g_queue.mutex == NULL) 
    {
        dPrint(DERROR,"g_queue.mutex == NULL\n");
        return FALSE;
    }

    xSemaphoreTake(g_queue.mutex, portMAX_DELAY);

    // 检查缓冲区是否为空
    if (g_queue.data_size == 0)
    {
        //dPrint(WARN,"缓冲区是为空\n");
        xSemaphoreGive(g_queue.mutex);
        return BUF_EMPTY;
    }

    // 在缓冲区中查找起始标识
    int packet_start = -1;
    for (int i = 0; i <= g_queue.data_size - g_queue.packet_start_flag_len; i++) {
        int check_pos = (g_queue.read_pos + i) % CCIR_QUEUE_SIZE;
        if (compare_string_in_buffer(check_pos, g_queue.packet_start_flag, g_queue.packet_start_flag_len)) {
            packet_start = i;
            break;
        }
    }

    // 如果没有找到起始标识
    if (packet_start == -1) 
    {
        dPrint(WARN,"没有找到起始标识,g_queue.packet_start_flag:%c\n",g_queue.packet_start_flag[0]);
        xSemaphoreGive(g_queue.mutex);
        return BUF_NO_BAG;
    }

    // 如果起始标识不在开头，丢弃之前的数据
    if (packet_start > 0) {
        g_queue.read_pos = (g_queue.read_pos + packet_start) % CCIR_QUEUE_SIZE;
        g_queue.data_size -= packet_start;
    }

    // 从起始标识之后查找结束标识
    int search_start = (g_queue.read_pos + g_queue.packet_start_flag_len) % CCIR_QUEUE_SIZE;
    int search_len = g_queue.data_size - g_queue.packet_start_flag_len;

    if (search_len <= 0) {
        // 数据不完整，等待更多数据
        dPrint(WARN,"数据不完整，等待更多数据\n");
        xSemaphoreGive(g_queue.mutex);
        return BUF_DATA_LESS;
    }

    int end_offset = find_char_in_buffer(search_start, search_len, g_queue.packet_end_flag);

    if (end_offset == -1) 
    {
        //dPrint(WARN,"没有找到结束标识，数据不完整\n");
        // 没有找到结束标识，数据不完整
        dPrint(WARN,"没有找到结束标识，数据不完整\n");
        xSemaphoreGive(g_queue.mutex);
        return BUF_DATA_LESS;
    }

    // 计算完整包的长度（包括起始标识和结束标识）
    int packet_len = g_queue.packet_start_flag_len + end_offset + 1;

    // 检查输出缓冲区是否足够大
    if (packet_len > BufLen) {
        dPrint(WARN,"检查输出缓冲区是否足够大\n");
        xSemaphoreGive(g_queue.mutex);
        return FALSE;
    }

    // 复制数据包到输出缓冲区
    copy_from_buffer(pBuf, g_queue.read_pos, packet_len);

    // 更新读指针和数据计数
    g_queue.read_pos = (g_queue.read_pos + packet_len) % CCIR_QUEUE_SIZE;
    g_queue.data_size -= packet_len;

    xSemaphoreGive(g_queue.mutex);
    return packet_len;
}

/**
 * @brief 清空循环缓冲区
 */
void PlcRecvQueue_Clear(void)
{
    if (g_queue.mutex == NULL) {
        return;
    }

    xSemaphoreTake(g_queue.mutex, portMAX_DELAY);

    g_queue.write_pos = 0;
    g_queue.read_pos = 0;
    g_queue.data_size = 0;
    memset(g_queue.buffer, 0, CCIR_QUEUE_SIZE);

    xSemaphoreGive(g_queue.mutex);
}

/**
 * @brief 获取缓冲区中的数据量
 */
int PlcRecvQueue_GetDataSize(void)
{
    int size = 0;

    if (g_queue.mutex == NULL) {
        return 0;
    }

    xSemaphoreTake(g_queue.mutex, portMAX_DELAY);
    size = g_queue.data_size;
    xSemaphoreGive(g_queue.mutex);

    return size;
}
