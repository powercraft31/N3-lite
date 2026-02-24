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
