#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include <stdint.h>
#include <stddef.h>
#include "ChargingStation.h"

#define CONFIG_NVS_NAMESPACE "config"  // NVS命名空间

/* 配置管理状态码 */
typedef enum {
    CONFIG_OK = 0,                      // 操作成功
    CONFIG_ERROR_NULL_POINTER,          // 空指针错误
    CONFIG_ERROR_NVS_NOT_INIT,          // NVS未初始化
    CONFIG_ERROR_FLASH_WRITE,           // Flash写入失败
    CONFIG_ERROR_FLASH_READ,            // Flash读取失败
    CONFIG_ERROR_FLASH_ERASE,           // Flash擦除失败
    CONFIG_ERROR_NOT_FOUND              // 配置项不存在
} ConfigStatus_t;

/* DataPoints????? */
typedef struct ConfigItem{
    
} __attribute__((packed)) ConfigItem;


void ConfigInit(void);

ConfigStatus_t SetConfigItem(ConfigItem *item);

ConfigStatus_t GetConfigItem(ConfigItem *item);

/* 整型值读写接口 */
ConfigStatus_t SetConfigInt32(const char *key, int32_t value);
ConfigStatus_t GetConfigInt32(const char *key, int32_t *value);

ConfigStatus_t SetConfigUInt32(const char *key, uint32_t value);
ConfigStatus_t GetConfigUInt32(const char *key, uint32_t *value);

ConfigStatus_t SetConfigUInt8(const char *key, uint8_t value);
ConfigStatus_t GetConfigUInt8(const char *key, uint8_t *value);

/* 字符串读写接口 */
ConfigStatus_t SetConfigString(const char *key, const char *value);
ConfigStatus_t GetConfigString(const char *key, char *value, size_t max_len);

/* 删除配置项 */
ConfigStatus_t EraseConfig(const char *key);

/* 清除所有配置 */
ConfigStatus_t EraseAllConfig(void);

/* JSON配置读写接口 - 用于EV充电桩配置 */
/**
 * @brief 保存EV配置JSON数据到Flash
 * @param json_string JSON字符串（数组格式）
 * @return CONFIG_OK 成功，其他值表示失败
 * @note JSON格式: [
	{
		"subDevId": "AC000000000001",  
        "name":"充电桩",                                                   
        "mac":"4080E1346368",
		"maxlimitCurrent":"30"
	}
]
 */
ConfigStatus_t SetEVConfigJSON(const char *json_string);

/**
 * @brief 从Flash读取EV配置JSON数据
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return CONFIG_OK 成功，其他值表示失败
 */
ConfigStatus_t GetEVConfigJSON(char *buffer, size_t buffer_size);

#endif
