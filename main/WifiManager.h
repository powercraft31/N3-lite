#ifndef __WIFI_MANAGER_H__
#define __WIFI_MANAGER_H__
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// WiFi AP配置参数（开放式WiFi，无密码）
#define ESP_WIFI_SSID      "ESP32_AP_zengziao"
#define ESP_WIFI_PASS      ""
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       4

// WiFi STA配置参数
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          5

// WiFi配置存储的NVS键名
#define NVS_WIFI_CONFIG         "wifi_config"

// WiFi模式枚举
typedef enum {
    WIFI_MODE_CONFIG_AP = 0,    // AP配网模式
    WIFI_MODE_CONFIG_STA = 1,   // STA工作模式
} WifiMode_t;

// AP配置结构体
typedef struct {
    char ssid[32];              // AP SSID
    char password[32];          // AP 密码
} WifiApConfig_t;

// STA配置结构体
typedef struct {
    char ssid[32];              // WiFi SSID
    char password[32];          // WiFi 密码
} WifiStaConfig_t;

// WiFi连接状态
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED,
} WifiStatus_t;

// 函数声明
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// WiFi管理器初始化
esp_err_t wifi_manager_init(void);

// AP模式相关
void wifi_init_softap(void);

// STA模式相关
void wifi_init_sta(void);
bool wifi_connect_sta(const char *ssid, const char *password);
WifiStatus_t wifi_get_status(void);

// WiFi重启并应用配置
void wifi_restart_and_apply_config(void);

// WiFi扫描
typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;
} WifiScanResult_t;

// 配置读写接口
int WifiManager_SetConfig(const char *wifi_json);
int WifiManager_GetConfig(char *buffer, size_t buffer_size);

// WiFi扫描列表（返回JSON格式，支持分页）
int WifiManager_SelectWifiList(char *buffer, size_t buffer_size, int pageIndex, int *totalPages);

#endif
