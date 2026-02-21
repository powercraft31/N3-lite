# N3Lite Meter HAL 重構設計文件 v1.1

> **文件版本**: 1.1
> **日期**: 2026-02-21（v1.1 修訂）
> **狀態**: 設計提案（v1.1：修正 3 項架構缺陷）
> **範圍**: 將 BL0942 電表驅動抽象為 IMeter 介面，解耦 AllocationController 對具體硬體的直接依賴

---

## 1. 設計動機

### 1.1 現行問題

目前 `AllocationController.c` 在第 164 行直接呼叫 `bl0942_get_current()`：

```c
MeterCurrVlaue = (bl0942_get_current() + 9) / 10;
```

這造成以下問題：

- **硬體耦合**: AllocationController 與 BL0942 晶片驅動緊密綁定，無法替換為其他電表型號
- **不可測試**: 無法在單元測試中注入 mock 電表
- **無資料新鮮度檢查**: 呼叫方無法得知資料是「剛更新的」還是「3 秒前的過時資料」
- **缺乏故障安全機制**: 若電表斷線，AllocationController 仍使用過時數值進行負載分配，可能導致過載

### 1.2 設計目標

1. 定義 `IMeter` 介面（C 函式指標結構體），隔離硬體實作
2. 加入 `get_last_update_timestamp()` 支援資料新鮮度檢查
3. 電表斷線 >3 秒自動觸發安全降載
4. **零破壞**: 保留所有現有核心邏輯不變

---

## 2. IMeter 介面定義

### 2.1 IMeter.h 草稿

```c
#ifndef __IMETER_H__
#define __IMETER_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 電表資料新鮮度狀態
 */
typedef enum {
    METER_DATA_FRESH,       // 資料在有效時間窗口內（<3s）
    METER_DATA_STALE,       // 資料過期（>=3s），應觸發安全降載
    METER_DATA_INVALID      // 從未成功讀取過資料
} MeterDataFreshness_t;

/**
 * @brief 電表斷線回呼函式型別
 * @param stale_duration_ms 資料過期持續時間（毫秒）
 *
 * 當電表資料超過 METER_STALE_THRESHOLD_MS 未更新時，
 * HAL 層會透過此回呼通知 AllocationController 觸發安全降載。
 */
typedef void (*MeterDisconnectCallback_t)(uint32_t stale_duration_ms);

/**
 * @brief IMeter 介面 — 電表硬體抽象層
 *
 * 所有電表實作（BL0942、模擬器、未來型號）皆須實作此介面。
 * AllocationController 僅透過此介面存取電表數據。
 */
typedef struct IMeter {
    /**
     * @brief 取得當前電壓有效值
     * @return 電壓值（**單位：0.1V**，例如 2200 = 220.0V；uint16_t 最大 6553.5V，
     *         足以覆蓋三相 480V 或直流 1000V 場景）
     *
     * ⚠️ **介面強制單位**：所有實作（BL0942、三相電表、模擬器）
     *    回傳值必須統一為 0.1V 解析度，由各 Impl 負責換算。
     */
    uint16_t (*get_voltage)(void);

    /**
     * @brief 取得當前電流有效值
     * @return 電流值（**單位：0.1A**，例如 321 = 32.1A；uint16_t 最大 6553.5A，
     *         足以覆蓋工業大電流場景）
     *
     * ⚠️ **介面強制單位**：所有實作回傳值必須統一為 0.1A 解析度，
     *    由各 Impl 負責換算。AllocationController 使用此值計算負載均衡時，
     *    直接除以 10 得安培數。
     */
    uint16_t (*get_current)(void);

    /**
     * @brief 取得當前有功功率
     * @return 功率值（單位：1W，例如 3500 = 3500W = 3.5kW）
     *
     * ⚠️ **使用 uint32_t（非 uint16_t）**：
     *   uint16_t 最大僅 65,535W（≈65kW）。
     *   三相 22kW 場景若以 0.1W 精度回傳，最大值 220,000 > 65,535，必然溢位。
     *   uint32_t 最大 4,294,967,295W，可覆蓋任何實際場景。
     */
    uint32_t (*get_power)(void);

    /**
     * @brief 取得累計用電量
     * @return 電能值（單位：0.001 kWh，例如 1500 = 1.500 kWh）
     *
     * ⚠️ **使用 uint32_t**：
     *   uint32_t 最大可累計約 4,294,967 kWh，足以支援長期計量。
     *   實作層（BL0942）須在每次成功讀取後換算為此單位再填入。
     */
    uint32_t (*get_energy)(void);

    /**
     * @brief 取得電表連線狀態
     * @return true=已連線且通訊正常, false=斷線或通訊異常
     */
    bool (*is_connected)(void);

    /**
     * @brief 取得最後一次成功更新資料的時間戳
     * @return tick count（FreeRTOS xTaskGetTickCount 回傳值）
     *         回傳 0 表示從未成功更新過
     */
    uint32_t (*get_last_update_timestamp)(void);

    /**
     * @brief 註冊電表斷線回呼
     * @param cb 斷線回呼函式，傳入 NULL 取消註冊
     *
     * 當 HAL 偵測到資料過期超過閾值（3 秒）時，
     * 將呼叫此回呼通知上層執行安全降載。
     */
    void (*register_disconnect_callback)(MeterDisconnectCallback_t cb);

} IMeter_t;

/** 電表資料過期閾值（毫秒） */
#define METER_STALE_THRESHOLD_MS  3000

/**
 * @brief 檢查電表資料新鮮度
 * @param meter 電表介面指標
 * @return MeterDataFreshness_t 資料狀態
 */
static inline MeterDataFreshness_t meter_check_freshness(const IMeter_t *meter)
{
    if (meter == NULL || meter->get_last_update_timestamp == NULL) {
        return METER_DATA_INVALID;
    }

    uint32_t last_update = meter->get_last_update_timestamp();
    if (last_update == 0) {
        return METER_DATA_INVALID;
    }

    uint32_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = (now - last_update) * portTICK_PERIOD_MS;

    if (elapsed_ms >= METER_STALE_THRESHOLD_MS) {
        return METER_DATA_STALE;
    }

    return METER_DATA_FRESH;
}

#endif /* __IMETER_H__ */
```

### 2.2 BL0942MeterImpl 實作結構（概念）

```c
// BL0942MeterImpl.c（未來實作，此處僅示意）

#include "IMeter.h"
#include "BL0942Meter.h"

static uint32_t s_last_update_tick = 0;
static MeterDisconnectCallback_t s_disconnect_cb = NULL;

static uint16_t bl0942_impl_get_voltage(void)  { /* 委派至 bl0942_data.voltage */ }
static uint16_t bl0942_impl_get_current(void)  { /* 委派至 bl0942_data.current */ }
static uint16_t bl0942_impl_get_power(void)    { /* 委派至 bl0942_data.power   */ }
static bool     bl0942_impl_is_connected(void) { /* 委派至 bl0942_is_connected() */ }
static uint32_t bl0942_impl_get_last_update_timestamp(void) { return s_last_update_tick; }

static void bl0942_impl_register_disconnect_cb(MeterDisconnectCallback_t cb) {
    s_disconnect_cb = cb;
}

/* 匯出的介面實例 */
const IMeter_t g_bl0942_meter = {
    .get_voltage                = bl0942_impl_get_voltage,
    .get_current                = bl0942_impl_get_current,
    .get_power                  = bl0942_impl_get_power,    // uint32_t
    .get_energy                 = bl0942_impl_get_energy,   // uint32_t，新增
    .is_connected               = bl0942_impl_is_connected,
    .get_last_update_timestamp  = bl0942_impl_get_last_update_timestamp,
    .register_disconnect_callback = bl0942_impl_register_disconnect_cb,
};
```

---

## 3. Before / After 資料流圖

### 3.1 Before：直接耦合

```
┌─────────────────────────┐
│  AllocationController   │
│  (AutoControl_task)     │
│                         │
│  MeterCurrVlaue =       │
│    bl0942_get_current() │──── 直接呼叫 ────┐
│    + 9) / 10;           │                  │
│                         │                  ▼
│                         │    ┌──────────────────────┐
│                         │    │   BL0942Meter.c      │
│                         │    │                      │
│                         │    │  static bl0942_data  │
│                         │    │  bl0942_get_current()│
│                         │    │  bl0942_is_connected │
│                         │    └──────────┬───────────┘
│                         │               │
│                         │               ▼
│                         │    ┌──────────────────────┐
│                         │    │  UART2 / BL0942 晶片 │
└─────────────────────────┘    └──────────────────────┘

問題:
  ✗ AllocationController 直接 #include "BL0942Meter.h"
  ✗ 直接呼叫 bl0942_get_current()，無法替換
  ✗ 無資料新鮮度概念
  ✗ 無斷線安全降載機制
```

### 3.2 After：透過 IMeter 介面解耦

```
┌─────────────────────────┐
│  AllocationController   │
│  (AutoControl_task)     │
│                         │
│  // 初始化時注入介面     │
│  static const IMeter_t  │
│    *s_meter;            │
│                         │     ┌─────────────────┐
│  freshness =            │     │   IMeter.h       │
│   meter_check_freshness │────▶│   (介面定義)     │
│   (s_meter);            │     │                  │
│                         │     │ .get_voltage()   │
│  if (STALE) → 安全降載  │     │ .get_current()   │
│                         │     │ .get_power()     │
│  curr = s_meter->       │     │ .is_connected()  │
│    get_current();       │     │ .get_last_update │
│                         │     │  _timestamp()    │
└─────────────────────────┘     │ .register_       │
                                │  disconnect_cb() │
                                └────────┬────────┘
                                         │
                          ┌──────────────┴──────────────┐
                          │                             │
                          ▼                             ▼
              ┌───────────────────┐        ┌───────────────────┐
              │ BL0942MeterImpl   │        │  MockMeterImpl    │
              │ (正式硬體實作)     │        │  (測試用模擬)     │
              │                   │        │                   │
              │ 委派至原有         │        │ 可注入任意數值     │
              │ bl0942_data 邏輯  │        │ 用於單元測試       │
              └───────┬───────────┘        └───────────────────┘
                      │
                      ▼
              ┌───────────────────┐
              │ UART2 / BL0942   │
              │ 晶片硬體          │
              └───────────────────┘

優點:
  ✓ AllocationController 僅依賴 IMeter.h
  ✓ 可輕鬆替換電表實作（不同型號 / 模擬器）
  ✓ 支援資料新鮮度檢查 (get_last_update_timestamp)
  ✓ 支援斷線回呼 → 自動觸發安全降載
  ✓ 單元測試可注入 MockMeterImpl
```

---

## 4. 必須保留的核心邏輯（請勿修改）

以下列出重構過程中**絕對不可變動**的核心元件：

### 4.1 BL0942_FRAME_INTERVAL_MS 非阻塞時序

**檔案**: `main/BL0942Meter.h:26`, `main/BL0942Meter.c:258`

```c
#define BL0942_FRAME_INTERVAL_MS  25   // 幀間隔 >20ms（手冊 3.2.6 節要求）
```

此值控制 BL0942 各暫存器讀取之間的幀間隔延時。手冊明確要求 >20ms，目前設定 25ms。
`bl0942_query_all_registers()` 中的 `vTaskDelay(pdMS_TO_TICKS(BL0942_FRAME_INTERVAL_MS))` 實現了
FreeRTOS 非阻塞等待，不佔用 CPU 時間。

**保留理由**: 硬體時序要求，改動會導致通訊失敗或資料損壞。

### 4.2 bl0942_calc_checksum 校驗邏輯

**檔案**: `main/BL0942Meter.c:295-302`

```c
uint8_t bl0942_calc_checksum(uint8_t frame_id, uint8_t reg_addr, uint8_t *pData)
{
    uint8_t sum = frame_id + reg_addr + pData[0] + pData[1] + pData[2];
    sum = sum & 0xFF;
    sum = ~sum;
    return sum;
}
```

公式: `CHECKSUM = ~((frame_id + reg_addr + DATA[0] + DATA[1] + DATA[2]) & 0xFF)`

**保留理由**: BL0942 晶片規格書定義的校驗演算法，為通訊正確性的關鍵保障。

### 4.3 bl0942_load_config_from_nvs NVS 校準載入

**檔案**: `main/BL0942Meter.c:35-66`

此函式從 NVS 讀取 JSON 格式的校準系數 (`voltage`, `current`, `power`, `energy`, `frequency`)，
並在值有效（>0）時更新全域 `bl0942_coef` 結構體。首次啟動時若 NVS 無設定，
會自動寫入預設值 (`bl0942_save_config_to_nvs`)。

**保留理由**: 校準系數直接影響所有電氣量的換算精度，是現場校準流程的基礎。

### 4.4 UpdateStableCurrentTracker（AllocationController 中）

**檔案**: `main/AllocationController.c:53-130`

此函式實現充電樁穩定電流偵測邏輯：
- 當實際電流與下發限制電流差值 ≤2A 時，重置追蹤器（充電樁正常響應）
- 差值 >2A 時，開始收集歷史樣本（3 筆）
- 3 筆樣本的最大值與最小值差距 ≤4A 時，判定為穩定（受 BMS 限制）
- 穩定電流值可讓出給其他充電樁使用

**保留理由**: 此為負載均衡核心演算法，已驗證可正確處理 BMS 限流場景。

---

## 5. 邊界條件與故障安全規格

### 5.1 電表斷線偵測（>3 秒過期資料）

#### 偵測機制

```
                    每次成功讀取 5 個暫存器
                    且校驗全部通過時更新
                              │
                              ▼
┌─────────────────────────────────────────┐
│  s_last_update_tick = xTaskGetTickCount()│
└─────────────────────────────────────────┘
                              │
            AllocationController 每次迴圈檢查
                              │
                              ▼
                 ┌────────────────────────┐
                 │ elapsed = now - last   │
                 │ elapsed_ms =           │
                 │   elapsed * portTICK   │
                 │   _PERIOD_MS           │
                 └────────┬───────────────┘
                          │
              ┌───────────┴───────────┐
              │                       │
    elapsed < 3000ms          elapsed >= 3000ms
              │                       │
              ▼                       ▼
      METER_DATA_FRESH       METER_DATA_STALE
      (正常使用資料)          (觸發安全降載)
```

#### 時間參數

| 參數 | 值 | 說明 |
|------|-----|------|
| `METER_STALE_THRESHOLD_MS` | 3000 ms | 資料過期閾值 |
| BL0942 查詢週期 | ~1000 ms | `bl0942_query_task` 中的 `vTaskDelay(1000)` |
| 單次查詢耗時 | ~125 ms | 5 暫存器 × 25ms 幀間隔 |
| 正常更新頻率 | ~1 次/秒 | 查詢成功時每秒更新一次 |

因此，正常情況下資料每秒更新一次。若連續 3 次查詢失敗（約 3 秒），
即觸發 `METER_DATA_STALE` 狀態。

---

> ### ⚠️ 強制規定：Timestamp 來源
>
> **`get_last_update_timestamp()` 的回傳值必須且只能使用 FreeRTOS 系統運行 Tick：**
>
> ```c
> // ✅ 正確 — 使用 FreeRTOS Tick（單調遞增，不受 NTP 影響）
> s_last_update_tick = xTaskGetTickCount();
> // elapsed_ms = (now - s_last_update_tick) * portTICK_PERIOD_MS;
>
> // ✅ 亦可 — 使用 ESP-IDF 微秒計時器（同樣單調遞增）
> s_last_update_us = esp_timer_get_time();
>
> // ❌ 嚴格禁止 — RTC 時間 / UNIX Timestamp / SNTP 同步時間
> // s_last_update = time(NULL);           // 禁止！
> // s_last_update = esp_rtc_time_get();   // 禁止！
> ```
>
> **禁止使用 RTC/UNIX 時間的原因：**
> 設備連上 WiFi 後，SNTP 會自動校正系統時鐘。校正幅度可能達數年（設備初次上網時），
> 導致以下致命誤判：
> - **時間大幅跳進未來**：`elapsed = now - last_update` 計算出的值立即超過 3000ms，
>   電表明明正常運作卻被誤判為「斷線」，觸發不必要的安全降載
> - **時間倒退**：`now < last_update`，`uint32_t` 減法溢位成超大數值，
>   誤判為「資料已過期極長時間」，同樣觸發誤降載
>
> FreeRTOS `xTaskGetTickCount()` 是硬體計時器驅動的單調遞增計數器，
> 不受任何外部時鐘同步影響，是嵌入式系統計算時間差的唯一安全來源。

---

### 5.2 資料有效性檢查

在 AllocationController 使用電表數據前，須執行以下檢查鏈：

```
Step 1: 介面指標檢查
  s_meter == NULL → 使用安全降載值

Step 2: 連線狀態檢查
  s_meter->is_connected() == false → 使用安全降載值

Step 3: 資料新鮮度檢查
  meter_check_freshness(s_meter) == METER_DATA_STALE → 觸發安全降載
  meter_check_freshness(s_meter) == METER_DATA_INVALID → 使用安全降載值

Step 4: 數值範圍檢查（合理性驗證）
  voltage == 0 且 current > 0 → 資料異常，使用安全降載值
  current > 理論最大值（如 >100A）→ 資料異常，使用安全降載值

Step 5: 資料正常 → 正常負載均衡邏輯
```

### 5.3 安全降載回退電流值

當電表資料不可用時，系統需使用安全回退值以防止過載：

| 場景 | 回退策略 | 回退值 |
|------|---------|--------|
| 電表斷線（>3s） | 所有充電樁降至最小電流 | `EV_MIN_CURRENT` (6A) |
| 電表資料從未成功 | 所有充電樁暫停充電 | 0A（觸發 `EVENT_AUTO_SUSPEND`） |
| 資料範圍異常 | 維持上一個有效限制電流，不做上調 | 保持 `limitCurrent` 不變 |
| 斷線後恢復 | 等待至少 2 次連續有效讀數後，才恢復正常分配邏輯 | 漸進恢復 |

#### 斷線回呼觸發流程

```
BL0942MeterImpl                          AllocationController
     │                                          │
     │  bl0942_query_all_registers() 失敗        │
     │  → isConnected = false                    │
     │  → s_last_update_tick 不更新              │
     │                                          │
     │          ... 3 秒後 ...                   │
     │                                          │
     │  freshness check 偵測到 STALE             │
     │  ──── disconnect_callback(3000) ────────▶│
     │                                          │
     │                          觸發安全降載:     │
     │                          1. 所有樁降至 6A  │
     │                          2. 發布監控事件   │
     │                          3. 記錄 WARN 日誌 │
     │                                          │
```

### 5.4 AllocationController 整合偽碼

```c
void AutoControl_task(void *pvParameter)
{
    // 初始化：注入電表介面
    const IMeter_t *meter = get_meter_instance();
    meter->register_disconnect_callback(on_meter_disconnect);

    AddTimer(TIMER_ID_LOADBALANCE, 1000, LoadBalance_TimerFunc);

    while (1) {
        if (LOAD_BALANCE_CAL == 1) {
            LOAD_BALANCE_CAL = 0;

            // === 新增：資料新鮮度門檻檢查 ===
            MeterDataFreshness_t freshness = meter_check_freshness(meter);

            if (freshness == METER_DATA_INVALID) {
                // 從未成功讀取，暫停所有充電樁
                emergency_suspend_all_stations();
                continue;
            }

            if (freshness == METER_DATA_STALE) {
                // 資料過期，降至安全電流
                emergency_reduce_to_min_current();
                continue;
            }

            // === 以下邏輯完全不變 ===
            InflowCurrent = GPIOManager_GetInletCurrent();
            MeterCurrVlaue = (meter->get_current() + 9) / 10;
            // ... 原有 ProcessAllStations 邏輯 ...
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        taskYIELD();
    }
}
```

---

## 6. 實作計畫摘要

### Phase 1: 建立介面（僅新增檔案）

| 步驟 | 動作 | 涉及檔案 |
|------|------|---------|
| 1 | 建立 `main/IMeter.h` | 新增 |
| 2 | 建立 `main/BL0942MeterImpl.c` | 新增 |
| 3 | 在 `bl0942_query_all_registers()` 成功時記錄 `s_last_update_tick` | 修改 `BL0942Meter.c`（僅加一行） |

### Phase 2: AllocationController 切換到介面

| 步驟 | 動作 | 涉及檔案 |
|------|------|---------|
| 4 | 新增 `meter_init()` 函式，注入 `IMeter_t` 指標 | 修改 `AllocationController.c` |
| 5 | 將 `bl0942_get_current()` 替換為 `s_meter->get_current()` | 修改 `AllocationController.c` |
| 6 | 在負載均衡迴圈開頭加入 freshness 檢查 | 修改 `AllocationController.c` |

### Phase 3: 驗證與測試

| 步驟 | 動作 |
|------|------|
| 7 | 建立 `MockMeterImpl` 用於單元測試 |
| 8 | 測試場景：正常、斷線 >3s、資料異常、斷線恢復 |
| 9 | 硬體整合測試：拔除 BL0942 通訊線驗證降載行為 |

### 風險評估

| 風險 | 等級 | 緩解方式 |
|------|------|---------|
| 時間戳溢位（uint32_t tick count） | 低 | FreeRTOS tick 在 32-bit 下約 49.7 天溢位，需處理 wrap-around（使用相減法 `(uint32_t)(now - last)` 可自動處理）|
| 介面指標為 NULL | 低 | 所有存取前加 NULL 檢查 |
| 既有功能迴歸 | 中 | Phase 1 僅新增檔案，不改既有邏輯；Phase 2 以最小差異替換呼叫點 |

---

## 附錄 A: 現有 BL0942 API 對照表

| 現有 API | IMeter 對應方法 | 備註 |
|----------|----------------|------|
| `bl0942_get_current()` | `get_current()` | 回傳值單位相同（raw × 10） |
| `bl0942_get_data() → voltage` | `get_voltage()` | 拆分為獨立方法 |
| `bl0942_get_data() → power` | `get_power()` | 回傳型態升級為 **uint32_t**，單位 1W |
| `bl0942_data.energy` | `get_energy()` | **新增**；回傳型態 **uint32_t**，單位 0.001 kWh |
| `bl0942_is_connected()` | `is_connected()` | 語義相同 |
| _(無對應)_ | `get_last_update_timestamp()` | **新增**：支援資料新鮮度 |
| _(無對應)_ | `register_disconnect_callback()` | **新增**：斷線通知機制 |

## 附錄 B: 相關檔案清單

| 檔案 | 角色 |
|------|------|
| `main/BL0942Meter.h` | BL0942 驅動標頭檔（暫存器定義、資料結構、API 宣告） |
| `main/BL0942Meter.c` | BL0942 驅動實作（UART 通訊、校驗、資料轉換、NVS 校準） |
| `main/AllocationController.h` | 負載均衡控制器標頭檔（常數定義、結構體、API 宣告） |
| `main/AllocationController.c` | 負載均衡控制器實作（電流分配、穩定追蹤、緊急降載） |
