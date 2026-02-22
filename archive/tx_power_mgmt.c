/* VUURWERK v1.0.4 - TX Power Management Implementation */

#include "tx_power_mgmt.h"
#include "driver/bk4819.h"

TX_PowerMgmt_t gTX_PowerMgmt;

void TX_POWER_MGMT_Init(void) {
    gTX_PowerMgmt.battery_mv = 4000;
    gTX_PowerMgmt.requested_power = 0;
    gTX_PowerMgmt.actual_power = 0;
    gTX_PowerMgmt.limited = false;
}

void TX_POWER_MGMT_Update(uint16_t battery_mv) {
    gTX_PowerMgmt.battery_mv = battery_mv;
}

uint8_t TX_POWER_MGMT_LimitPower(uint8_t requested_power) {
    gTX_PowerMgmt.requested_power = requested_power;

    uint8_t max_power = requested_power;

    // Step down power based on battery voltage
    if (gTX_PowerMgmt.battery_mv < BATT_THRESH_CRITICAL) {
        // Critical: LOW only
        max_power = 0; // OUTPUT_POWER_LOW
    } else if (gTX_PowerMgmt.battery_mv < BATT_THRESH_LOW) {
        // Low: MID maximum
        if (max_power > 1) max_power = 1; // OUTPUT_POWER_MID
    } else if (gTX_PowerMgmt.battery_mv < BATT_THRESH_MED) {
        // Medium: No HIGH allowed
        if (max_power > 1) max_power = 1; // OUTPUT_POWER_MID
    }
    // Above MED threshold: no limitation

    gTX_PowerMgmt.actual_power = max_power;
    gTX_PowerMgmt.limited = (max_power < requested_power);

    return max_power;
}

bool TX_POWER_MGMT_IsLimited(void) {
    return gTX_PowerMgmt.limited;
}
