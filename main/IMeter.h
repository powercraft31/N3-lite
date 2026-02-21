#ifndef __IMETER_H__
#define __IMETER_H__

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Meter data freshness status
 */
typedef enum {
    METER_DATA_FRESH,       // Data within valid time window (<3s)
    METER_DATA_STALE,       // Data expired (>=3s), should trigger safety fallback
    METER_DATA_INVALID      // Never successfully read data
} MeterDataFreshness_t;

/**
 * @brief Meter disconnect callback type
 * @param stale_duration_ms Duration of data staleness in milliseconds
 */
typedef void (*MeterDisconnectCallback_t)(uint32_t stale_duration_ms);

/**
 * @brief IMeter interface - Meter Hardware Abstraction Layer
 *
 * All meter implementations (BL0942, simulator, future models) must implement
 * this interface. AllocationController accesses meter data exclusively through
 * this interface.
 */
typedef struct IMeter {
    /**
     * @brief Get current voltage RMS
     * @return Voltage in 0.1V (e.g., 2200 = 220.0V)
     */
    uint16_t (*get_voltage)(void);

    /**
     * @brief Get current RMS
     * @return Current in 0.1A (e.g., 321 = 32.1A)
     */
    uint16_t (*get_current)(void);

    /**
     * @brief Get active power
     * @return Power in 1W (e.g., 3500 = 3500W)
     */
    uint32_t (*get_power)(void);

    /**
     * @brief Get accumulated energy
     * @return Energy in 0.001kWh (e.g., 1500 = 1.500kWh)
     */
    uint32_t (*get_energy)(void);

    /**
     * @brief Get meter connection status
     * @return true=connected, false=disconnected or communication error
     */
    bool (*is_connected)(void);

    /**
     * @brief Get last successful data update timestamp
     * @return FreeRTOS xTaskGetTickCount() value. Returns 0 if never updated.
     *
     * MUST use FreeRTOS tick (monotonic, not affected by NTP/SNTP).
     * NEVER use RTC / UNIX timestamp / SNTP-synced time.
     */
    uint32_t (*get_last_update_timestamp)(void);

    /**
     * @brief Register meter disconnect callback
     * @param cb Disconnect callback function. Pass NULL to unregister.
     */
    void (*register_disconnect_callback)(MeterDisconnectCallback_t cb);

} IMeter_t;

/** Meter data staleness threshold (milliseconds) */
#define METER_STALE_THRESHOLD_MS  3000

/**
 * @brief Check meter data freshness
 * @param meter Meter interface pointer
 * @return MeterDataFreshness_t data status
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
