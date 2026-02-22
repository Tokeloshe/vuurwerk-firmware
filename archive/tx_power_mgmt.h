/* VUURWERK v1.0.4 - TX Power Management
 * Auto-step-down on low battery to prevent brownout
 */

#ifndef TX_POWER_MGMT_H
#define TX_POWER_MGMT_H

#include <stdint.h>
#include <stdbool.h>

// Battery thresholds in mV
#define BATT_THRESH_HIGH    3800
#define BATT_THRESH_MED     3600
#define BATT_THRESH_LOW     3400
#define BATT_THRESH_CRITICAL 3200

typedef struct {
    uint16_t battery_mv;
    uint8_t requested_power;
    uint8_t actual_power;
    bool limited;
} TX_PowerMgmt_t;

extern TX_PowerMgmt_t gTX_PowerMgmt;

void TX_POWER_MGMT_Init(void);
void TX_POWER_MGMT_Update(uint16_t battery_mv);
uint8_t TX_POWER_MGMT_LimitPower(uint8_t requested_power);
bool TX_POWER_MGMT_IsLimited(void);

#endif
