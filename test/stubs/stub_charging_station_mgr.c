#include "ChargingStationManager.h"

/*
 * Injectable stub: emergency_suspend_all_stations() and emergency_reduce_to_min_current()
 * call SelectAllChargeStation() internally. Tests must inject their station arrays here
 * so the emergency functions operate on the same objects the test asserts against.
 */
static ChargingStation *s_stub_stations = NULL;
static int s_stub_station_count = 0;

void stub_csm_set_stations(ChargingStation *stations, int count)
{
    s_stub_stations = stations;
    s_stub_station_count = count;
}

ChargingStation *SelectAllChargeStation(int *count)
{
    *count = s_stub_station_count;
    return s_stub_stations;
}

/* Stub for PrintChargingStationData (called by BL0942Meter.c, not by AllocationController) */
void PrintChargingStationData(void) {}
