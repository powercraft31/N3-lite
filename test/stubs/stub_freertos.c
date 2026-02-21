/*
 * FreeRTOS stubs for host / unit-test compilation.
 *
 * In an ESP-IDF target build these headers come from the FreeRTOS component;
 * for host-only test builds provide minimal fakes via test/fakes/ include path.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint32_t s_mock_tick = 0;

void stub_freertos_set_tick(uint32_t tick) { s_mock_tick = tick; }

TickType_t xTaskGetTickCount(void) { return (TickType_t)s_mock_tick; }

/* AutoControlInit calls xTaskCreate - stub satisfies linker */
BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *pcName,
                       configSTACK_DEPTH_TYPE usStackDepth, void *pvParameters,
                       UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask)
{
    (void)pxTaskCode; (void)pcName; (void)usStackDepth;
    (void)pvParameters; (void)uxPriority; (void)pxCreatedTask;
    return 1; /* pdPASS */
}

/* AutoControl_task calls vTaskDelay - stub satisfies linker */
void vTaskDelay(TickType_t xTicksToDelay)
{
    (void)xTicksToDelay;
}
