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

#ifndef SQUELCH_H
#define SQUELCH_H

#include <stdint.h>
#include <stdbool.h>

// Adaptive noise floor squelch state
typedef struct {
	int16_t  noise_floor_dBm;        // Current adaptive noise floor
	uint8_t  squelch_open_count;     // Hysteresis counter (0-10)
	uint32_t noise_samples;          // Total samples collected
} AdaptiveSquelch_t;

// Global state (defined in squelch.c)
extern AdaptiveSquelch_t gAdaptiveSquelch;

// Initialize squelch system
void SQUELCH_Init(void);

#endif // SQUELCH_H