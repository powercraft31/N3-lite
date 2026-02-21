#include "GPIOManager.h"

static uint8_t s_inlet_current = 32;

void stub_gpio_set_inlet_current(uint8_t a) { s_inlet_current = a; }

uint8_t GPIOManager_GetInletCurrent(void)   { return s_inlet_current; }
