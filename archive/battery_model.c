/* VUURWERK v1.0.4 - Battery Model Implementation
 *
 * Li-ion discharge curve (typical 18650):
 * - 4.20V = 100% (fully charged)
 * - 4.00V = 90%
 * - 3.80V = 70%
 * - 3.70V = 50%
 * - 3.60V = 30%
 * - 3.50V = 10%
 * - 3.30V = 0% (cutoff)
 */

#include "battery_model.h"

BatteryModel_t gBatteryModel = {
	.type = BATTERY_1600MAH,
	.voltage_mV = 3700,
	.percentage = 50,
	.is_charging = false,
	.low_battery = false
};

// Discharge curve lookup table
// Format: {voltage_mV, percentage}
static const struct {
	uint16_t voltage_mV;
	uint8_t percentage;
} discharge_curve[CURVE_POINTS] = {
	{4200, 100},
	{4000,  90},
	{3800,  70},
	{3700,  50},
	{3600,  30},
	{3500,  10},
	{3300,   0}
};

void BATTERY_MODEL_Init(BatteryType_t type)
{
	gBatteryModel.type = type;
	gBatteryModel.voltage_mV = 3700;
	gBatteryModel.percentage = 50;
	gBatteryModel.is_charging = false;
	gBatteryModel.low_battery = false;
}

void BATTERY_MODEL_Update(uint16_t voltage_mV, bool is_charging)
{
	gBatteryModel.voltage_mV = voltage_mV;
	gBatteryModel.is_charging = is_charging;

	// If charging, show increasing percentage
	if (is_charging) {
		// Simple linear during charging
		if (voltage_mV >= 4200) {
			gBatteryModel.percentage = 100;
		} else if (voltage_mV <= 3300) {
			gBatteryModel.percentage = 0;
		} else {
			// Linear: (V - 3300) / (4200 - 3300) * 100
			gBatteryModel.percentage = ((voltage_mV - 3300) * 100) / 900;
		}
	} else {
		// Use piecewise linear interpolation of discharge curve
		uint8_t percentage = 0;

		// Find which curve segment we're in
		for (uint8_t i = 0; i < (CURVE_POINTS - 1); i++) {
			uint16_t v_high = discharge_curve[i].voltage_mV;
			uint16_t v_low = discharge_curve[i + 1].voltage_mV;
			uint8_t p_high = discharge_curve[i].percentage;
			uint8_t p_low = discharge_curve[i + 1].percentage;

			if (voltage_mV >= v_low && voltage_mV <= v_high) {
				// Interpolate within this segment
				// percentage = p_low + (V - v_low) * (p_high - p_low) / (v_high - v_low)
				int32_t v_range = v_high - v_low;
				int32_t p_range = p_high - p_low;
				int32_t v_offset = voltage_mV - v_low;

				percentage = p_low + ((v_offset * p_range) / v_range);
				break;
			}
		}

		// Clamp to 0-100
		if (percentage > 100) percentage = 100;

		gBatteryModel.percentage = percentage;
	}

	// Low battery warning at < 10%
	gBatteryModel.low_battery = (gBatteryModel.percentage < 10);
}

uint8_t BATTERY_MODEL_GetPercentage(void)
{
	return gBatteryModel.percentage;
}

bool BATTERY_MODEL_IsLow(void)
{
	return gBatteryModel.low_battery;
}
