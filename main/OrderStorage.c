#include "OrderStorage.h"
#include "ConfigManager.h"  // 使用 ConfigManager 的接口操作 Flash
#include "cJSON.h"
#include "DeBug.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// 环形队列大小
#define ORDER_QUEUE_SIZE 60
// 全局 ID 的 Key
#define GLOBAL_ID_KEY "order_global_id"
// 订单 Key 前缀
#define ORDER_KEY_PREFIX "ord_"

// 全局 ID（内存缓存）
static uint32_t g_global_id = 0;
// 初始化标志
static bool g_initialized = false;

/**
 * @brief 初始化订单存储系统
 * @description 从 ConfigManager 读取全局 ID，如果不存在则初始化为 0
 */
int OrderStorage_Init(void)
{
    if (g_initialized) {
        dPrint(WARN, "OrderStorage 已经初始化过\n");
        return 0;
    }

    // 通过 ConfigManager 读取全局 ID
    ConfigStatus_t status = GetConfigUInt32(GLOBAL_ID_KEY, &g_global_id);

    if (status == CONFIG_ERROR_NOT_FOUND) {
        // 首次使用，初始化为 0
        g_global_id = 0;
        status = SetConfigUInt32(GLOBAL_ID_KEY, g_global_id);
        if (status != CONFIG_OK) {
            dPrint(DERROR, "初始化全局 ID 失败\n");
            return -1;
        }
        dPrint(INFO, "首次初始化订单存储，全局 ID 设置为 0\n");
    } else if (status != CONFIG_OK) {
        dPrint(DERROR, "读取全局 ID 失败\n");
        return -1;
    } else {
        dPrint(INFO, "读取到订单全局 ID: %lu\n", (unsigned long)g_global_id);
    }

    g_initialized = true;
    return 0;
}

/**
 * @brief 从充电桩结构体直接组装 JSON 并保存订单
 * @param station 充电桩结构体指针
 * @return 0-成功，-1-失败
 * @description
 *   1. 全局 ID 自增
 *   2. 使用取模算法生成 Key：ord_0 ~ ord_59
 *   3. 直接从 ChargingStation 组装 JSON 字符串
 *   4. 通过 ConfigManager 接口写入 Flash（自动覆盖旧数据）
 *   5. 更新全局 ID
 */
int OrderStorage_SaveFromStation(const ChargingStation *station)
{
    if (!g_initialized) {
        dPrint(DERROR, "OrderStorage 未初始化，请先调用 OrderStorage_Init()\n");
        return -1;
    }

    if (station == NULL) {
        dPrint(DERROR, "充电桩指针为空\n");
        return -1;
    }

    // 1. 全局 ID 自增
    g_global_id++;

    // 2. 使用取模算法生成 Key
    // 公式：ord_1, ord_2, ..., ord_60（环形队列，循环覆盖）
    // slot 范围：1~60，第61条订单回到 slot 1
    uint32_t slot = ((g_global_id - 1) % ORDER_QUEUE_SIZE) + 1;
    char key[16];
    snprintf(key, sizeof(key), "%s%lu", ORDER_KEY_PREFIX, (unsigned long)slot);

    dPrint(INFO, "保存订单：global_id=%lu, slot=%lu, key=%s\n", (unsigned long)g_global_id, (unsigned long)slot, key);

    // 3. 直接组装 JSON 对象
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        dPrint(DERROR, "创建 JSON 对象失败\n");
        return -1;
    }

    // 转换并添加字段（所有字段都转为字符串）
    char temp_str[64];

    // id: 全局订单编号（从1开始累加）
    snprintf(temp_str, sizeof(temp_str), "%lu", (unsigned long)g_global_id);
    cJSON_AddStringToObject(json, "id", temp_str);

    // startTime: 如果时间已校准，转换为秒；否则传入空字符串
    if (station->isStartTimeCalibrated) {
        snprintf(temp_str, sizeof(temp_str), "%llu", station->startTime / 1000);
        cJSON_AddStringToObject(json, "startTime", temp_str);
    } else {
        cJSON_AddStringToObject(json, "startTime", "");
    }

    // endTime: 如果时间已校准，转换为秒；否则传入空字符串
    if (station->isStartTimeCalibrated) {
        snprintf(temp_str, sizeof(temp_str), "%llu", station->endTime / 1000);
        cJSON_AddStringToObject(json, "endTime", temp_str);
    } else {
        cJSON_AddStringToObject(json, "endTime", "");
    }

    // energy: Wh 转换为 kWh（浮点数）
    float energyUsed = station->energy - station->lastEnergy;  // Wh
    float energyKwh = energyUsed / 1000.0;  // 转换为 kWh
    snprintf(temp_str, sizeof(temp_str), "%.3f", energyKwh);  // 保留3位小数
    cJSON_AddStringToObject(json, "energy", temp_str);

    // duration: 秒 -> 字符串
    if(station->isStartTimeCalibrated)
    {
        int duration = (station->endTime - station->startTime) / 1000;  // 转换为秒
        snprintf(temp_str, sizeof(temp_str), "%d", duration);
        cJSON_AddStringToObject(json, "duration", temp_str);
    }else {
        snprintf(temp_str, sizeof(temp_str), "%d", station->duration);
        cJSON_AddStringToObject(json, "duration", temp_str);
    }


    // subDevId: 直接使用序列号
    cJSON_AddStringToObject(json, "subDevId", station->serialNum);

    // 转换为 JSON 字符串
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (json_str == NULL) {
        dPrint(DERROR, "生成 JSON 字符串失败\n");
        return -1;
    }

    dPrint(INFO, "订单 JSON: %s\n", json_str);

    // 4. 通过 ConfigManager 接口写入 JSON 字符串
    ConfigStatus_t status = SetConfigString(key, json_str);
    cJSON_free(json_str);  // 释放 JSON 字符串内存

    if (status != CONFIG_OK) {
        dPrint(DERROR, "写入订单失败 (key=%s)\n", key);
        return -1;
    }

    // 5. 更新全局 ID
    status = SetConfigUInt32(GLOBAL_ID_KEY, g_global_id);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "更新全局 ID 失败\n");
        return -1;
    }

    dPrint(INFO, "订单保存成功 (global_id=%lu, subDevId=%s)\n", (unsigned long)g_global_id, station->serialNum);
    return 0;
}

/**
 * @brief 读取所有有效订单，返回 JSON 数组字符串
 * @param json_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 0-成功，-1-失败
 * @description
 *   1. 计算有效订单数量（最多 60 条）
 *   2. 按保存顺序读取订单（最老的在前，最新的在后）
 *   3. 将所有订单组装成 JSON 数组返回
 */
int OrderStorage_ReadAllJSON(char *json_buffer, size_t buffer_size)
{
    if (!g_initialized) {
        dPrint(DERROR, "OrderStorage 未初始化\n");
        return -1;
    }

    if (json_buffer == NULL || buffer_size == 0) {
        dPrint(DERROR, "参数为空\n");
        return -1;
    }

    // 创建 JSON 数组
    cJSON *json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        dPrint(DERROR, "创建 JSON 数组失败\n");
        return -1;
    }

    // 计算有效订单数量（最多 60 条，或者等于 global_id）
    uint32_t total_orders = (g_global_id < ORDER_QUEUE_SIZE) ? g_global_id : ORDER_QUEUE_SIZE;

    if (total_orders == 0) {
        dPrint(INFO, "没有订单数据\n");
        // 返回空数组 "[]"
        char *empty_array = cJSON_PrintUnformatted(json_array);
        if (empty_array) {
            strncpy(json_buffer, empty_array, buffer_size - 1);
            json_buffer[buffer_size - 1] = '\0';
            cJSON_free(empty_array);
        }
        cJSON_Delete(json_array);
        return 0;
    }

    // 计算开始读取的 slot（最老的订单）
    // slot 范围：1~60（不再有 slot 0）
    // 如果 global_id < 60，从 slot 1 开始（即从第一条订单开始）
    // 如果 global_id >= 60，从下一个要覆盖的位置开始读（即最老的订单）
    uint32_t start_slot;
    if (g_global_id < ORDER_QUEUE_SIZE) {
        start_slot = 1;  // 从 slot 1 开始读
    } else {
        // 下一个要写入的 slot 就是最老的订单
        start_slot = ((g_global_id) % ORDER_QUEUE_SIZE) + 1;
    }

    dPrint(INFO, "读取订单：total=%lu, start_slot=%lu\n", (unsigned long)total_orders, (unsigned long)start_slot);

    // 按顺序读取订单（slot 范围 1~60）
    for (uint32_t i = 0; i < total_orders; i++) {
        // 计算当前 slot：从 start_slot 开始，循环范围 1~60
        uint32_t slot = ((start_slot - 1 + i) % ORDER_QUEUE_SIZE) + 1;
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", ORDER_KEY_PREFIX, (unsigned long)slot);

        // 通过 ConfigManager 读取 JSON 字符串
        char order_json_str[512];  // 单个订单 JSON 字符串缓冲区
        ConfigStatus_t status = GetConfigString(key, order_json_str, sizeof(order_json_str));

        if (status == CONFIG_ERROR_NOT_FOUND) {
            // 这个 slot 没有数据，跳过
            dPrint(WARN, "Slot %lu 没有数据 (key=%s)，跳过\n", (unsigned long)slot, key);
            continue;
        } else if (status != CONFIG_OK) {
            dPrint(DERROR, "读取订单失败 (key=%s)\n", key);
            continue;
        }

        dPrint(DEBUG, "读取订单 (slot=%lu): %s\n", (unsigned long)slot, order_json_str);

        // 解析 JSON 字符串并添加到数组
        cJSON *order_json = cJSON_Parse(order_json_str);
        if (order_json == NULL) {
            dPrint(DERROR, "解析订单 JSON 失败 (slot=%lu)\n", (unsigned long)slot);
            continue;
        }

        // 将订单对象添加到数组中
        cJSON_AddItemToArray(json_array, order_json);
    }

    // 将 JSON 数组转换为字符串
    char *result_str = cJSON_PrintUnformatted(json_array);
    cJSON_Delete(json_array);

    if (result_str == NULL) {
        dPrint(DERROR, "生成 JSON 数组字符串失败\n");
        return -1;
    }

    // 复制到输出缓冲区
    strncpy(json_buffer, result_str, buffer_size - 1);
    json_buffer[buffer_size - 1] = '\0';
    cJSON_free(result_str);

    dPrint(INFO, "成功读取订单，生成 JSON 数组\n");
    return 0;
}

/**
 * @brief 清空所有订单数据
 * @description 删除所有订单 Key 和全局 ID
 */
int OrderStorage_ClearAll(void)
{
    if (!g_initialized) {
        dPrint(DERROR, "OrderStorage 未初始化\n");
        return -1;
    }

    dPrint(INFO, "开始清空所有订单数据\n");

    // 删除所有订单 Key（通过 ConfigManager 接口）
    // slot 范围：1~60（ord_1 到 ord_60）
    for (uint32_t i = 1; i <= ORDER_QUEUE_SIZE; i++) {
        char key[16];
        snprintf(key, sizeof(key), "%s%lu", ORDER_KEY_PREFIX, (unsigned long)i);

        ConfigStatus_t status = EraseConfig(key);
        if (status != CONFIG_OK && status != CONFIG_ERROR_NOT_FOUND) {
            dPrint(WARN, "删除 key=%s 失败\n", key);
        }
    }

    // 重置全局 ID
    g_global_id = 0;
    ConfigStatus_t status = SetConfigUInt32(GLOBAL_ID_KEY, g_global_id);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "重置全局 ID 失败\n");
        return -1;
    }

    dPrint(INFO, "所有订单数据已清空\n");
    return 0;
}

/**
 * @brief 获取当前全局 ID
 * @return 当前全局 ID
 */
uint32_t OrderStorage_GetGlobalId(void)
{
    return g_global_id;
}
