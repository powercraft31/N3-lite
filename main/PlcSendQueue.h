#ifndef __QUEUE_H__
#define __QUEUE_H__
/*
*PLC发送队列
*/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化队列
 * @return 0 成功, -1 失败
 */
int queue_init(void);

/**
 * @brief 销毁队列，释放所有资源
 */
void queue_deinit(void);

/**
 * @brief 从队列前端插入数据（不允许重复数据）
 * @param data 要插入的数据指针（外部提供内存）
 * @param len 数据长度
 * @return 0 成功, -1 失败（如数据已存在或内存不足）
 */
int queue_push_front(char *data, int len);

/**
 * @brief 从队列后端取出数据并删除
 * @param data 用于存储取出数据的缓冲区（外部提供内存）
 * @param len 输入时为缓冲区大小，输出时为实际数据长度
 * @return 0 成功, -1 失败（如队列为空或缓冲区不足）
 */
int queue_pop_back(char *data, int *len);

/**
 * @brief 获取队列中的元素数量
 * @return 队列中的元素数量
 */
int queue_size(void);

/**
 * @brief 清空队列
 */
void queue_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __QUEUE_H__ */
