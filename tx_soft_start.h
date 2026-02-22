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

#ifndef TX_SOFT_START_H
#define TX_SOFT_START_H

#include <stdint.h>
#include <stdbool.h>

#define TX_RAMP_STEPS  6   // 6 steps at 10ms/tick = 60ms ramp (uses all 5 s_curve entries)

typedef struct {
	bool     active;          // Currently ramping
	uint8_t  step;            // Current ramp step (0 to TX_RAMP_STEPS)
	uint8_t  target_power;    // Target PA bias level
	uint32_t frequency;       // TX frequency (needed by SetupPowerAmplifier)
} TxSoftStart_t;

extern TxSoftStart_t gTxSoftStart;

void TX_SOFT_START_Init(void);
void TX_SOFT_START_Begin(uint8_t target_power, uint32_t frequency);  // Called once at TX start
void TX_SOFT_START_Process(void);  // Called every 10ms during TX

#endif // TX_SOFT_START_H