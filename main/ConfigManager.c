#include "ConfigManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "DeBug.h"
#include <string.h>
#include "cJSON.h"

static bool nvs_initialized = false;

/**
 * @brief 初始化配置管理模块
 * @return CONFIG_OK 成功
 * @note NVS flash 应该在调用此函数前已经初始化（通常在main函数中）
 */
void ConfigInit(void)
{
    if (nvs_initialized) {
        dPrint(WARN, "Config already initialized");
        return ;
    }

    nvs_initialized = true;
    dPrint(INFO, "Config manager initialized");
    return ;
}

/**
 * @brief 保存ConfigItem结构体到NVS（使用blob方式）
 * @param item 配置项指针
 * @return CONFIG_OK 成功，其他值表示失败
 * @note 使用固定key "config_item" 存储整个结构体
 */
ConfigStatus_t SetConfigItem(ConfigItem *item)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!item) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    // 使用blob方式存储整个结构体
    err = nvs_set_blob(handle, "config_item", item, sizeof(ConfigItem));
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set config blob: %s", esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "Config item saved successfully (size: %d bytes)", sizeof(ConfigItem));
    return CONFIG_OK;
}

/**
 * @brief 从NVS读取ConfigItem结构体（使用blob方式）
 * @param item 配置项指针，用于接收读取的数据
 * @return CONFIG_OK 成功，其他值表示失败
 * @note 使用固定key "config_item" 读取整个结构体
 */
ConfigStatus_t GetConfigItem(ConfigItem *item)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!item) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    // 获取blob大小
    size_t required_size = sizeof(ConfigItem);
    err = nvs_get_blob(handle, "config_item", item, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "Config item not found in NVS");
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get config blob: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "Config item loaded successfully (size: %d bytes)", required_size);
    return CONFIG_OK;
}

/* ================ 整型值读写函数实现 ================ */

ConfigStatus_t SetConfigInt32(const char *key, int32_t value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_set_i32(handle, key, value);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set int32 '%s': %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "Set int32 '%s' = %ld", key, (long)value);
    return CONFIG_OK;
}

ConfigStatus_t GetConfigInt32(const char *key, int32_t *value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key || !value) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    err = nvs_get_i32(handle, key, value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "Key '%s' not found", key);
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get int32 '%s': %s", key, esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "Get int32 '%s' = %ld", key, (long)*value);
    return CONFIG_OK;
}

ConfigStatus_t SetConfigUInt32(const char *key, uint32_t value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_set_u32(handle, key, value);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set uint32 '%s': %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "Set uint32 '%s' = %lu", key, (unsigned long)value);
    return CONFIG_OK;
}

ConfigStatus_t GetConfigUInt32(const char *key, uint32_t *value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key || !value) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    err = nvs_get_u32(handle, key, value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "Key '%s' not found", key);
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get uint32 '%s': %s", key, esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "Get uint32 '%s' = %lu", key, (unsigned long)*value);
    return CONFIG_OK;
}

ConfigStatus_t SetConfigUInt8(const char *key, uint8_t value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_set_u8(handle, key, value);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set uint8 '%s': %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "Set uint8 '%s' = %u", key, value);
    return CONFIG_OK;
}

ConfigStatus_t GetConfigUInt8(const char *key, uint8_t *value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key || !value) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    err = nvs_get_u8(handle, key, value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "Key '%s' not found", key);
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get uint8 '%s': %s", key, esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "Get uint8 '%s' = %u", key, *value);
    return CONFIG_OK;
}

/* ================ 字符串读写函数实现 ================ */

ConfigStatus_t SetConfigString(const char *key, const char *value)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key || !value) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_set_str(handle, key, value);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set string '%s': %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "Set string '%s' = '%s'", key, value);
    return CONFIG_OK;
}

ConfigStatus_t GetConfigString(const char *key, char *value, size_t max_len)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key || !value || max_len == 0) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    size_t required_size = max_len;
    err = nvs_get_str(handle, key, value, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "Key '%s' not found", key);
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get string '%s': %s", key, esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "Get string '%s' = '%s'", key, value);
    return CONFIG_OK;
}

/* ================ 删除配置函数实现 ================ */

ConfigStatus_t EraseConfig(const char *key)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!key) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_ERASE;
    }

    err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        dPrint(DERROR, "Failed to erase key '%s': %s", key, esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_ERASE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_ERASE;
    }

    dPrint(INFO, "Erased config key '%s'", key);
    return CONFIG_OK;
}

ConfigStatus_t EraseAllConfig(void)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_ERASE;
    }

    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to erase all: %s", esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_ERASE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_ERASE;
    }

    dPrint(INFO, "Erased all config items");
    return CONFIG_OK;
}

/* ================ JSON配置读写函数实现 ================ */

#define EV_CONFIG_JSON_KEY "ev_config_json"

/**
 * @brief 保存EV配置JSON数据到Flash
 */
ConfigStatus_t SetEVConfigJSON(const char *json_string)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!json_string) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    // 验证JSON格式
    cJSON *json = cJSON_Parse(json_string);
    if (!json) {
        dPrint(DERROR, "Invalid JSON format");
        return CONFIG_ERROR_FLASH_WRITE;
    }
    if (!cJSON_IsArray(json)) {
        dPrint(DERROR, "JSON must be an array");
        cJSON_Delete(json);
        return CONFIG_ERROR_FLASH_WRITE;
    }
    cJSON_Delete(json);

    // 保存JSON字符串到NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_set_str(handle, EV_CONFIG_JSON_KEY, json_string);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to set EV config JSON: %s", esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_ERROR_FLASH_WRITE;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to commit: %s", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_WRITE;
    }

    dPrint(INFO, "EV config JSON saved successfully");
    return CONFIG_OK;
}

/**
 * @brief 从Flash读取EV配置JSON数据
 */
ConfigStatus_t GetEVConfigJSON(char *buffer, size_t buffer_size)
{
    if (!nvs_initialized) {
        return CONFIG_ERROR_NVS_NOT_INIT;
    }
    if (!buffer || buffer_size == 0) {
        return CONFIG_ERROR_NULL_POINTER;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        dPrint(DERROR, "Failed to open NVS: %s\n", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    size_t required_size = buffer_size;
    err = nvs_get_str(handle, EV_CONFIG_JSON_KEY, buffer, &required_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        dPrint(WARN, "EV config JSON not found\n");
        return CONFIG_ERROR_NOT_FOUND;
    } else if (err != ESP_OK) {
        dPrint(DERROR, "Failed to get EV config JSON: %s\n", esp_err_to_name(err));
        return CONFIG_ERROR_FLASH_READ;
    }

    dPrint(INFO, "EV config JSON loaded successfully\n");
    return CONFIG_OK;
}