/**
 * @file sim_control_overrides.h
 * @brief PC-simulation-only scaling overrides.
 */

#ifndef _FILE_GFL_SIM_CONTROL_OVERRIDES_H_
#define _FILE_GFL_SIM_CONTROL_OVERRIDES_H_

// 28 Vrms line voltage -> 22.86 V phase peak -> 0.720 pu at a 55 V DC base.
#undef CTRL_DCBUS_VOLTAGE
#define CTRL_DCBUS_VOLTAGE (55.0f)

#undef GFL_GRID_VOLTAGE_PU
#define GFL_GRID_VOLTAGE_PU (0.7200f)

#undef GFL_DCBUS_VOLTAGE_REF_V
#define GFL_DCBUS_VOLTAGE_REF_V (55.0f)

#endif // _FILE_GFL_SIM_CONTROL_OVERRIDES_H_
