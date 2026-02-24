/**
 * @file    DefaultCommImpl.c
 * @brief   Production implementation of the ICommPlatform interface
 * @details Encapsulates all JSON assembly and PublishEvent() calls that were
 *          previously scattered across AllocationController.c.
 *          This is the single file that owns the wire format.
 */

#include "ICommPlatform.h"
#include "CEvent.h"
#include "ChargingStation.h"
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
