#define UNIT_TEST  /* enable test hooks in AllocationController.c */
#include "unity.h"
#include "AllocationController.h"
#include "MockMeterImpl.h"
#include "stubs/stub_cevent.h"
#include "stubs/stub_gpio_manager.h"
#include "stubs/stub_charging_station_mgr.h"
#include "stubs/stub_freertos.h"
#include <string.h>

/* -- helpers ------------------------------------------------- */

static ChargingStation make_charging_station(const char *mac, int limit_A)
{
    ChargingStation s;
    memset(&s, 0, sizeof(s));
    strncpy(s.mac, mac, sizeof(s.mac) - 1);
    s.enumStatus    = Charging;
    s.connectStatus = SUB_DEVICE_CONNECT_STATUS_ONLINE;
    s.limitCurrent  = limit_A * 100;    /* unit: 0.01A */
    s.maxlimitCurrent = EV_MAX_CURRENT; /* Amps (compared directly against Amp values) */
    return s;
}

/* -- setUp / tearDown ---------------------------------------- */

void setUp(void)
{
    mock_meter_reset();
    meter_init(&g_mock_meter);
    g_last_published_event = (EVENT_TYPE)-1;
    stub_freertos_set_tick(1000);          /* arbitrary non-zero "now" */
    stub_csm_set_stations(NULL, 0);       /* default: no injected stations */
    mock_meter_set_connected(true);
    mock_meter_set_timestamp(1000);        /* fresh: same as "now" */
}

void tearDown(void) {}

/* -- Test Case 1: Normal DLB balance ------------------------- */

void test_dlb_normal_balance(void)
{
    /*
     * Inflow 60A, meter sees 0A from other loads -> all 60A available for EVs.
     * 2 stations -> each should get ~30A.
     */
    stub_gpio_set_inlet_current(60);
    mock_meter_set_current(0);        /* 0.1A units: 0A from other loads */
    mock_meter_set_timestamp(1000);
    stub_freertos_set_tick(1000);     /* fresh */

    ChargingStation stations[2];
    stations[0] = make_charging_station("AA:BB:CC:DD:EE:01", 16);
    stations[1] = make_charging_station("AA:BB:CC:DD:EE:02", 16);

    /* Set InflowCurrent and MeterCurrVlaue via test hook, then run balance */
    alloc_ctrl_test_set_vars(60, 0);
    ProcessAllStations(stations, 2);

    /* Each station should get ~30A (60A / 2 stations) +/- MIN_ADJUST_THRESHOLD */
    int limit0 = stations[0].limitCurrent / 100;
    int limit1 = stations[1].limitCurrent / 100;
    TEST_ASSERT_INT_WITHIN(MIN_ADJUST_THRESHOLD, 30, limit0);
    TEST_ASSERT_INT_WITHIN(MIN_ADJUST_THRESHOLD, 30, limit1);
    /* Total should not exceed inflow */
    TEST_ASSERT_LESS_OR_EQUAL(60, limit0 + limit1);
}

/* -- Test Case 2: Overload protection ------------------------ */

void test_dlb_overload_protection(void)
{
    /*
     * Inflow 32A (household max), meter sees 50.0A -> overload.
     * totalAvail = 32 - (50 - 0) = -18 -> emergency branch.
     * emergencyCurrent = 32 / 2 = 16A per station.
     */
    stub_gpio_set_inlet_current(32);
    mock_meter_set_current(500);      /* 500 = 50.0A in 0.1A units */
    mock_meter_set_timestamp(1000);
    stub_freertos_set_tick(1000);

    ChargingStation stations[2];
    stations[0] = make_charging_station("AA:BB:CC:DD:EE:01", 16);
    stations[1] = make_charging_station("AA:BB:CC:DD:EE:02", 16);

    alloc_ctrl_test_set_vars(32, 500);
    ProcessAllStations(stations, 2);

    /* Overload branch should publish EVENT_AUTO_SET_LIMIT_CUUR */
    TEST_ASSERT_EQUAL(EVENT_AUTO_SET_LIMIT_CUUR, g_last_published_event);

    /* Each station limit must not exceed inflow / stationCount */
    int limit0 = stations[0].limitCurrent / 100;
    int limit1 = stations[1].limitCurrent / 100;
    TEST_ASSERT_LESS_OR_EQUAL(32, limit0 + limit1);
    /* Limits must be non-negative */
    TEST_ASSERT_GREATER_OR_EQUAL(0, limit0);
    TEST_ASSERT_GREATER_OR_EQUAL(0, limit1);
}

/* -- Test Case 3: Stale meter failsafe ----------------------- */

void test_dlb_meter_stale_failsafe(void)
{
    /*
     * Meter timestamp frozen at tick 1000, system advances to tick 5000.
     * portTICK_PERIOD_MS = 1 in test build -> elapsed = (5000 - 1000) * 1 = 4000ms > 3000ms.
     * Freshness gate triggers METER_DATA_STALE -> emergency_reduce_to_min_current().
     *
     * emergency_reduce_to_min_current() calls SelectAllChargeStation() internally,
     * so we inject our test stations via stub_csm_set_stations().
     */
    mock_meter_set_timestamp(1000);
    stub_freertos_set_tick(5000);     /* 4s > 3s threshold */
    mock_meter_set_current(100);      /* 10.0A - value doesn't matter, data is stale */

    ChargingStation stations[2];
    stations[0] = make_charging_station("AA:BB:CC:DD:EE:01", 16);
    stations[1] = make_charging_station("AA:BB:CC:DD:EE:02", 16);

    /* Inject stations so emergency_reduce_to_min_current can find them */
    stub_csm_set_stations(stations, 2);

    /* Run the full cycle (freshness gate -> should trigger stale path) */
    alloc_ctrl_test_set_vars(32, 100);
    alloc_ctrl_test_run_cycle(stations, 2);

    /* Stale path: emergency_reduce_to_min_current() publishes EVENT_AUTO_SET_LIMIT_CUUR */
    TEST_ASSERT_EQUAL(EVENT_AUTO_SET_LIMIT_CUUR, g_last_published_event);

    /* Each station must be limited to EV_MIN_CURRENT (6A) */
    int limit0 = stations[0].limitCurrent / 100;
    int limit1 = stations[1].limitCurrent / 100;
    TEST_ASSERT_EQUAL(EV_MIN_CURRENT, limit0);
    TEST_ASSERT_EQUAL(EV_MIN_CURRENT, limit1);
}

/* -- app_main (ESP-IDF Unity entry) -------------------------- */

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dlb_normal_balance);
    RUN_TEST(test_dlb_overload_protection);
    RUN_TEST(test_dlb_meter_stale_failsafe);
    UNITY_END();
}
