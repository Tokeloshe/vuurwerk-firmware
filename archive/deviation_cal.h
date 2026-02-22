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

#ifndef DEVIATION_CAL_H
#define DEVIATION_CAL_H

#include <stdint.h>

// Offset range: -5 to +5 mic gain steps
#define DEVIATION_CAL_OFFSET_MIN  -5
#define DEVIATION_CAL_OFFSET_MAX   5

// Per-band TX audio offset
typedef struct {
	int8_t vhf_offset;  // VHF band offset (136-174 MHz) [-5..+5]
	int8_t uhf_offset;  // UHF band offset (400-520 MHz) [-5..+5]
} DeviationCal_t;

extern DeviationCal_t gDeviationCal;

// Initialize offsets to zero (no adjustment)
void DEVIATION_CAL_Init(void);

// Get per-band offset for TX Compressor
// frequency_10Hz: TX frequency in 10 Hz units
// Returns: mic gain offset (-5 to +5)
int8_t DEVIATION_CAL_GetOffset(uint32_t frequency_10Hz);

// Adjust offset (for UI/menu control)
// band: 0=VHF, 1=UHF
// delta: -1 or +1
void DEVIATION_CAL_Adjust(uint8_t band, int8_t delta);

// Get display value (100 + offset*5, so 75-125 range for UI)
// band: 0=VHF, 1=UHF
uint8_t DEVIATION_CAL_Get(uint8_t band);

#endif // DEVIATION_CAL_H