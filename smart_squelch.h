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

#ifndef SMART_SQUELCH_H
#define SMART_SQUELCH_H

#include <stdint.h>

typedef struct {
	uint16_t noise_smooth;    // EWMA of REG_65 (noise indicator)
	uint16_t glitch_smooth;   // EWMA of REG_63 (glitch indicator)
	uint16_t rssi_smooth;     // EWMA of REG_67 (RSSI)
	int16_t  noise_floor;     // From RSSI histogram module
	uint8_t  voice_prob;      // 0-100, updated every 10ms tick
} SmartSquelchState_t;

extern SmartSquelchState_t gSmartSquelch;

void    SMART_SQUELCH_Update(void);           // Called every 10ms tick during RX
int8_t  SMART_SQUELCH_GetAdjustment(void);    // Squelch threshold offset in 0.5dB steps

#endif