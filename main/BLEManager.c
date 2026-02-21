/**
 * =============================================================================
 * BLE管理器 - 双服务架构实现
 * =============================================================================
 *
 * 功能说明：
 * 1. 心率服务(Heart Rate Service 0x180D)
 *    - 让设备在iOS系统"设置→蓝牙"中可见并可连接
 *    - 提供模拟心率数据(75 bpm)
 *
 * 2. Nordic UART Service (6E400001-B5A3-F393-E0A9-E50E24DCCA9E)
 *    - 提供双向数据通信(类似串口)
 *    - TX特性：设备→手机（通知）
 *    - RX特性：手机→设备（写入）
 *
 * 广播策略：
 *    - 主广播包：心率服务UUID + 设备名称（iOS系统识别）
 *    - 扫描响应：Nordic UART UUID（APP过滤扫描用）
 */

#include "BLEManager.h"
#include "DeBug.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "RPCServer.h"
#include "OTAManager.h"
#include "ConfigManager.h"

/* ========== 全局状态变量 ========== */
static BLEStatus_t s_ble_status = BLE_STATUS_DISCONNECTED;  // BLE连接状态
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;    // 连接句柄
static BLE_RxCallback_t s_rx_callback = NULL;                // 数据接收回调函数

/* ========== GATT特性句柄 ========== */
static uint16_t s_char_rx_handle;           // Nordic UART RX特性句柄
static uint16_t s_char_tx_handle;           // Nordic UART TX特性句柄
static uint16_t s_char_heart_rate_handle;   // 心率测量特性句柄

/* ========== BLE配置 ========== */
static uint8_t ble_addr_type;               // BLE地址类型

static QueueHandle_t s_rpc_queue = NULL;         // RPC请求队列
static TaskHandle_t s_rpc_task_handle = NULL;    // RPC处理任务句柄

/**
 * @brief RPC处理任务（支持OTA和JSON RPC）
 * @param param 任务参数（未使用）
 *
 * @note 工作流程：
 *       1. 从队列中获取请求（阻塞等待）
 *       2. 判断数据类型：
 *          - OTA帧（0x01-0x04）→ 调用OTAManager_ProcessFrame()
 *          - JSON RPC → 调用ProcessRPCRequest()
 *       3. 通过NOTIFY发送响应给手机
 */
static void rpc_task(void *param)
{
    rpc_request_t request;
    char response[RPC_MAX_DATA_LEN];
    int response_len = 0;

    while (1) {
        // 批量处理队列中的数据，每处理一个就让出CPU
        while (xQueueReceive(s_rpc_queue, &request, 0) == pdTRUE) {

            // ========== 判断数据类型 ==========
            uint8_t first_byte = request.data[0];

            if (first_byte >= 0x01 && first_byte <= 0x04) {
                // ---------- OTA帧处理 ----------
                dPrint(DEBUG, "处理OTA帧, type=0x%02X, len=%d\n", first_byte, request.len);
                OTAManager_ProcessFrame(request.data, request.len);

            } else {
                // ---------- JSON RPC处理 ----------
                dPrint(INFO, "处理RPC请求, len=%d\n", request.len);

                response_len = 0;
                int result = ProcessRPCRequest((char *)request.data, request.len, response, &response_len);

                if (result == RTN_SUCCESS && response_len > 0) {
                    // 发送响应给手机（通过NOTIFY）
                    esp_err_t err = BLEManager_SendData((uint8_t *)response, response_len);
                    if (err == ESP_OK) {
                        dPrint(INFO, "RPC响应已发送, len=%d\n", response_len);
                    } else {
                        dPrint(DERROR, "RPC响应发送失败\n");
                    }
                } else {
                    dPrint(WARN, "RPC处理失败或无响应, result=%d\n", result);
                }
            }

            // 每处理一个请求就让出CPU，防止看门狗超时 ,强制睡 1 个 Tick
            vTaskDelay(1);
        }

        // 队列已空，延迟等待
        vTaskDelay(1);  // 队列空时延迟10ms
    }
}

/**
 * @brief BLE RPC接收回调（快速入队,不阻塞BLE协议栈）
 *
 * @param data 接收到的数据
 * @param len 数据长度
 *
 * @note 此函数在BLE协议栈上下文中调用,必须快速返回！
 *       只负责将数据放入队列,实际处理在rpc_task中进行
 *       支持OTA帧（0x01-0x04）和JSON RPC帧
 */
static void ble_rpc_callback(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) {
        dPrint(WARN, "BLE收到空数据\n");
        return;
    }

    if (len > RPC_MAX_DATA_LEN) {
        dPrint(WARN, "BLE数据过长: %d bytes, 最大支持: %d\n", len, RPC_MAX_DATA_LEN);
        return;
    }

    // 构造请求（OTA帧和JSON RPC共用同一结构体）
    rpc_request_t request;
    memcpy(request.data, data, len);
    request.len = len;

    // 放入队列（快速入队，无论是OTA还是RPC）
    if (xQueueSend(s_rpc_queue, &request, 0) != pdTRUE) {
        dPrint(WARN, "队列已满,数据被丢弃\n");
    } else {
        // 简单日志区分
        uint8_t frame_type = data[0];
        if (frame_type >= 0x01 && frame_type <= 0x04) {
            dPrint(DEBUG, "OTA帧已入队, type=0x%02X, len=%d\n", frame_type, len);
        } else {
            dPrint(INFO, "RPC请求已入队, len=%d\n", len);
        }
    }
}


/**
 * @brief Nordic UART RX特性写入回调（手机→设备）
 *
 * @param conn_handle 连接句柄
 * @param attr_handle 属性句柄
 * @param ctxt GATT访问上下文
 * @param arg 用户参数
 * @return 0:成功 其他:错误码
 */
static int gatt_svr_chr_access_rx(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            // 如果设置了回调函数,则调用
            if (s_rx_callback != NULL) {
                uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
                dPrint(INFO, "BLE接收到数据,首包: %d字节, 总长: %d字节\n",
                       ctxt->om->om_len, om_len);

                uint8_t *data = malloc(om_len);
                if (data != NULL) {
                    // 复制数据到缓冲区
                    os_mbuf_copydata(ctxt->om, 0, om_len, data);
                    // 调用用户回调函数
                    s_rx_callback(data, om_len);
                    free(data);
                }
            }
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief Nordic UART TX特性读取回调（设备→手机）
 *
 * @param conn_handle 连接句柄
 * @param attr_handle 属性句柄
 * @param ctxt GATT访问上下文
 * @param arg 用户参数
 * @return 0:成功 其他:错误码
 */
static int gatt_svr_chr_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            dPrint(DEBUG, "BLE TX特性读取\n");
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief 心率测量特性访问回调
 *
 * @param conn_handle 连接句柄
 * @param attr_handle 属性句柄
 * @param ctxt GATT访问上下文
 * @param arg 用户参数
 * @return 0:成功 其他:错误码
 *
 * @note 心率数据格式符合BLE Heart Rate Profile规范：
 *       Byte 0: Flags (0x00 = UINT8格式)
 *       Byte 1: Heart Rate Value (75 bpm)
 */
static int gatt_svr_chr_access_heart_rate(uint16_t conn_handle, uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            // 返回模拟的心率数据：75 bpm
            uint8_t heart_rate_data[2] = {0x00, 75};
            int rc = os_mbuf_append(ctxt->om, &heart_rate_data, sizeof(heart_rate_data));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * =============================================================================
 * GATT服务和特性定义
 * =============================================================================
 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /**
         * 服务1：心率服务 (Heart Rate Service)
         * UUID: 0x180D (标准BLE SIG服务)
         * 作用：让设备在iOS系统蓝牙设置中可见
         */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_HEART_RATE_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // 心率测量特性 (Heart Rate Measurement)
                // UUID: 0x2A37
                // 属性: 读取 + 通知
                .uuid = BLE_UUID16_DECLARE(BLE_HEART_RATE_MEASUREMENT_UUID),
                .access_cb = gatt_svr_chr_access_heart_rate,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_char_heart_rate_handle,
            },
            {
                0, // 特性数组结束标志
            }
        },
    },
    {
        /**
         * 服务2：Nordic UART Service
         * UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
         * 作用：提供双向数据通信功能
         */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // RX特性 (手机→设备)
                // UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
                // 属性: 写入 + 无响应写入
                .uuid = BLE_UUID128_DECLARE(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                             0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e),
                .access_cb = gatt_svr_chr_access_rx,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_char_rx_handle,
            },
            {
                // TX特性 (设备→手机)
                // UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
                // 属性: 读取 + 通知
                .uuid = BLE_UUID128_DECLARE(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                             0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e),
                .access_cb = gatt_svr_chr_access_tx,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_char_tx_handle,
            },
            {
                0, // 特性数组结束标志
            }
        },
    },
    {
        0, // 服务数组结束标志
    },
};

/**
 * @brief GAP事件处理回调函数
 *
 * @param event GAP事件指针
 * @param arg 用户参数
 * @return 0:成功 其他:错误码
 */
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            /* 连接事件 */
            dPrint(INFO, "BLE连接 %s; status=%d\n",
                     event->connect.status == 0 ? "成功" : "失败",
                     event->connect.status);

            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc == 0) {
                    s_conn_handle = event->connect.conn_handle;
                    s_ble_status = BLE_STATUS_CONNECTED;
                    dPrint(INFO, "BLE已连接,conn_handle=%d\n", s_conn_handle);

                    // 请求优化的连接参数（用于提升OTA速度）
                    struct ble_gap_upd_params conn_params = {
                        .itvl_min = 6,   // 15ms (12 * 1.25ms)（最低间隔6 * 1.25ms）
                        .itvl_max = 12,   // 30ms (24 * 1.25ms)(12 * 1.25ms)
                        .latency = 0,     // 从设备延迟：0（不跳过连接事件）
                        .supervision_timeout = 400,  // 4000ms (400 * 10ms)
                        .min_ce_len = 0,
                        .max_ce_len = 0,
                    };

                    rc = ble_gap_update_params(event->connect.conn_handle, &conn_params);
                    if (rc == 0) {
                        dPrint(INFO, "已请求连接参数更新: 15-30ms间隔\n");
                    } else {
                        dPrint(WARN, "请求连接参数更新失败: rc=%d\n", rc);
                    }
                }
            } else {
                // 连接失败,重新开始广播
                BLEManager_StartAdvertising();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            /* 断开连接事件 */
            dPrint(INFO, "BLE断开连接; reason=%d\n", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_ble_status = BLE_STATUS_DISCONNECTED;

            // 重新开始广播
            BLEManager_StartAdvertising();
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            /* 广播完成事件 */
            dPrint(DEBUG, "BLE广播完成; reason=%d\n", event->adv_complete.reason);
            BLEManager_StartAdvertising();
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            /* 特性订阅事件（客户端订阅了通知）*/
            dPrint(INFO, "BLE订阅事件; conn_handle=%d attr_handle=%d\n",
                     event->subscribe.conn_handle,
                     event->subscribe.attr_handle);
            return 0;

        case BLE_GAP_EVENT_MTU:
            /* MTU协商事件 */
            dPrint(INFO, "BLE MTU更新; conn_handle=%d mtu=%d\n",
                     event->mtu.conn_handle,
                     event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* 连接参数更新完成事件 */
            if (event->conn_update.status == 0) {
                rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
                if (rc == 0) {
                    dPrint(INFO, "========== 连接参数更新成功 ==========\n");
                    dPrint(INFO, "连接间隔: %d (%.2f ms)\n",
                           desc.conn_itvl, desc.conn_itvl * 1.25);
                    dPrint(INFO, "从设备延迟: %d\n", desc.conn_latency);
                    dPrint(INFO, "监督超时: %d (%d ms)\n",
                           desc.supervision_timeout, desc.supervision_timeout * 10);
                    dPrint(INFO, "=====================================\n");
                }
            } else {
                dPrint(WARN, "连接参数更新失败; status=%d\n", event->conn_update.status);
            }
            return 0;
    }

    return 0;
}

/**
 * @brief 开始BLE广播
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_FAIL: 失败
 *
 * @note 广播包结构：
 *       主广播包(Advertisement Data, 最大31字节):
 *         - Flags: 通用可发现 + 仅BLE (3字节)
 *         - 心率服务UUID 0x180D (4字节)
 *         - 设备名称 "N3Lite" (8字节)
 *         - TX Power Level (3字节)
 *         总计: 18字节
 *
 *       扫描响应包(Scan Response Data, 最大31字节):
 *         - Nordic UART Service UUID (18字节) ← APP过滤扫描用
 *
 *       广播间隔: 20-100ms
 */
esp_err_t BLEManager_StartAdvertising(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Nordic UART Service UUID (128-bit)
    static const ble_uuid128_t service_uuid =
        BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                         0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

    /* ========== 配置主广播数据 ========== */
    memset(&fields, 0, sizeof(fields));
    //MAX bit = 31
    //flag = 3 + UUID = 4 + TXPower = 3 + Name 2 + Size Size = 31 - 12 = 19 （device Name Max Size = 19）
    // 1. 广播标志 (3 bytes)
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // 2. 心率服务UUID (4 bytes)
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(BLE_HEART_RATE_SERVICE_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    char subDevId[64] = {0};
    ConfigStatus_t status = GetConfigString("subDevId",subDevId,sizeof(subDevId));
    if(status == CONFIG_OK)
    {
        fields.name = (uint8_t *)subDevId;
        fields.name_len = strlen(subDevId);
        fields.name_is_complete = 1;
        dPrint(INFO,"设置的蓝牙名称是:%s\n",subDevId);
    }
    else
    {
        // 3. 设备名称
        fields.name = (uint8_t *)BLE_DEVICE_NAME;
        fields.name_len = strlen(BLE_DEVICE_NAME);
        fields.name_is_complete = 1;
        dPrint(INFO,"设置的蓝牙名称是:%s\n",BLE_DEVICE_NAME);
    }

    // 4. 发射功率等级 (3 bytes)
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        dPrint(DERROR, "BLE设置广播字段失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    /* ========== 配置扫描响应数据 ========== */
    memset(&rsp_fields, 0, sizeof(rsp_fields));

    // Nordic UART Service UUID (18 bytes)
    // 用于iOS/Android APP通过Nordic UART Service UUID过滤扫描
    rsp_fields.uuids128 = (ble_uuid128_t[]){service_uuid};
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 0;  // 0表示还有其他服务(心率服务)

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        dPrint(DERROR, "BLE设置扫描响应失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    /* ========== 配置广播参数 ========== */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // 可连接的无定向广播
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // 通用可发现模式
    adv_params.itvl_min = 32;   // 20ms (32 * 0.625ms)
    adv_params.itvl_max = 160;  // 100ms (160 * 0.625ms)

    /* ========== 启动广播 ========== */
    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        dPrint(DERROR, "BLE开始广播失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    s_ble_status = BLE_STATUS_ADVERTISING;
    dPrint(INFO, "BLE广播已启动\n");
    return ESP_OK;
}

// 停止广播
esp_err_t BLEManager_StopAdvertising(void)
{
    int rc = ble_gap_adv_stop();
    if (rc != 0) {
        dPrint(DERROR, "BLE停止广播失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    s_ble_status = BLE_STATUS_DISCONNECTED;
    dPrint(INFO, "BLE广播已停止\n");
    return ESP_OK;
}

// BLE主机同步回调
static void ble_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        dPrint(DERROR, "BLE获取地址失败; rc=%d\n", rc);
        return;
    }

    // 获取BLE设备地址
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (rc != 0) {
        dPrint(DERROR, "BLE推断地址类型失败; rc=%d\n", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);
    if (rc == 0) {
        dPrint(INFO, "BLE设备地址: %02x:%02x:%02x:%02x:%02x:%02x\n",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }

    // 开始广播
    BLEManager_StartAdvertising();
}

// BLE主机任务
static void ble_host_task(void *param)
{
    dPrint(INFO, "BLE主机任务已启动\n");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// 初始化BLE管理器
esp_err_t BLEManager_Init(void)
{
    int rc;

    dPrint(INFO, "初始化BLE管理器\n");

    // 初始化NimBLE端口（这会自动初始化控制器）
    nimble_port_init();

    // 初始化GAP和GATT服务
    ble_svc_gap_init();
    ble_svc_gatt_init();
    //先获取N3Lite的序列号，
    char subDevId[64] = {0};
    ConfigStatus_t status = GetConfigString("subDevId",subDevId,sizeof(subDevId));
    if(status == CONFIG_OK)
    {
        rc = ble_svc_gap_device_name_set(subDevId);
        dPrint(INFO,"设置的GAP设备名称是:%s\n",subDevId);
    }
    else
    {
        //使用默认的蓝牙名称
        // 配置GAP设备名称
        rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
        dPrint(INFO,"设置的GAP设备名称是:%s\n",BLE_DEVICE_NAME);
    }
    
    if (rc != 0) {
        dPrint(DERROR, "BLE设置设备名称失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    // 注册GATT服务
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        dPrint(DERROR, "BLE GATT服务计数失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        dPrint(DERROR, "BLE添加GATT服务失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    // 设置同步回调
    ble_hs_cfg.sync_cb = ble_on_sync;

    // 启动BLE主机任务
    nimble_port_freertos_init(ble_host_task);

    // 创建RPC请求队列
    s_rpc_queue = xQueueCreate(RPC_QUEUE_SIZE, sizeof(rpc_request_t));
    if (s_rpc_queue == NULL) {
        dPrint(DERROR, "创建RPC队列失败\n");
        return ESP_FAIL;
    }

    // 创建RPC处理任务
    BaseType_t ret = xTaskCreate(
        rpc_task,               // 任务函数
        "rpc_task",             // 任务名称
        8192,                   // 栈大小（8KB，增大以支持WiFi扫描）
        NULL,                   // 参数
        5,                      // 优先级
        &s_rpc_task_handle      // 任务句柄
    );

    if (ret != pdPASS) {
        dPrint(DERROR, "创建RPC任务失败\n");
        return ESP_FAIL;
    }

    // 设置BLE RPC接收回调
    BLEManager_SetRxCallback(ble_rpc_callback);
    return ESP_OK;
}

// 设置数据接收回调
esp_err_t BLEManager_SetRxCallback(BLE_RxCallback_t callback)
{
    s_rx_callback = callback;
    return ESP_OK;
}

// 发送数据
esp_err_t BLEManager_SendData(uint8_t *data, uint16_t len)
{
    if (s_ble_status != BLE_STATUS_CONNECTED) {
        dPrint(WARN, "BLE未连接,无法发送数据\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        dPrint(DERROR, "BLE发送数据参数无效\n");
        return ESP_ERR_INVALID_ARG;
    }

    // 创建mbuf并填充数据
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        dPrint(DERROR, "BLE创建mbuf失败\n");
        return ESP_FAIL;
    }

    // 发送通知
    int rc = ble_gattc_notify_custom(s_conn_handle, s_char_tx_handle, om);
    if (rc != 0) {
        dPrint(DERROR, "BLE发送通知失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    dPrint(DEBUG, "BLE发送数据成功,长度: %d\n", len);
    return ESP_OK;
}

// 获取BLE状态
BLEStatus_t BLEManager_GetStatus(void)
{
    return s_ble_status;
}

// 断开连接
esp_err_t BLEManager_Disconnect(void)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        dPrint(WARN, "BLE没有活动的连接\n");
        return ESP_ERR_INVALID_STATE;
    }

    int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        dPrint(DERROR, "BLE断开连接失败; rc=%d\n", rc);
        return ESP_FAIL;
    }

    dPrint(INFO, "BLE连接已断开\n");
    return ESP_OK;
}
