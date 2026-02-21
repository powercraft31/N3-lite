/**
 * @file    BL0942MeterImpl.c
 * @brief   BL0942 implementation of the IMeter interface
 * @details Wraps existing BL0942Meter.c APIs behind the IMeter HAL.
 *          Exports g_bl0942_meter as the concrete meter instance.
 */

#include "IMeter.h"
#include "BL0942Meter.h"

static MeterDisconnectCallback_t s_disconnect_cb = NULL;

static uint16_t bl0942_impl_get_voltage(void)
{
    BL0942_Data_t data;
    if (bl0942_get_data(&data)) {
        return data.voltage;    /* already in 0.1V */
    }
    return 0;
}

static uint16_t bl0942_impl_get_current(void)
{
    return bl0942_get_current();    /* already in 0.1A */
}

static uint32_t bl0942_impl_get_power(void)
{
    BL0942_Data_t data;
    if (bl0942_get_data(&data)) {
        /* BL0942 stores power in 1W units (uint16_t); IMeter spec requires 1W (uint32_t), direct cast */
        return (uint32_t)data.power;
    }
    return 0;
}

static uint32_t bl0942_impl_get_energy(void)
{
    BL0942_Data_t data;
    if (bl0942_get_data(&data)) {
        /* BL0942 stores energy in Wh (uint32_t), 1 Wh = 1 unit of 0.001kWh */
        return data.energy;
    }
    return 0;
}

static bool bl0942_impl_is_connected(void)
{
    return bl0942_is_connected();
}

static uint32_t bl0942_impl_get_last_update_timestamp(void)
{
    return bl0942_get_last_update_tick();
}

static void bl0942_impl_register_disconnect_cb(MeterDisconnectCallback_t cb)
{
    s_disconnect_cb = cb;
}

/* Exported IMeter instance for BL0942 */
const IMeter_t g_bl0942_meter = {
    .get_voltage                  = bl0942_impl_get_voltage,
    .get_current                  = bl0942_impl_get_current,
    .get_power                    = bl0942_impl_get_power,
    .get_energy                   = bl0942_impl_get_energy,
    .is_connected                 = bl0942_impl_is_connected,
    .get_last_update_timestamp    = bl0942_impl_get_last_update_timestamp,
    .register_disconnect_callback = bl0942_impl_register_disconnect_cb,
};
