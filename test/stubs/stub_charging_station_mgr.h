#ifndef __STUB_CHARGING_STATION_MGR_H__
#define __STUB_CHARGING_STATION_MGR_H__

#include "ChargingStationManager.h"

/**
 * @brief Inject station array for SelectAllChargeStation stub.
 *
 * Must be called before alloc_ctrl_test_run_cycle() so that
 * emergency_suspend_all_stations() / emergency_reduce_to_min_current()
 * (which call SelectAllChargeStation internally) operate on the test stations.
 */
void stub_csm_set_stations(ChargingStation *stations, int count);

#endif /* __STUB_CHARGING_STATION_MGR_H__ */
