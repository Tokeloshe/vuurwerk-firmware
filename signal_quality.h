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

#ifndef SIGNAL_QUALITY_H
#define SIGNAL_QUALITY_H

#include <stdint.h>

// Signal quality levels
typedef enum {
	QUALITY_POOR = 0,       // Heavy fading (variance >= 100)
	QUALITY_FAIR = 1,       // Noticeable fading (variance >= 36)
	QUALITY_GOOD = 2,       // Minor fading (variance >= 9)
	QUALITY_EXCELLENT = 3   // Rock solid (variance < 9)
} SignalQuality_t;

// RSSI history for variance calculation
#define RSSI_HISTORY_SIZE 8
typedef struct {
	int16_t history[RSSI_HISTORY_SIZE];  // Last 8 RSSI readings
	uint8_t index;                        // Ring buffer index
	uint8_t count;                        // Samples collected (0-8)
} RssiHistory_t;

extern RssiHistory_t gRssiHistory;

// Initialize signal quality tracker
void SIGNAL_QUALITY_Init(void);

// Update with new RSSI reading
void SIGNAL_QUALITY_Update(int16_t rssi_dBm);

// Get current signal quality level
// Returns: 0-3 (poor/fair/good/excellent)
SignalQuality_t SIGNAL_QUALITY_Get(void);

#endif // SIGNAL_QUALITY_H