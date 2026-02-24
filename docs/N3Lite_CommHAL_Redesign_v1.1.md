# N3Lite Communication Platform HAL Redesign v1.1

> **File Version**: 1.1
> **Date**: 2026-02-24
> **Status**: Design Proposal (Revised)
> **Scope**: Abstract communication event publishing behind an `ICommPlatform` interface, decoupling AllocationController from JSON serialisation and the CEvent system

### Version History

| Version | Date | Change |
|---------|------|--------|
| v1.0 | 2026-02-24 | Initial design proposal |
| v1.1 | 2026-02-24 | **Architecture fix**: Added `const` correctness to all `ChargingStation*` interface parameters — enforces at compile-time that implementations must not modify the caller's station data |

---

## 1. Background & Problem Statement

### 1.1 Current Code Problems

`AllocationController.c` currently contains **three distinct communication patterns** that tightly couple the DLB (Dynamic Load Balancing) algorithm to the communication format:

#### Pattern 1: Monitor Status — `snprintf` + JSON + `PublishEvent`

Throughout `ProcessAllStations()`, `ProcessSingleStation()`, and `ProcessTwoStations()`, the controller manually assembles JSON strings and publishes them as monitor events. There are **17 instances** of this pattern across the file:

**Example — `emergency_suspend_all_stations()` (lines 159–170):**
```c
char jsonData[256] = {0};
// ...
snprintf(jsonData, sizeof(jsonData),
         "{\"content\":\"Meter data invalid, emergency suspend\",\"value\":\"0\"}");
PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));
```

**Example — `ProcessAllStations()` overcurrent emergency (lines 402–403):**
```c
snprintf(jsonData, sizeof(jsonData),
         "{\"content\":\"电流严重超标，紧急限流\",\"value\":\"%d\"}", emergencyCurrent);
PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));
```

**Example — `ProcessSingleStation()` set limit (lines 713–717):**
```c
snprintf(jsonData, sizeof(jsonData), "%s%s%s%d%s",
         "{\"content\":\"", "设置限制电流:",
         "\",\"value\":\"", station->limitCurrent,
         "\"}");
PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));
```

#### Pattern 2: Set Limit Current — Struct cast + `PublishEvent`

The controller sets `station->limitCurrent` and then casts the entire `ChargingStation` struct to `char*`:

**Example — `ProcessAllStations()` (line 415):**
```c
stations[idx].limitCurrent = emergencyCurrent * 100;
PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
```

This pattern occurs **15 times** across the file (lines 175, 205, 415, 463, 470, 487, 588, 637, 663, 711, 792, 861, 915, 923, 945).

#### Pattern 3: Start/Suspend Commands — Struct cast + `PublishEvent`

**Start command — `ProcessAllStations()` (line 585):**
```c
PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
```

**Suspend command — `ProcessAllStations()` (line 471):**
```c
PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[idx], sizeof(ChargingStation));
```

`EVENT_AUTO_START` occurs **9 times** (lines 203, 411, 460, 585, 633, 702, 788, 859, 913).
`EVENT_AUTO_SUSPEND` occurs **6 times** (lines 176, 471, 488, 664, 924, 946).

### 1.2 Why This Is a Problem

| Problem | Impact |
|---------|--------|
| **JSON format leaks into DLB logic** | Every call site manually constructs `{"content":"...","value":"..."}` — the DLB algorithm has no business knowing the wire format |
| **`char jsonData[256]` buffers everywhere** | 5 local `char jsonData[256]` declarations across 4 functions; wastes stack in a FreeRTOS task with only 4096 bytes |
| **Unsafe `(char*)&station` casts** | The `ChargingStation` struct is cast to `char*` with `sizeof(ChargingStation)` — relies on the subscriber knowing the exact struct layout; violates type safety |
| **Untestable** | Unit tests for the DLB algorithm must either stub the entire CEvent system or inspect raw `char*` payloads to verify that the correct events were published |
| **Single point of format change** | If the JSON schema changes (e.g., adding a `"timestamp"` field), all 17 `snprintf` call sites must be updated individually |

---

## 2. Design Goals

| # | Goal | Measurable Criterion |
|---|------|---------------------|
| G1 | **Zero JSON in AllocationController** | After refactoring, `AllocationController.c` must contain **zero** `snprintf` calls and **zero** `char jsonData[]` buffer declarations |
| G2 | **Zero raw casts** | After refactoring, `AllocationController.c` must contain **zero** `(char*)&station` casts |
| G3 | **Typed interface** | All ICommPlatform function parameters use explicit C types with physical units documented in Doxygen — no `void*`, no `char* json`, no raw indices |
| G4 | **Testable** | A `MockCommImpl` can be injected to capture all DLB outputs and verify them in unit tests without any real CEvent infrastructure |
| G5 | **Zero DLB logic change** | The DLB algorithm (current calculation, stable current tracking, priority station, dead-zone, hysteresis) must remain **byte-for-byte identical** in logic |
| G6 | **Single-file format ownership** | All JSON assembly and `PublishEvent` calls move to exactly one file: `DefaultCommImpl.c` |

---

## 3. ICommPlatform Interface Specification

### 3.1 ICommPlatform.h — Full Interface Definition

```c
#ifndef __ICOMM_PLATFORM_H__
#define __ICOMM_PLATFORM_H__

#include <stdint.h>
#include "ChargingStation.h"

/**
 * @brief ICommPlatform — Communication Platform Hardware Abstraction Layer
 *
 * Abstracts all outbound communication from the DLB (Dynamic Load Balancing)
 * controller. AllocationController calls these typed functions instead of
 * directly assembling JSON or calling PublishEvent().
 *
 * All implementations (DefaultCommImpl for production, MockCommImpl for tests)
 * must populate every function pointer. NULL pointers are treated as no-ops
 * by the caller.
 *
 * @note This interface is OUTPUT-ONLY. It does not handle inbound messages
 *       or event subscriptions — those remain in the CEvent subscriber model.
 */
typedef struct ICommPlatform {

    /**
     * @brief Report DLB status/decision to the monitoring system
     *
     * Replaces all EVENT_AUTO_CONTROL_MONITOR + snprintf(jsonData, ...) patterns.
     * The implementation is responsible for serialising these parameters into
     * whatever wire format the platform requires (currently JSON).
     *
     * @param content  Human-readable description of the DLB decision
     *                 (e.g., "电流严重超标，紧急限流"). Must not be NULL.
     * @param value_A  Numeric value associated with the decision, typically
     *                 a current in Amperes (e.g., 16 = 16A). Use 0 when
     *                 no numeric value is relevant.
     *
     * @note The implementation MUST NOT modify any ChargingStation state.
     * @note The implementation MUST NOT block for more than 10ms.
     */
    void (*report_dlb_status)(const char *content, int16_t value_A);

    /**
     * @brief Send a current-limit command to a charging station
     *
     * Replaces all EVENT_AUTO_SET_LIMIT_CUUR + (char*)&station patterns.
     * The caller MUST have already set station->limitCurrent before calling.
     *
     * @param station  Pointer to the target ChargingStation. Must not be NULL.
     *                 The implementation reads station->limitCurrent (unit: 0.01A,
     *                 e.g., 3200 = 32.00A) and station->mac for addressing.
     *
     * @note The implementation MUST NOT modify any fields of *station.
     * @note The implementation MUST NOT block for more than 10ms.
     */
    void (*send_limit_current_cmd)(const ChargingStation *station);

    /**
     * @brief Send a start-charging command to a charging station
     *
     * Replaces all EVENT_AUTO_START + (char*)&station patterns.
     * Used when a station in SuspendEvse state needs to resume charging.
     *
     * @param station  Pointer to the target ChargingStation. Must not be NULL.
     *                 The implementation reads station->mac for addressing.
     *
     * @note The implementation MUST NOT modify any fields of *station.
     * @note The implementation MUST NOT block for more than 10ms.
     */
    void (*send_start_cmd)(const ChargingStation *station);

    /**
     * @brief Send a suspend-charging command to a charging station
     *
     * Replaces all EVENT_AUTO_SUSPEND + (char*)&station patterns.
     * Used when available current is insufficient and a station must stop.
     *
     * @param station  Pointer to the target ChargingStation. Must not be NULL.
     *                 The implementation reads station->mac for addressing.
     *
     * @note The implementation MUST NOT modify any fields of *station.
     * @note The implementation MUST NOT block for more than 10ms.
     */
    void (*send_suspend_cmd)(const ChargingStation *station);

} ICommPlatform_t;

#endif /* __ICOMM_PLATFORM_H__ */
```

### 3.2 Function Mapping to Current EVENT_TYPE Values

| ICommPlatform Function | Replaces EVENT_TYPE | Call Count in AllocationController.c |
|------------------------|--------------------|------------------------------------|
| `report_dlb_status(content, value_A)` | `EVENT_AUTO_CONTROL_MONITOR` | 17 call sites |
| `send_limit_current_cmd(station)` | `EVENT_AUTO_SET_LIMIT_CUUR` | 15 call sites |
| `send_start_cmd(station)` | `EVENT_AUTO_START` | 9 call sites |
| `send_suspend_cmd(station)` | `EVENT_AUTO_SUSPEND` | 6 call sites |

### 3.3 What Each Function Must NOT Do

| Constraint | Rationale |
|------------|-----------|
| Must NOT modify `ChargingStation` fields | The DLB algorithm owns the station state; the comm layer is a passive output channel |
| Must NOT allocate heap memory | Runs in a FreeRTOS task with 4096-byte stack; use stack buffers only |
| Must NOT block >10ms | The DLB loop runs every 1.3s; blocking would delay load-balancing decisions |
| Must NOT call back into AllocationController | Prevents re-entrant loops between DLB and comm layers |

### 3.4 `const` Correctness — Why and How (v1.1 Architecture Fix)

**Why `const ChargingStation *station` instead of `ChargingStation *station`:**

The Doxygen comments in v1.0 stated "The implementation MUST NOT modify any fields of `*station`", but this was only a documentation contract — the C compiler could not enforce it. An implementation could silently write to `station->limitCurrent` or any other field, and the error would only appear at runtime.

In v1.1, this constraint is promoted to a **compile-time guarantee**:

```c
// v1.0 — documentation-only contract (compiler cannot enforce)
void (*send_limit_current_cmd)(ChargingStation *station);

// v1.1 — const enforced at compile time
void (*send_limit_current_cmd)(const ChargingStation *station);
```

**Effect on callers (`AllocationController.c`):**

No change required. In C, a non-const pointer (`ChargingStation *`) is implicitly convertible to a const pointer (`const ChargingStation *`). Passing `&stations[i]` (which is `ChargingStation *`) to a `const ChargingStation *` parameter is always valid and requires no cast.

**Effect on implementations (`DefaultCommImpl.c`, `MockCommImpl.c`):**

All implementation functions must declare the parameter as `const ChargingStation *station`. Any attempt to write through the pointer will produce a compiler error:

```c
// DefaultCommImpl.c — reads station data, must not write
static void default_send_limit_current_cmd(const ChargingStation *station) {
    // OK: reading fields
    uint8_t *mac = station->mac;
    int limit    = station->limitCurrent;

    // ERROR: compiler rejects this
    // station->limitCurrent = 0;  // cannot assign to const
}
```

**Effect on `MockCommImpl.c`:**

The mock records station data by copying values out of the struct (MAC address, limitCurrent), not by storing the pointer. This approach is already correct and compatible with `const ChargingStation *`.

---

## 4. DefaultCommImpl.c Overview

### 4.1 Purpose

`DefaultCommImpl.c` is the production implementation of `ICommPlatform`. It moves **all** JSON assembly and `PublishEvent()` calls out of `AllocationController.c` and into a single, dedicated file.

### 4.2 Implementation Sketch

```c
// DefaultCommImpl.c
#include "ICommPlatform.h"
#include "CEvent.h"
#include <string.h>
#include <stdio.h>

static void default_report_dlb_status(const char *content, int16_t value_A)
{
    char jsonData[256] = {0};
    snprintf(jsonData, sizeof(jsonData),
             "{\"content\":\"%s\",\"value\":\"%d\"}", content, value_A);
    PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));
}

static void default_send_limit_current_cmd(const ChargingStation *station)
{
    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR,
                 (char *)station, sizeof(ChargingStation));
}

static void default_send_start_cmd(const ChargingStation *station)
{
    PublishEvent(EVENT_AUTO_START,
                 (char *)station, sizeof(ChargingStation));
}

static void default_send_suspend_cmd(const ChargingStation *station)
{
    PublishEvent(EVENT_AUTO_SUSPEND,
                 (char *)station, sizeof(ChargingStation));
}

const ICommPlatform_t g_default_comm = {
    .report_dlb_status      = default_report_dlb_status,
    .send_limit_current_cmd = default_send_limit_current_cmd,
    .send_start_cmd         = default_send_start_cmd,
    .send_suspend_cmd       = default_send_suspend_cmd,
};
```

### 4.3 Before / After Comparison

#### `emergency_suspend_all_stations()` — Lines 157–179

**BEFORE (current code):**
```c
static void emergency_suspend_all_stations(void)
{
    char jsonData[256] = {0};
    int stationCount = 0;
    ChargingStation *stations = SelectAllChargeStation(&stationCount);

    if (stations == NULL || stationCount <= 0) {
        return;
    }

    dPrint(WARN, "Meter data INVALID - emergency suspend all stations\n");
    snprintf(jsonData, sizeof(jsonData),
             "{\"content\":\"Meter data invalid, emergency suspend\",\"value\":\"0\"}");
    PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

    for (int i = 0; i < stationCount; i++) {
        if (IsStationCharging(&stations[i])) {
            stations[i].limitCurrent = 0;
            PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char *)&stations[i], sizeof(ChargingStation));
            PublishEvent(EVENT_AUTO_SUSPEND, (char *)&stations[i], sizeof(ChargingStation));
        }
    }
}
```

**AFTER (refactored):**
```c
static void emergency_suspend_all_stations(void)
{
    int stationCount = 0;
    ChargingStation *stations = SelectAllChargeStation(&stationCount);

    if (stations == NULL || stationCount <= 0) {
        return;
    }

    dPrint(WARN, "Meter data INVALID - emergency suspend all stations\n");
    s_comm->report_dlb_status("Meter data invalid, emergency suspend", 0);

    for (int i = 0; i < stationCount; i++) {
        if (IsStationCharging(&stations[i])) {
            stations[i].limitCurrent = 0;
            s_comm->send_limit_current_cmd(&stations[i]);
            s_comm->send_suspend_cmd(&stations[i]);
        }
    }
}
```

**Changes:**
- Removed `char jsonData[256]` declaration
- Removed `snprintf` + `PublishEvent(EVENT_AUTO_CONTROL_MONITOR, ...)` replaced with `s_comm->report_dlb_status(...)`
- Removed `(char*)&stations[i]` casts replaced with typed `s_comm->send_*` calls

#### `ProcessAllStations()` overcurrent branch — Lines 397–418

**BEFORE:**
```c
snprintf(jsonData, sizeof(jsonData), "{\"content\":\"电流严重超标，紧急限流\",\"value\":\"%d\"}", emergencyCurrent);
PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

for (int i = 0; i < chargingCount; i++)
{
    int idx = chargingIndexes[i];
    if (stations[idx].enumStatus == SuspendEvse)
    {
        PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
    }
    stations[idx].limitCurrent = emergencyCurrent * 100;
    PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));
}
```

**AFTER:**
```c
s_comm->report_dlb_status("电流严重超标，紧急限流", emergencyCurrent);

for (int i = 0; i < chargingCount; i++)
{
    int idx = chargingIndexes[i];
    if (stations[idx].enumStatus == SuspendEvse)
    {
        s_comm->send_start_cmd(&stations[idx]);
    }
    stations[idx].limitCurrent = emergencyCurrent * 100;
    s_comm->send_limit_current_cmd(&stations[idx]);
}
```

---

## 5. AllocationController.c After Refactoring

### 5.1 New Static Variable

```c
// Comm platform interface pointer (injected via comm_init)
static const ICommPlatform_t *s_comm = NULL;
```

### 5.2 Call Site Transformations

Every call site follows one of three mechanical transformations:

#### Transform A: Monitor Status (17 sites)

```c
// BEFORE:
snprintf(jsonData, sizeof(jsonData),
         "{\"content\":\"电流严重超标，紧急限流\",\"value\":\"%d\"}", emergencyCurrent);
PublishEvent(EVENT_AUTO_CONTROL_MONITOR, jsonData, strlen(jsonData));

// AFTER:
s_comm->report_dlb_status("电流严重超标，紧急限流", emergencyCurrent);
```

#### Transform B: Set Limit Current (15 sites)

```c
// BEFORE:
PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR, (char*)&stations[idx], sizeof(ChargingStation));

// AFTER:
s_comm->send_limit_current_cmd(&stations[idx]);
```

#### Transform C: Start / Suspend (15 sites total)

```c
// BEFORE:
PublishEvent(EVENT_AUTO_START, (char*)&stations[idx], sizeof(ChargingStation));
PublishEvent(EVENT_AUTO_SUSPEND, (char*)&stations[idx], sizeof(ChargingStation));

// AFTER:
s_comm->send_start_cmd(&stations[idx]);
s_comm->send_suspend_cmd(&stations[idx]);
```

### 5.3 Eliminated Declarations

After refactoring, these declarations are **removed** from AllocationController.c:

| Function | Removed Declaration | Line (current) |
|----------|-------------------|----------------|
| `emergency_suspend_all_stations()` | `char jsonData[256] = {0};` | 159 |
| `emergency_reduce_to_min_current()` | `char jsonData[256] = {0};` | 186 |
| `ProcessAllStations()` | `char jsonData[256] = {0};` | 282 |
| `ProcessSingleStation()` | `char jsonData[256] = {0};` | 604 |
| `ProcessTwoStations()` | `char jsonData[256] = {0};` | 734 |

The `#include "CEvent.h"` can also be removed from `AllocationController.c` (it remains in `DefaultCommImpl.c`).

### 5.4 Net Effect on AllocationController.c

| Metric | Before | After |
|--------|--------|-------|
| `snprintf` calls | 17 | **0** |
| `char jsonData[256]` buffers | 5 | **0** |
| `PublishEvent` calls | 47 | **0** |
| `(char*)&station` casts | 30 | **0** |
| `#include "CEvent.h"` | yes | **no** |
| DLB algorithm logic | unchanged | **unchanged** |

---

## 6. Dependency Injection

### 6.1 Injection Function

Following the same pattern as `meter_init()` in the Meter HAL refactor:

```c
// AllocationController.h — new declaration
void comm_init(const ICommPlatform_t *comm);

// AllocationController.c — implementation
static const ICommPlatform_t *s_comm = NULL;

void comm_init(const ICommPlatform_t *comm)
{
    s_comm = comm;
}
```

### 6.2 Initialisation Point

In the application startup code (typically `app_main()` or the initialisation sequence that already calls `meter_init()`):

```c
#include "AllocationController.h"

// Production setup:
extern const ICommPlatform_t g_default_comm;   // from DefaultCommImpl.c
extern const IMeter_t        g_bl0942_meter;   // from BL0942MeterImpl.c

void app_main(void)
{
    // ... other init ...

    meter_init(&g_bl0942_meter);       // existing Meter HAL injection
    comm_init(&g_default_comm);        // new Comm HAL injection

    AutoControlInit();
}
```

### 6.3 NULL Safety

All call sites in AllocationController.c should use a guard pattern:

```c
if (s_comm != NULL && s_comm->report_dlb_status != NULL) {
    s_comm->report_dlb_status("...", value);
}
```

Alternatively, a helper macro can reduce boilerplate:

```c
#define COMM_CALL(fn, ...) \
    do { if (s_comm != NULL && s_comm->fn != NULL) s_comm->fn(__VA_ARGS__); } while(0)

// Usage:
COMM_CALL(report_dlb_status, "电流严重超标，紧急限流", emergencyCurrent);
COMM_CALL(send_limit_current_cmd, &stations[idx]);
```

---

## 7. Unit Test Strategy

### 7.1 MockCommImpl Design

```c
// test/MockCommImpl.h
#ifndef __MOCK_COMM_IMPL_H__
#define __MOCK_COMM_IMPL_H__

#include "ICommPlatform.h"

#define MOCK_COMM_MAX_CALLS 32

typedef enum {
    MOCK_CALL_REPORT_STATUS,
    MOCK_CALL_LIMIT_CURRENT,
    MOCK_CALL_START,
    MOCK_CALL_SUSPEND
} MockCommCallType;

typedef struct {
    MockCommCallType type;
    union {
        struct {
            const char *content;
            int16_t value_A;
        } report;
        struct {
            char mac[20];
            int  limitCurrent;      /**< unit: 0.01A */
        } station_cmd;
    };
} MockCommCall;

/** Recorded call history */
extern MockCommCall g_mock_comm_calls[];
extern int          g_mock_comm_call_count;

/** The mock instance — inject via comm_init(&g_mock_comm) */
extern const ICommPlatform_t g_mock_comm;

/** Reset call history before each test */
void mock_comm_reset(void);

#endif /* __MOCK_COMM_IMPL_H__ */
```

### 7.2 Test Example

```c
// test_allocation_comm.c (Unity framework)

void test_emergency_overcurrent_publishes_limit_and_monitor(void)
{
    // Arrange
    mock_comm_reset();
    mock_meter_reset();
    comm_init(&g_mock_comm);
    meter_init(&g_mock_meter);

    ChargingStation stations[2] = { /* set up two charging stations */ };
    stations[0].enumStatus = Charging;
    stations[0].acCurrentL1 = 500;  // 50.0A — exceeds InflowCurrent
    stations[1].enumStatus = Charging;
    stations[1].acCurrentL1 = 400;  // 40.0A

    alloc_ctrl_test_set_vars(/*inflow=*/40, /*meter_curr_01A=*/900);

    // Act
    ProcessAllStations(stations, 2);

    // Assert — verify DLB published correct commands
    TEST_ASSERT_GREATER_THAN(0, g_mock_comm_call_count);

    // First call should be a monitor report about overcurrent
    TEST_ASSERT_EQUAL(MOCK_CALL_REPORT_STATUS, g_mock_comm_calls[0].type);

    // Both stations should receive limit current commands
    int limit_count = 0;
    for (int i = 0; i < g_mock_comm_call_count; i++) {
        if (g_mock_comm_calls[i].type == MOCK_CALL_LIMIT_CURRENT) {
            limit_count++;
        }
    }
    TEST_ASSERT_EQUAL(2, limit_count);
}
```

### 7.3 What MockCommImpl Enables

| Test Scenario | What to Assert |
|--------------|---------------|
| Normal dual-station allocation | `report_dlb_status` called with correct `value_A` = allocated current |
| Emergency overcurrent | Both stations receive `send_limit_current_cmd` with `limitCurrent = (InflowCurrent/2)*100` |
| Insufficient current — priority station | Selected station gets `send_start_cmd` + `send_limit_current_cmd`; other gets `send_suspend_cmd` |
| Meter data stale | All stations get `send_limit_current_cmd` with `EV_MIN_CURRENT*100` |
| Meter data invalid | All stations get `send_limit_current_cmd(0)` + `send_suspend_cmd` |
| Dead-zone — no adjustment needed | Zero `send_limit_current_cmd` calls when diff < `MIN_ADJUST_THRESHOLD` |

---

## 8. Files to Create / Modify

| File | Action | Description |
|------|--------|-------------|
| `main/ICommPlatform.h` | **Create** | Interface definition (struct of function pointers) |
| `main/DefaultCommImpl.c` | **Create** | Production implementation: JSON assembly + PublishEvent calls |
| `main/AllocationController.c` | **Modify** | Replace all `snprintf`/`PublishEvent` calls with `s_comm->*` calls; add `comm_init()`; remove `#include "CEvent.h"` |
| `main/AllocationController.h` | **Modify** | Add `comm_init()` declaration; add `#include "ICommPlatform.h"` |
| `main/CMakeLists.txt` | **Modify** | Add `DefaultCommImpl.c` to source list |
| `test/MockCommImpl.h` | **Create** | Mock implementation header (call recording structs) |
| `test/MockCommImpl.c` | **Create** | Mock implementation (records calls for assertion) |
| `test/test_allocation_comm.c` | **Create** | Unit tests for DLB comm integration |
| App startup file (e.g., `main/main.c`) | **Modify** | Add `comm_init(&g_default_comm)` alongside existing `meter_init()` |

### Files NOT Modified

| File | Reason |
|------|--------|
| `main/CEvent.h` | The CEvent pub/sub system is unchanged; DefaultCommImpl uses it |
| `main/CEvent.c` | No changes to event dispatch logic |
| `main/IMeter.h` | Meter HAL is a separate concern |
| `main/BL0942Meter.c` | BL0942 driver is unrelated |
| `main/BL0942MeterImpl.c` | Meter implementation is unrelated |
| `main/ChargingStation.h` | Struct definition is unchanged |
| `main/ChargingStationManager.c` | Station management is unchanged |
| `main/GPIOManager.c` | GPIO/inlet current reading is unchanged |

---

## 9. What This Refactor Does NOT Change

The following are **explicitly out of scope**:

| Item | Reason |
|------|--------|
| **DLB algorithm logic** | All current calculations, stable current tracking, priority station selection, dead-zone thresholds, and hysteresis remain identical |
| **BL0942 driver** | `BL0942Meter.c` / `BL0942Meter.h` are untouched; the meter HAL is a separate abstraction |
| **PLC protocol** | The PLC frame encoding/decoding in `ChargingStationManager.c` is not part of this refactor |
| **CEvent system** | `CEvent.h` / `CEvent.c` (pub/sub infrastructure) remains unchanged — `DefaultCommImpl.c` still uses `PublishEvent()` internally |
| **Inbound event handling** | `SubscribeEvent()` call sites and event handlers in other modules are not affected |
| **`ProcessSingleStation` / `ProcessTwoStations` removal** | These deprecated functions are refactored to use `s_comm->*` but are not deleted (they may still be called by legacy paths) |
| **Wire format** | The JSON schema `{"content":"...","value":"..."}` is preserved exactly as-is inside `DefaultCommImpl.c` |
| **Station struct layout** | `ChargingStation` fields (`limitCurrent`, `mac`, `enumStatus`, etc.) are unchanged |

---

## Appendix A: Complete PublishEvent Call-Site Catalogue

### A.1 EVENT_AUTO_CONTROL_MONITOR (17 sites)

| # | Function | Line | Content Pattern | Value |
|---|----------|------|----------------|-------|
| 1 | `emergency_suspend_all_stations` | 168–170 | `"Meter data invalid, emergency suspend"` | `0` |
| 2 | `emergency_reduce_to_min_current` | 195–197 | `"Meter data stale, limit to min current"` | `EV_MIN_CURRENT` |
| 3 | `ProcessAllStations` | 402–403 | `"电流严重超标，紧急限流"` | `emergencyCurrent` |
| 4 | `ProcessAllStations` | 444–445 | `"总电流不足，优先先充电的桩"` | `totalAvail` |
| 5 | `ProcessAllStations` | 479–480 | `"总电流不足，所有桩暂停"` | `totalAvail` |
| 6 | `ProcessAllStations` | 591–593 | `"完成%d个桩的负载均衡分配"` | `baseAllocation` |
| 7 | `ProcessSingleStation` | 657–661 | `"可用电流不足,设置为0A"` | `0` |
| 8 | `ProcessSingleStation` | 671–675 | `"可限制电流不超过充电桩最大限制电流Avail"` | `Avail` |
| 9 | `ProcessSingleStation` | 704–708 | `"充电桩处于暂停状态，先发布启动事件"` | `0` |
| 10 | `ProcessSingleStation` | 713–717 | `"设置限制电流:"` | `station->limitCurrent` |
| 11 | `ProcessTwoStations` | 744–748 | `"两个充电桩都在充电，平均分配电流"` | `0` |
| 12 | `ProcessTwoStations` | 818–822 | `"两个充电桩平均分配电流"` | `avgCurrent` |
| 13 | `ProcessTwoStations` | 872–876 | `"平均电流不足，选择充电桩%d充电"` | `totalAvail` |
| 14 | `ProcessTwoStations` | 933–937 | `"总电流不足，两个充电桩都暂停"` | `totalAvail` |
| 15 | `ProcessTwoStations` | 956–960 | `"只有充电桩1在充电，按单桩处理:"` | `0` |
| 16 | `ProcessTwoStations` | 968–972 | `"只有充电桩2在充电，按单桩处理:"` | `0` |
| 17 | `ProcessTwoStations` | 978–982 | `"两个充电桩都未在充电，无需负载均衡:"` | `0` |

### A.2 EVENT_AUTO_SET_LIMIT_CUUR (15 sites)

Lines: 175, 205, 415, 463, 470, 487, 588, 637, 663, 711, 792, 861, 915, 923, 945

### A.3 EVENT_AUTO_START (9 sites)

Lines: 203, 411, 460, 585, 633, 702, 788, 859, 913

### A.4 EVENT_AUTO_SUSPEND (6 sites)

Lines: 176, 471, 488, 664, 924, 946

---

## Appendix B: Data Flow Diagram

### B.1 Before: AllocationController Directly Assembles JSON

```
┌──────────────────────────────────────────────┐
│         AllocationController.c               │
│                                              │
│  char jsonData[256];                         │
│  snprintf(jsonData, ...,                     │
│    "{\"content\":\"...\",\"value\":\"...\"}");│
│  PublishEvent(EVENT_AUTO_CONTROL_MONITOR,    │── CEvent ──▶ Subscribers
│              jsonData, strlen(jsonData));     │
│                                              │
│  PublishEvent(EVENT_AUTO_SET_LIMIT_CUUR,     │
│              (char*)&station,                │── CEvent ──▶ Subscribers
│              sizeof(ChargingStation));        │
│                                              │
│  PublishEvent(EVENT_AUTO_START,              │
│              (char*)&station,                │── CEvent ──▶ Subscribers
│              sizeof(ChargingStation));        │
└──────────────────────────────────────────────┘

Problems:
  ✗ JSON format knowledge embedded in DLB algorithm
  ✗ 17 snprintf call sites to maintain
  ✗ Unsafe (char*) casts on every command
  ✗ Cannot unit-test DLB outputs without CEvent stubs
```

### B.2 After: AllocationController Uses ICommPlatform

```
┌──────────────────────────┐
│  AllocationController.c  │
│                          │
│  s_comm->report_dlb_     │     ┌───────────────────┐
│    status("...", val);   │────▶│  ICommPlatform.h   │
│                          │     │  (interface only)  │
│  s_comm->send_limit_    │     │                    │
│    current_cmd(station); │────▶│ .report_dlb_status │
│                          │     │ .send_limit_current│
│  s_comm->send_start_    │     │ .send_start_cmd    │
│    cmd(station);         │────▶│ .send_suspend_cmd  │
│                          │     └────────┬───────────┘
│  s_comm->send_suspend_  │              │
│    cmd(station);         │     ┌────────┴──────────────┐
└──────────────────────────┘     │                       │
                                 ▼                       ▼
                     ┌───────────────────┐   ┌───────────────────┐
                     │ DefaultCommImpl.c │   │  MockCommImpl.c   │
                     │ (production)      │   │  (unit tests)     │
                     │                   │   │                   │
                     │ snprintf(json...) │   │ records calls in  │
                     │ PublishEvent(...) │   │ g_mock_comm_calls │
                     └───────┬───────────┘   └───────────────────┘
                             │
                             ▼
                     ┌───────────────────┐
                     │  CEvent system    │
                     │  PublishEvent()   │──▶ Subscribers
                     └───────────────────┘

Advantages:
  ✓ AllocationController has zero JSON / CEvent knowledge
  ✓ Format changes isolated to DefaultCommImpl.c
  ✓ Type-safe station pointers (no char* casts)
  ✓ MockCommImpl enables full DLB unit testing
```

---

## Appendix C: Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| `s_comm` is NULL at runtime | Low | NULL-check guard on every call site (or `COMM_CALL` macro); same pattern already proven with `s_meter` |
| `report_dlb_status` content string contains `"` or `%` | Low | `DefaultCommImpl` should use `%s` format safely; content strings are compile-time constants in current code |
| Stack size impact of removing `jsonData[256]` | Positive | Removing 256-byte stack buffers from AllocationController **frees** stack space in the 4096-byte task |
| Regression in event delivery order | Low | `DefaultCommImpl` calls `PublishEvent` in the same order as current code; CEvent dispatch is synchronous |
| Breaking existing CEvent subscribers | None | Subscribers receive identical `EVENT_TYPE` + payload; the refactor only changes the **producer**, not the event format |
