#include "ConfigManagerController.h"
#include "DeBug.h"
#include "cJSON.h"
#include "GPIOManager.h"
#include "OTAManager.h"
#include "WifiManager.h"
#include "BL0942Meter.h"
#include <sys/time.h>
#include <time.h>

// 时间校准标志，记录是否已经校准过系统时间
static BOOL g_time_calibrated = FALSE;

int SetConfig(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    dPrint(DEBUG,"收到手机APP发送的设置配置请求 request:%s\n",request);

    // 解析请求JSON
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse SetConfig request JSON\n");
        return RTN_FAIL;
    }

    // 获取data对象
    cJSON *dataItem = cJSON_GetObjectItem(requestJson, "data");
    if (dataItem == NULL || !cJSON_IsObject(dataItem)) {
        dPrint(DERROR, "data字段不存在或不是对象类型\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    // 获取configname字段
    cJSON *confignameItem = cJSON_GetObjectItem(dataItem, "configname");
    if (confignameItem == NULL || !cJSON_IsString(confignameItem)) {
        dPrint(DERROR, "configname字段不存在或不是字符串类型\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    char *configname = confignameItem->valuestring;
    dPrint(DEBUG, "ConfigName: %s\n", configname);

    // 根据configname分发到不同的处理函数
    if (strcmp(configname, "InflowMaxCurrent") == 0) {
        // 处理最大流入电流设置
        cJSON *inflowItem = cJSON_GetObjectItem(dataItem, "InflowMaxCurrent");
        if (inflowItem != NULL && cJSON_IsNumber(inflowItem)) {
            uint8_t inflowMaxCurrent = (uint8_t)inflowItem->valueint;
            dPrint(INFO, "设置最大流入电流: %u A\n", inflowMaxCurrent);

            // 调用GPIO管理器设置入户电流
            int ret = GPIOManager_SetInletCurrent(inflowMaxCurrent);
            if (ret == 0) {
                dPrint(INFO, "成功设置入户电流: %u A\n", inflowMaxCurrent);
            } else {
                dPrint(WARN, "设置入户电流失败, ret=%d\n", ret);
            }

            sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"success\"},\"code\":200,\"data\":{\"InflowMaxCurrent\":%u}}", inflowMaxCurrent);
            *responseLen = strlen(response);
        }
    }
    else if (strcmp(configname, "wifi") == 0) {
        // 处理WiFi配置设置
        cJSON *wifiItem = cJSON_GetObjectItem(dataItem, "wifi");
        if (wifiItem != NULL && cJSON_IsObject(wifiItem)) {
            // 将wifi对象转换为JSON字符串
            char *wifiJsonStr = cJSON_PrintUnformatted(wifiItem);
            if (wifiJsonStr != NULL) {
                dPrint(INFO, "设置WiFi配置: %s\n", wifiJsonStr);

                // 调用WiFi管理器保存配置
                int ret = WifiManager_SetConfig(wifiJsonStr);
                cJSON_free(wifiJsonStr);

                if (ret == 0) {
                    dPrint(INFO, "WiFi配置设置成功\n");
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"success\"},\"data\":{\"configname\":\"wifi\"}}");
                } else {
                    dPrint(DERROR, "WiFi配置设置失败, ret=%d\n", ret);
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":500,\"data\":{\"error\":\"Failed to set wifi config\"}}");
                }
                *responseLen = strlen(response);
            } else {
                dPrint(DERROR, "转换WiFi配置为JSON字符串失败\n");
                sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":500,\"data\":{\"error\":\"Failed to convert wifi config\"}}");
                *responseLen = strlen(response);
            }
        } else {
            dPrint(DERROR, "wifi字段不存在或不是对象类型\n");
            sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":400,\"data\":{\"error\":\"Invalid wifi config\"}}");
            *responseLen = strlen(response);
        }
    }
    else if (strcmp(configname, "bl0942_coef") == 0) {
        // 处理BL0942系数配置设置
        cJSON *coefItem = cJSON_GetObjectItem(dataItem, "bl0942_coef");
        if (coefItem != NULL && cJSON_IsObject(coefItem)) {
            // 将系数对象转换为JSON字符串
            char *coefJsonStr = cJSON_PrintUnformatted(coefItem);
            if (coefJsonStr != NULL) {
                dPrint(INFO, "设置BL0942系数配置: %s\n", coefJsonStr);

                // 调用BL0942管理器保存配置
                int ret = bl0942_set_coef(coefJsonStr);
                cJSON_free(coefJsonStr);

                if (ret == 0) {
                    dPrint(INFO, "BL0942系数配置设置成功\n");
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"success\"},\"data\":{\"configname\":\"bl0942_coef\"}}");
                } else {
                    dPrint(DERROR, "BL0942系数配置设置失败, ret=%d\n", ret);
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":500,\"data\":{\"error\":\"Failed to set bl0942_coef\"}}");
                }
                *responseLen = strlen(response);
            } else {
                dPrint(DERROR, "转换BL0942系数配置为JSON字符串失败\n");
                sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":500,\"data\":{\"error\":\"Failed to convert bl0942_coef\"}}");
                *responseLen = strlen(response);
            }
        } else {
            dPrint(DERROR, "bl0942_coef字段不存在或不是对象类型\n");
            sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"code\":400,\"data\":{\"error\":\"Invalid bl0942_coef config\"}}");
            *responseLen = strlen(response);
        }
    }
    else if (strcmp(configname, "CurrentTime") == 0) {
        // 检查是否已经校准过，如果已校准，直接返回成功
        if (g_time_calibrated) {
            dPrint(INFO, "系统时间已经校准过，跳过本次校准请求\n");
            sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"success\"},\"data\":{\"configname\":\"CurrentTime\"}}");
            *responseLen = strlen(response);
            cJSON_Delete(requestJson);
            return RTN_SUCCESS;
        }

        // 获取 CurrentTime 字段（Unix时间戳字符串，毫秒）
        cJSON *timeItem = cJSON_GetObjectItem(dataItem, "CurrentTime");
        if (timeItem != NULL && cJSON_IsString(timeItem)) {
            // 读取毫秒级别的时间戳
            unsigned long long timestamp_ms = (unsigned long long)atoll(timeItem->valuestring);

            // 验证时间戳的合理性（例如：大于 2020-01-01 00:00:00 UTC，即 1577836800000 毫秒）
            if (timestamp_ms < 1577836800000ULL) {
                dPrint(DERROR, "时间戳无效（毫秒）: %llu\n", timestamp_ms);
                sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"data\":{\"error\":\"Invalid timestamp\"}}");
                *responseLen = strlen(response);
            } else {
                // 毫秒转换为秒和微秒
                time_t timestamp_sec = (time_t)(timestamp_ms / 1000);
                long timestamp_usec = (long)((timestamp_ms % 1000) * 1000);

                // 设置系统时间
                struct timeval tv = {
                    .tv_sec = timestamp_sec,
                    .tv_usec = timestamp_usec
                };

                int ret = settimeofday(&tv, NULL);
                if (ret == 0) {
                    // 设置成功，标记为已校准
                    g_time_calibrated = TRUE;

                    // 打印当前时间以验证
                    struct tm timeinfo;
                    localtime_r(&timestamp_sec, &timeinfo);
                    char strftime_buf[64];
                    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

                    dPrint(INFO, "系统时间校准成功: %s.%03lld (timestamp_ms: %llu)\n",
                           strftime_buf, timestamp_ms % 1000, timestamp_ms);
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"success\"},\"data\":{\"configname\":\"CurrentTime\"}}");
                } else {
                    dPrint(DERROR, "设置系统时间失败, ret=%d\n", ret);
                    sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"data\":{\"error\":\"Failed to set system time\"}}");
                }
                *responseLen = strlen(response);
            }
        } else {
            dPrint(DERROR, "CurrentTime字段不存在或不是字符串类型\n");
            sprintf(response, "{\"ret\":{\"ConfigManager.SetConfig\":\"failed\"},\"data\":{\"error\":\"Invalid CurrentTime field\"}}");
            *responseLen = strlen(response);
        }
    }

    cJSON_Delete(requestJson);
    return RTN_SUCCESS;
}

int GetConfig(char *method,char *request,int requestLen,char *response,int *responseLen)
{
    dPrint(DEBUG,"收到手机APP发送的查询配置请求 request:%s\n",request);

    // 解析请求JSON
    cJSON *requestJson = cJSON_Parse(request);
    if (requestJson == NULL) {
        dPrint(DERROR, "Failed to parse GetConfig request JSON\n");
        return RTN_FAIL;
    }

    // 获取data对象
    cJSON *dataItem = cJSON_GetObjectItem(requestJson, "data");
    if (dataItem == NULL || !cJSON_IsObject(dataItem)) {
        dPrint(DERROR, "data字段不存在或不是对象类型\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    // 获取configname字段
    cJSON *confignameItem = cJSON_GetObjectItem(dataItem, "configname");
    if (confignameItem == NULL || !cJSON_IsString(confignameItem)) {
        dPrint(DERROR, "configname字段不存在或不是字符串类型\n");
        cJSON_Delete(requestJson);
        return RTN_FAIL;
    }

    char *configname = confignameItem->valuestring;
    dPrint(DEBUG, "GetConfig ConfigName: %s\n", configname);

    // 根据configname分发到不同的处理函数
    if (strcmp(configname, "InflowMaxCurrent") == 0) {
        // 查询最大流入电流
        uint8_t inflowMaxCurrent = GPIOManager_GetInletCurrent();
        dPrint(INFO, "查询到入户电流: %u A\n", inflowMaxCurrent);

        // 返回成功响应
        sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"success\"},\"data\":{\"configname\":\"InflowMaxCurrent\",\"InflowMaxCurrent\":%u}}", inflowMaxCurrent);
        *responseLen = strlen(response);
    }
    else if (strcmp(configname, "version") == 0) {
        // 查询固件版本号
        char version[16] = {0};
        esp_err_t ret = OTAManager_GetVersion(version, sizeof(version));

        if (ret == ESP_OK) {
            dPrint(INFO, "查询到固件版本号: %s\n", version);
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"success\"},\"data\":{\"configname\":\"version\",\"version\":\"%s\"}}", version);
        } else if (ret == ESP_ERR_NOT_FOUND) {
            dPrint(WARN, "固件版本号未设置\n");
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"failed\"},\"code\":404,\"data\":{\"error\":\"Version not found\"}}");
        } else {
            dPrint(DERROR, "查询固件版本号失败, ret=%d\n", ret);
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"failed\"},\"code\":500,\"data\":{\"error\":\"Failed to get version\"}}");
        }
        *responseLen = strlen(response);
    }
    else if (strcmp(configname, "wifi") == 0) {
        // 查询WiFi配置
        char wifiConfigBuffer[1024] = {0};
        int ret = WifiManager_GetConfig(wifiConfigBuffer, sizeof(wifiConfigBuffer));

        if (ret == 0) {
            dPrint(INFO, "查询到WiFi配置: %s\n", wifiConfigBuffer);
            // 构造返回的JSON响应
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"success\"},\"data\":{\"configname\":\"wifi\",\"wifi\":%s}}", wifiConfigBuffer);
        } else {
            dPrint(DERROR, "查询WiFi配置失败, ret=%d\n", ret);
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"failed\"},\"code\":404,\"data\":{\"error\":\"WiFi config not found\"}}");
        }
        *responseLen = strlen(response);
    }
    else if (strcmp(configname, "bl0942_coef") == 0) {
        // 查询BL0942系数配置
        char coefConfigBuffer[512] = {0};
        int ret = bl0942_get_coef(coefConfigBuffer, sizeof(coefConfigBuffer));

        if (ret == 0) {
            dPrint(INFO, "查询到BL0942系数配置: %s\n", coefConfigBuffer);
            // 构造返回的JSON响应
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"success\"},\"data\":{\"configname\":\"bl0942_coef\",\"bl0942_coef\":%s}}", coefConfigBuffer);
        } else {
            dPrint(DERROR, "查询BL0942系数配置失败, ret=%d\n", ret);
            sprintf(response, "{\"ret\":{\"ConfigManager.GetConfig\":\"failed\"},\"code\":404,\"data\":{\"error\":\"BL0942 coef config not found\"}}");
        }
        *responseLen = strlen(response);
    }

    cJSON_Delete(requestJson);
    return RTN_SUCCESS;
}

BOOL IsSystemTimeCalibrated(void)
{
    return g_time_calibrated;
}