#ifndef __MOCK_METER_IMPL_H__
#define __MOCK_METER_IMPL_H__

#include <stdint.h>
#include <stdbool.h>
#include "IMeter.h"

/** Reset all mock state to zero / disconnected */
void mock_meter_reset(void);

/** Setters - call these from setUp() or individual tests */
void mock_meter_set_voltage(uint16_t v_01V);       /* 0.1V units */
void mock_meter_set_current(uint16_t c_01A);       /* 0.1A units */
void mock_meter_set_power(uint32_t p_1W);           /* 1W units */
void mock_meter_set_energy(uint32_t e_001kWh);      /* 0.001kWh units */
void mock_meter_set_connected(bool connected);
void mock_meter_set_timestamp(uint32_t tick);       /* FreeRTOS tick value */

/** The IMeter_t instance to inject via meter_init() */
extern const IMeter_t g_mock_meter;

#endif /* __MOCK_METER_IMPL_H__ */
