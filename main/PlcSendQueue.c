#include "PlcSendQueue.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "types.h"
#include "DeBug.h"

/* 队列节点结构 */
typedef struct queue_node {
    char *data;              // 数据指针
    int len;                 // 数据长度
    struct queue_node *prev; // 前一个节点
    struct queue_node *next; // 后一个节点
} queue_node_t;

/* 队列全局变量 */
static queue_node_t *queue_head = NULL;  // 队列头（前端）
static queue_node_t *queue_tail = NULL;  // 队列尾（后端）
static SemaphoreHandle_t queue_mutex = NULL;  // 互斥锁
static int queue_count = 0;  // 队列元素数量

/**
 * @brief 初始化队列
 */
int queue_init(void)
{
    if (queue_mutex != NULL) {
        // 已经初始化
        return RTN_SUCCESS;
    }

    queue_mutex = xSemaphoreCreateMutex();
    if (queue_mutex == NULL) {
        return RTN_FAIL;
    }

    queue_head = NULL;
    queue_tail = NULL;
    queue_count = 0;

    return RTN_SUCCESS;
}

/**
 * @brief 销毁队列，释放所有资源
 */
void queue_deinit(void)
{
    if (queue_mutex == NULL) {
        return;
    }

    xSemaphoreTake(queue_mutex, portMAX_DELAY);

    // 释放所有节点
    queue_node_t *current = queue_head;
    while (current != NULL) {
        queue_node_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }

    queue_head = NULL;
    queue_tail = NULL;
    queue_count = 0;

    xSemaphoreGive(queue_mutex);
    vSemaphoreDelete(queue_mutex);
    queue_mutex = NULL;
}

/**
 * @brief 检查数据是否已存在于队列中
 * @note 调用前必须已获取mutex
 */
static int is_data_exist(const char *data, int len)
{
    queue_node_t *current = queue_head;

    while (current != NULL) {
        // 比较数据长度和内容
        if (current->len == len && memcmp(current->data, data, len) == 0) {
            return 1;  // 存在
        }
        if(strncmp(current->data,data,strlen(current->data)) == 0)
        {
            return 1;  // 存在
        }
        current = current->next;
    }

    return 0;  // 不存在
}

/**
 * @brief 从队列前端插入数据（不允许重复数据）
 */
int queue_push_front(char *data, int len)
{
    if (data == NULL || len <= 0) {
        return RTN_FAIL;
    }

    if (queue_mutex == NULL) {
        return RTN_FAIL;
    }
    
    xSemaphoreTake(queue_mutex, portMAX_DELAY);

    // 检查队列大小（使用queue_count而不是queue_size()避免死锁）
    if(queue_count > 20)
    {
        //元素数量大于20，就返回错误
        dPrint(WARN,"queue_push_front failed queue_count:%d > 20\n",queue_count);
        xSemaphoreGive(queue_mutex);  // 必须释放互斥锁
        return RTN_FAIL;
    }
    // 检查数据是否已存在
    if (is_data_exist(data, len)) {
        xSemaphoreGive(queue_mutex);
        return RTN_FAIL;  // 数据已存在，不允许重复插入
    }

    // 创建新节点
    queue_node_t *new_node = (queue_node_t *)malloc(sizeof(queue_node_t));
    if (new_node == NULL) {
        xSemaphoreGive(queue_mutex);
        return RTN_FAIL;
    }

    // 分配数据内存并复制数据
    new_node->data = (char *)malloc(len);
    if (new_node->data == NULL) {
        free(new_node);
        xSemaphoreGive(queue_mutex);
        return RTN_FAIL;
    }

    memcpy(new_node->data, data, len);
    new_node->len = len;
    new_node->prev = NULL;
    new_node->next = queue_head;

    // 插入到队列前端
    if (queue_head != NULL) {
        queue_head->prev = new_node;
    } else {
        // 队列为空，新节点也是尾节点
        queue_tail = new_node;
    }

    queue_head = new_node;
    queue_count++;

    xSemaphoreGive(queue_mutex);
    return RTN_SUCCESS;
}

/**
 * @brief 从队列后端取出数据并删除
 */
int queue_pop_back(char *data, int *len)
{
    if (data == NULL || len == NULL) {
        return RTN_FAIL;
    }

    if (queue_mutex == NULL) 
    {
        dPrint(DERROR,"queue_mutex == NULL\n");
        return RTN_FAIL;
    }

    xSemaphoreTake(queue_mutex, portMAX_DELAY);

    // 检查队列是否为空
    if (queue_tail == NULL) 
    {
        //dPrint(DERROR,"queue_tail == NULL\n");
        xSemaphoreGive(queue_mutex);
        return RTN_FAIL;
    }

    // 检查缓冲区是否足够大
    if (*len < queue_tail->len) 
    {
        dPrint(DERROR,"*len:%d < queue_tail->len:%d\n",*len,queue_tail->len);
        xSemaphoreGive(queue_mutex);
        return RTN_FAIL;
    }

    // 复制数据到外部缓冲区
    memcpy(data, queue_tail->data, queue_tail->len);
    *len = queue_tail->len;

    // 从队列中删除尾节点
    queue_node_t *old_tail = queue_tail;
    queue_tail = queue_tail->prev;

    if (queue_tail != NULL) 
    {
        queue_tail->next = NULL;
    } 
    else 
    {
        // 队列变为空
        queue_head = NULL;
    }

    // 释放节点内存
    free(old_tail->data);
    free(old_tail);
    queue_count--;

    xSemaphoreGive(queue_mutex);
    return RTN_SUCCESS;
}

/**
 * @brief 获取队列中的元素数量
 */
int queue_size(void)
{
    int size = 0;

    if (queue_mutex == NULL) {
        return 0;
    }

    xSemaphoreTake(queue_mutex, portMAX_DELAY);
    size = queue_count;
    xSemaphoreGive(queue_mutex);

    return size;
}

/**
 * @brief 清空队列
 */
void queue_clear(void)
{
    if (queue_mutex == NULL) {
        return;
    }

    xSemaphoreTake(queue_mutex, portMAX_DELAY);

    // 释放所有节点
    queue_node_t *current = queue_head;
    while (current != NULL) {
        queue_node_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }

    queue_head = NULL;
    queue_tail = NULL;
    queue_count = 0;

    xSemaphoreGive(queue_mutex);
}
