#include <string.h>
#include "MockMeterImpl.h"

typedef struct {
    uint16_t voltage;
    uint16_t current;
    uint32_t power;
    uint32_t energy;
    bool     connected;
    uint32_t last_update_tick;
    MeterDisconnectCallback_t disconnect_cb;
} MockMeterState_t;

static MockMeterState_t s_state;

void mock_meter_reset(void)           { memset(&s_state, 0, sizeof(s_state)); }
void mock_meter_set_voltage(uint16_t v)   { s_state.voltage = v; }
void mock_meter_set_current(uint16_t c)   { s_state.current = c; }
void mock_meter_set_power(uint32_t p)     { s_state.power   = p; }
void mock_meter_set_energy(uint32_t e)    { s_state.energy  = e; }
void mock_meter_set_connected(bool c)     { s_state.connected = c; }
void mock_meter_set_timestamp(uint32_t t) { s_state.last_update_tick = t; }

static uint16_t mock_get_voltage(void)  { return s_state.voltage; }
static uint16_t mock_get_current(void)  { return s_state.current; }
static uint32_t mock_get_power(void)    { return s_state.power; }
static uint32_t mock_get_energy(void)   { return s_state.energy; }
static bool     mock_is_connected(void) { return s_state.connected; }
static uint32_t mock_get_timestamp(void){ return s_state.last_update_tick; }
static void     mock_register_cb(MeterDisconnectCallback_t cb) { s_state.disconnect_cb = cb; }

const IMeter_t g_mock_meter = {
    .get_voltage                  = mock_get_voltage,
    .get_current                  = mock_get_current,
    .get_power                    = mock_get_power,
    .get_energy                   = mock_get_energy,
    .is_connected                 = mock_is_connected,
    .get_last_update_timestamp    = mock_get_timestamp,
    .register_disconnect_callback = mock_register_cb,
};
