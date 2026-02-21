#include "WifiManager.h"
#include "ConfigManager.h"
#include "DeBug.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// MAC地址格式化宏
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

// 全局变量
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;
static WifiStatus_t s_wifi_status = WIFI_STATUS_DISCONNECTED;

// WiFi配置全局变量
static WifiMode_t s_wifi_mode = WIFI_MODE_CONFIG_AP;

static WifiApConfig_t s_ap_config = {
    .ssid = ESP_WIFI_SSID,
    .password = ESP_WIFI_PASS
};

static WifiStaConfig_t s_sta_config = {
    .ssid = "",
    .password = ""
};

// WiFi扫描结果存储（用于分页）
static wifi_ap_record_t *s_scan_results = NULL;
static uint16_t s_scan_results_count = 0;

// IP事件处理
void ip_event_handler(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        dPrint(INFO, "获取到IP:" IPSTR "\n", IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_wifi_status = WIFI_STATUS_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// WiFi事件处理
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    // AP模式事件
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        dPrint(INFO, "设备已连接, MAC: " MACSTR "\n", MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        dPrint(INFO, "设备已断开, MAC: " MACSTR "\n", MAC2STR(event->mac));
    }
    // STA模式事件
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        dPrint(INFO, "STA模式已启动\n");
        // 不自动连接，由wifi_connect_sta()显式调用， 避免在扫描时自动连接导致冲突
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            dPrint(INFO, "重试连接WiFi (%d/%d)\n", s_retry_num, MAX_RETRY);
            s_wifi_status = WIFI_STATUS_CONNECTING;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            dPrint(DERROR, "连接WiFi失败\n");
            s_wifi_status = WIFI_STATUS_FAILED;
        }
    }
}

// ==================== AP模式功能 ====================
void wifi_init_softap(void)
{
    // 创建默认AP网络接口
    esp_netif_create_default_wifi_ap();

    // 初始化WiFi配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册WiFi事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // 配置AP参数（使用全局s_ap_config）
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.ap.ssid, s_ap_config.ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(s_ap_config.ssid);
    wifi_config.ap.channel = ESP_WIFI_CHANNEL;
    wifi_config.ap.max_connection = MAX_STA_CONN;

    // 根据密码设置加密方式
    if (strlen(s_ap_config.password) == 0) {
        // 开放式WiFi（无密码）
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.password[0] = '\0';
    } else {
        // WPA2加密
        strncpy((char *)wifi_config.ap.password, s_ap_config.password, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    // 设置AP模式并启动
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    dPrint(INFO, "AP模式启动完成. SSID:%s 加密:%s\n",
             s_ap_config.ssid, strlen(s_ap_config.password) > 0 ? "WPA2" : "开放");
}

// ==================== STA模式功能 ====================
void wifi_init_sta(void)
{
    // 创建事件组
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
    }

    // 创建默认STA网络接口
    esp_netif_create_default_wifi_sta();

    // 初始化WiFi配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        NULL));

    // 设置STA模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    dPrint(INFO, "STA模式初始化完成\n");
}

bool wifi_connect_sta(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        dPrint(DERROR, "SSID不能为空\n");
        return false;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 显式调用连接（不依赖WIFI_EVENT_STA_START事件自动连接）
    s_wifi_status = WIFI_STATUS_CONNECTING;
    s_retry_num = 0;  // 重置重试计数
    ESP_ERROR_CHECK(esp_wifi_connect());

    dPrint(INFO, "开始连接WiFi SSID:%s\n", ssid);

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        dPrint(INFO, "连接WiFi成功\n");
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        dPrint(DERROR, "连接WiFi失败\n");
        return false;
    }

    dPrint(DERROR, "连接WiFi超时\n");
    return false;
}

WifiStatus_t wifi_get_status(void)
{
    return s_wifi_status;
}

// ==================== 配置管理功能 ====================
// 从JSON字符串解析WiFi配置到全局变量
static void parse_wifi_config(const char *json_str)
{
    if (!json_str) {
        dPrint(DERROR, "JSON字符串为空\n");
        return;
    }

    dPrint(DEBUG, "解析WiFi配置: %s\n", json_str);

    // 解析JSON
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        dPrint(DERROR, "解析JSON失败\n");
        return;
    }

    // 解析wifimode
    cJSON *mode_item = cJSON_GetObjectItem(root, "wifimode");
    if (mode_item && cJSON_IsString(mode_item)) {
        if (strcmp(mode_item->valuestring, "sta") == 0 || strcmp(mode_item->valuestring, "STA") == 0) {
            s_wifi_mode = WIFI_MODE_CONFIG_STA;
        } else {
            s_wifi_mode = WIFI_MODE_CONFIG_AP;
        }
    }

    // 解析AP配置
    cJSON *ap_obj = cJSON_GetObjectItem(root, "ap");
    if (ap_obj && cJSON_IsObject(ap_obj)) {
        cJSON *ssid_item = cJSON_GetObjectItem(ap_obj, "ssid");
        if (ssid_item && cJSON_IsString(ssid_item)) {
            strncpy(s_ap_config.ssid, ssid_item->valuestring, sizeof(s_ap_config.ssid) - 1);
            s_ap_config.ssid[sizeof(s_ap_config.ssid) - 1] = '\0';
        }

        cJSON *password_item = cJSON_GetObjectItem(ap_obj, "password");
        if (password_item && cJSON_IsString(password_item)) {
            strncpy(s_ap_config.password, password_item->valuestring, sizeof(s_ap_config.password) - 1);
            s_ap_config.password[sizeof(s_ap_config.password) - 1] = '\0';
        }
    }

    // 解析STA配置
    cJSON *sta_obj = cJSON_GetObjectItem(root, "sta");
    if (sta_obj && cJSON_IsObject(sta_obj)) {
        cJSON *ssid_item = cJSON_GetObjectItem(sta_obj, "ssid");
        if (ssid_item && cJSON_IsString(ssid_item)) {
            strncpy(s_sta_config.ssid, ssid_item->valuestring, sizeof(s_sta_config.ssid) - 1);
            s_sta_config.ssid[sizeof(s_sta_config.ssid) - 1] = '\0';
        }

        cJSON *password_item = cJSON_GetObjectItem(sta_obj, "password");
        if (password_item && cJSON_IsString(password_item)) {
            strncpy(s_sta_config.password, password_item->valuestring, sizeof(s_sta_config.password) - 1);
            s_sta_config.password[sizeof(s_sta_config.password) - 1] = '\0';
        }
    }

    cJSON_Delete(root);

    dPrint(INFO, "WiFi配置解析完成 - Mode:%s, AP:%s, STA:%s\n",
           s_wifi_mode == WIFI_MODE_CONFIG_AP ? "AP" : "STA",
           s_ap_config.ssid, s_sta_config.ssid);
}

// WiFi重启并应用配置
void wifi_restart_and_apply_config(void)
{
    dPrint(INFO, "WiFi配置已更新，准备重新启动WiFi...\n");

    // 停止当前WiFi
    esp_wifi_stop();

    // 根据新的模式重新初始化WiFi
    if (s_wifi_mode == WIFI_MODE_CONFIG_STA) {
        dPrint(INFO, "重新初始化为STA模式并连接WiFi\n");
        wifi_init_sta();

        // 连接到配置的WiFi
        if (strlen(s_sta_config.ssid) > 0) {
            bool connected = wifi_connect_sta(s_sta_config.ssid, s_sta_config.password);
            if (connected) {
                dPrint(INFO, "成功连接到WiFi: %s\n", s_sta_config.ssid);
            } else {
                dPrint(WARN, "连接WiFi失败: %s\n", s_sta_config.ssid);
            }
        } else {
            dPrint(WARN, "STA模式下SSID为空，未连接WiFi\n");
        }
        
    } else {
        dPrint(INFO, "重新初始化为AP模式\n");
        wifi_init_softap();
    }

    dPrint(INFO, "WiFi配置应用成功\n");
}

// 设置WiFi配置
int WifiManager_SetConfig(const char *wifi_json)
{
    if (!wifi_json) {
        dPrint(DERROR, "WiFi配置JSON为空\n");
        return -1;
    }

    dPrint(INFO, "收到WiFi配置: %s\n", wifi_json);

    // 保存到NVS
    ConfigStatus_t ret = SetConfigString(NVS_WIFI_CONFIG, wifi_json);
    if (ret != CONFIG_OK) {
        dPrint(DERROR, "保存WiFi配置到NVS失败, ret=%d\n", ret);
        return -1;
    }

    // 解析并更新全局配置
    parse_wifi_config(wifi_json);

    // 重启WiFi并应用新配置
    wifi_restart_and_apply_config();

    return 0;
}

// 获取WiFi配置
int WifiManager_GetConfig(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        dPrint(DERROR, "缓冲区参数无效\n");
        return -1;
    }

    // 直接从NVS读取
    if (GetConfigString(NVS_WIFI_CONFIG, buffer, buffer_size) != CONFIG_OK) {
        dPrint(WARN, "未找到WiFi配置\n");
        return -1;
    }

    dPrint(INFO, "WiFi配置读取成功\n");
    return 0;
}

// WiFi扫描列表（返回JSON格式，支持分页）
int WifiManager_SelectWifiList(char *buffer, size_t buffer_size, int pageIndex, int *totalPages)
{
    if (!buffer || buffer_size == 0) {
        dPrint(DERROR, "缓冲区参数无效\n");
        return -1;
    }

    #define PAGE_SIZE 3  // 每页3个WiFi
    // 检查WiFi模式，只能在STA模式下扫描
    wifi_mode_t current_mode;
    esp_err_t ret = esp_wifi_get_mode(&current_mode);
    if (ret != ESP_OK || (current_mode != WIFI_MODE_STA && current_mode != WIFI_MODE_APSTA)) {
        dPrint(DERROR, "WiFi扫描需要在STA模式下进行，当前模式不正确\n");
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    // 确保WiFi已启动
    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        // ESP_ERR_WIFI_STATE 表示WiFi已经启动，这是正常的
        dPrint(DERROR, "启动WiFi失败: %d\n", ret);
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    // pageIndex=0时，执行扫描
    if (pageIndex == 0) {
        // 检查当前连接状态
        wifi_ap_record_t ap_info;
        bool is_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

        if (!is_connected) {
            // 如果未连接（可能正在连接或重连），先断开，避免冲突
            dPrint(INFO, "WiFi未连接或正在连接，先停止连接过程\n");
            esp_wifi_disconnect();
            // 等待断开完成，确保WiFi空闲
            vTaskDelay(pdMS_TO_TICKS(500));
            dPrint(INFO, "已断开WiFi，准备扫描\n");
        } else {
            dPrint(INFO, "WiFi已连接，可以直接扫描\n");
        }

        // 释放旧的扫描结果
        if (s_scan_results != NULL) {
            free(s_scan_results);
            s_scan_results = NULL;
            s_scan_results_count = 0;
        }

        dPrint(INFO, "开始扫描WiFi...\n");

        // 配置WiFi扫描参数
        wifi_scan_config_t scan_config = {
            .ssid = NULL,           // 扫描所有SSID
            .bssid = NULL,          // 扫描所有BSSID
            .channel = 0,           // 扫描所有信道
            .show_hidden = true,    // 显示隐藏SSID
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,  // 主动扫描，更快
            .scan_time = {
                .active = {
                    .min = 100,     // 每个信道最少扫描100ms
                    .max = 300      // 每个信道最多扫描300ms
                }
            }
        };

        ret = esp_wifi_scan_start(&scan_config, true);
        if (ret != ESP_OK) {
            dPrint(DERROR, "启动WiFi扫描失败: %d\n", ret);
            snprintf(buffer, buffer_size, "[]");
            return -1;
        }

        // 获取扫描结果数量
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);

        if (ap_count == 0) {
            dPrint(INFO, "未扫描到WiFi\n");
            snprintf(buffer, buffer_size, "[]");
            if (totalPages) *totalPages = 0;
            return 0;
        }

        // 分配内存存储扫描记录
        s_scan_results = malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (!s_scan_results) {
            dPrint(DERROR, "分配扫描结果内存失败\n");
            snprintf(buffer, buffer_size, "[]");
            return -1;
        }

        // 获取扫描记录
        s_scan_results_count = ap_count;
        ret = esp_wifi_scan_get_ap_records(&s_scan_results_count, s_scan_results);
        if (ret != ESP_OK) {
            dPrint(DERROR, "获取扫描结果失败: %d\n", ret);
            free(s_scan_results);
            s_scan_results = NULL;
            s_scan_results_count = 0;
            snprintf(buffer, buffer_size, "[]");
            return -1;
        }

        dPrint(INFO, "WiFi扫描完成，找到 %d 个网络\n", s_scan_results_count);
    }

    // 检查是否有扫描结果
    if (s_scan_results == NULL || s_scan_results_count == 0) {
        dPrint(DERROR, "没有扫描结果，请先执行pageIndex=0的扫描\n");
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    wifi_ap_record_t *ap_records = s_scan_results;
    uint16_t actual_count = s_scan_results_count;

    // 计算总页数
    int total_pages = (actual_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (totalPages) {
        *totalPages = total_pages;
    }

    // 检查页码有效性
    if (pageIndex < 0 || pageIndex >= total_pages) {
        dPrint(DERROR, "页码无效: %d, 总页数: %d\n", pageIndex, total_pages);
        free(ap_records);
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    // 计算当前页的起始和结束索引
    uint16_t start_index = pageIndex * PAGE_SIZE;
    uint16_t end_index = start_index + PAGE_SIZE;
    if (end_index > actual_count) {
        end_index = actual_count;
    }

    // 获取当前连接的WiFi信息
    wifi_ap_record_t current_ap;
    bool is_connected = (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK);
    char connected_ssid[33] = {0};
    if (is_connected) {
        strncpy(connected_ssid, (char *)current_ap.ssid, sizeof(connected_ssid) - 1);
    }

    // 构建JSON数组
    cJSON *wifi_array = cJSON_CreateArray();
    if (!wifi_array) {
        dPrint(DERROR, "创建JSON数组失败\n");
        free(ap_records);
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    // 只添加当前页的WiFi
    for (uint16_t i = start_index; i < end_index; i++) {
        cJSON *wifi_item = cJSON_CreateObject();
        if (!wifi_item) continue;

        // SSID
        cJSON_AddStringToObject(wifi_item, "ssid", (char *)ap_records[i].ssid);

        // BSSID (MAC地址)
        char bssid_str[18];
        snprintf(bssid_str, sizeof(bssid_str), MACSTR, MAC2STR(ap_records[i].bssid));
        cJSON_AddStringToObject(wifi_item, "bssid", bssid_str);

        // 信号强度
        cJSON_AddNumberToObject(wifi_item, "signal_level", ap_records[i].rssi);

        // 频率（根据信道计算）
        int frequency = 2412;  // 默认2.4GHz
        if (ap_records[i].primary >= 1 && ap_records[i].primary <= 14) {
            frequency = 2407 + ap_records[i].primary * 5;
        } else if (ap_records[i].primary >= 36) {
            frequency = 5000 + ap_records[i].primary * 5;
        }
        cJSON_AddNumberToObject(wifi_item, "frequency", frequency);

        // 加密方式标志
        char flags[128] = {0};
        if (ap_records[i].authmode == WIFI_AUTH_OPEN) {
            strcpy(flags, "[OPEN][ESS]");
        } else if (ap_records[i].authmode == WIFI_AUTH_WEP) {
            strcpy(flags, "[WEP][ESS]");
        } else if (ap_records[i].authmode == WIFI_AUTH_WPA_PSK) {
            strcpy(flags, "[WPA-PSK-CCMP][ESS]");
        } else if (ap_records[i].authmode == WIFI_AUTH_WPA2_PSK) {
            strcpy(flags, "[WPA2-PSK-CCMP][ESS]");
        } else if (ap_records[i].authmode == WIFI_AUTH_WPA_WPA2_PSK) {
            strcpy(flags, "[WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]");
        } else {
            strcpy(flags, "[ESS]");
        }
        cJSON_AddStringToObject(wifi_item, "flags", flags);

        // 连接状态
        const char *connect_status = "-";  // 默认未知
        if (is_connected && strcmp((char *)ap_records[i].ssid, connected_ssid) == 0) {
            connect_status = "connected";
        } else if (strlen(s_sta_config.ssid) > 0 &&
                   strcmp((char *)ap_records[i].ssid, s_sta_config.ssid) == 0) {
            connect_status = "disconnect";  // 已知但未连接
        }
        cJSON_AddStringToObject(wifi_item, "connectStatus", connect_status);

        cJSON_AddItemToArray(wifi_array, wifi_item);
    }
    
    // 转换为字符串
    char *json_str = cJSON_PrintUnformatted(wifi_array);
    cJSON_Delete(wifi_array);

    if (!json_str) {
        dPrint(DERROR, "生成WiFi列表JSON失败\n");
        snprintf(buffer, buffer_size, "[]");
        return -1;
    }

    strncpy(buffer, json_str, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    cJSON_free(json_str);

    dPrint(INFO, "WiFi扫描完成，总共 %d 个网络，当前第 %d 页，共 %d 页\n",
           actual_count, pageIndex, total_pages);
    return 0;
}

// ==================== WiFi管理器初始化 ====================
esp_err_t wifi_manager_init(void)
{
    char json_buffer[1024] = {0};
    char subDevId[32] = {0};
    char ap_ssid[32] = {0};
    dPrint(INFO, "WiFi管理器初始化中...\n");
    // 1. 获取设备ID
    ConfigStatus_t status = GetConfigString("subDevId", subDevId, sizeof(subDevId));
    if (status == CONFIG_OK && strlen(subDevId) > 0) {
        // 限制subDevId长度，确保完整SSID不超过31字节 (32-1)
        // "ESP32_AP_" = 9字节，最多留22字节给ID
        char truncated_id[23] = {0};
        strncpy(truncated_id, subDevId, sizeof(truncated_id) - 1);
        snprintf(ap_ssid, sizeof(ap_ssid), "ESP32_AP_%s", truncated_id);
    } else {
        // 没有设备ID，使用默认宏
        strncpy(ap_ssid, ESP_WIFI_SSID, sizeof(ap_ssid) - 1);
    }

    // 2. 尝试从NVS读取WiFi配置
    ConfigStatus_t ret = GetConfigString(NVS_WIFI_CONFIG, json_buffer, sizeof(json_buffer));

    if (ret == CONFIG_OK) {
        // 找到配置，解析到全局变量
        dPrint(INFO, "找到已保存的WiFi配置\n");
        parse_wifi_config(json_buffer);
    } else {
        // 没有配置，创建默认AP模式配置并保存
        dPrint(INFO, "未找到WiFi配置，创建默认AP模式配置\n");

        // 设置默认值
        s_wifi_mode = WIFI_MODE_CONFIG_AP;
        strncpy(s_ap_config.ssid, ap_ssid, sizeof(s_ap_config.ssid) - 1);
        strncpy(s_ap_config.password, ESP_WIFI_PASS, sizeof(s_ap_config.password) - 1);
        s_sta_config.ssid[0] = '\0';
        s_sta_config.password[0] = '\0';

        // 生成默认JSON并保存
        snprintf(json_buffer, sizeof(json_buffer),
            "{\"ap\":{\"ssid\":\"%s\",\"password\":\"%s\"},"
            "\"sta\":{\"ssid\":\"\",\"password\":\"\"},"
            "\"wifimode\":\"ap\"}",
            s_ap_config.ssid, s_ap_config.password);

        SetConfigString(NVS_WIFI_CONFIG, json_buffer);
        dPrint(INFO, "默认配置已保存: %s\n", json_buffer);
    }

    // 3. 根据配置启动相应模式
    if (s_wifi_mode == WIFI_MODE_CONFIG_AP) {
        dPrint(INFO, "启动AP模式 (SSID:%s)...\n", s_ap_config.ssid);
        wifi_init_softap();
    } else if (s_wifi_mode == WIFI_MODE_CONFIG_STA) {
        dPrint(INFO, "启动STA模式...\n");
        wifi_init_sta();

        // 如果有STA配置，尝试连接
        if (strlen(s_sta_config.ssid) > 0) {
            dPrint(INFO, "尝试连接到WiFi: %s\n", s_sta_config.ssid);
            wifi_connect_sta(s_sta_config.ssid, s_sta_config.password);
        } else {
            dPrint(INFO, "STA模式已初始化，等待配置\n");
        }
    }

    dPrint(INFO, "WiFi管理器初始化完成\n");
    return ESP_OK;
}
