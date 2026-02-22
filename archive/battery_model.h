/* VUURWERK v1.0.4 - Battery Discharge Curve Model
 * Accurate battery percentage using discharge curve
 *
 * PROBLEM: Linear voltage-to-percentage is wrong
 * - Li-ion battery voltage is non-linear
 * - 4.2V = 100%, 3.7V = ~50%, 3.5V = ~10%
 * - Stock firmware uses simple linear scaling = inaccurate
 *
 * SOLUTION: Piecewise linear model of actual discharge curve
 */

#ifndef BATTERY_MODEL_H
#define BATTERY_MODEL_H

#include <stdint.h>
#include <stdbool.h>

// Battery type
typedef enum {
	BATTERY_1600MAH = 0,  // Stock battery
	BATTERY_2200MAH = 1   // Extended battery
} BatteryType_t;

// Battery model state
typedef struct {
	BatteryType_t type;
	uint16_t voltage_mV;     // Current voltage (millivolts)
	uint8_t percentage;      // Calculated percentage (0-100)
	bool is_charging;        // Charging detected
	bool low_battery;        // Low battery warning (< 10%)
} BatteryModel_t;

extern BatteryModel_t gBatteryModel;

// Discharge curve points (voltage_mV, percentage)
// Li-ion 18650 typical curve
#define CURVE_POINTS  7

// Initialize battery model
void BATTERY_MODEL_Init(BatteryType_t type);

// Update battery voltage and calculate percentage
void BATTERY_MODEL_Update(uint16_t voltage_mV, bool is_charging);

// Get current battery percentage
uint8_t BATTERY_MODEL_GetPercentage(void);

// Check if low battery warning should be shown
bool BATTERY_MODEL_IsLow(void);

#endif // BATTERY_MODEL_H
