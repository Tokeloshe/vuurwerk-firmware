/* Copyright (c) 2026 James Honiball (KC3TFZ)
 * 
 * This file is part of VUURWERK and is dual-licensed:
 *   1. GPL v3 (when distributed as part of the VUURWERK firmware)
 *   2. Commercial license available from the author
 * 
 * You may not extract, repackage, or redistribute this file 
 * independently under any license other than GPL v3 as part 
 * of the complete VUURWERK firmware, without written permission
 * from the author.
 */

#include "tx_soft_start.h"
#include "driver/bk4819.h"

TxSoftStart_t gTxSoftStart = {
	.active       = false,
	.step         = 0,
	.target_power = 0,
	.frequency    = 0
};

void TX_SOFT_START_Init(void)
{
	gTxSoftStart.active       = false;
	gTxSoftStart.step         = 0;
	gTxSoftStart.target_power = 0;
	gTxSoftStart.frequency    = 0;
}

void TX_SOFT_START_Begin(uint8_t target_power, uint32_t frequency)
{
	gTxSoftStart.target_power = target_power;
	gTxSoftStart.frequency    = frequency;
	gTxSoftStart.step         = 0;
	gTxSoftStart.active       = true;

	// Start at zero power
	BK4819_SetupPowerAmplifier(0, frequency);
}

// Precomputed S-curve: (1 - cos(pi * step / 5)) / 2 * 255
static const uint8_t s_curve[5] = { 19, 75, 128, 181, 237 };

void TX_SOFT_START_Process(void)
{
	if (!gTxSoftStart.active)
		return;

	gTxSoftStart.step++;

	if (gTxSoftStart.step >= TX_RAMP_STEPS) {
		// Ramp complete — set full target power
		BK4819_SetupPowerAmplifier(gTxSoftStart.target_power, gTxSoftStart.frequency);
		gTxSoftStart.active = false;
	} else {
		// S-curve power ramp for smoother transition
		uint8_t power = ((uint16_t)gTxSoftStart.target_power * s_curve[gTxSoftStart.step - 1]) >> 8;
		BK4819_SetupPowerAmplifier(power, gTxSoftStart.frequency);
	}
}