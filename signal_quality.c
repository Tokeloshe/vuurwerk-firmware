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

#include "signal_quality.h"

RssiHistory_t gRssiHistory = {
	.history = {0},
	.index = 0,
	.count = 0
};

void SIGNAL_QUALITY_Init(void)
{
	for (uint8_t i = 0; i < RSSI_HISTORY_SIZE; i++)
		gRssiHistory.history[i] = 0;

	gRssiHistory.index = 0;
	gRssiHistory.count = 0;
}

void SIGNAL_QUALITY_Update(int16_t rssi_dBm)
{
	// Add to ring buffer
	gRssiHistory.history[gRssiHistory.index] = rssi_dBm;
	gRssiHistory.index = (gRssiHistory.index + 1) & 7;  // % 8 via AND

	if (gRssiHistory.count < RSSI_HISTORY_SIZE)
		gRssiHistory.count++;
}

SignalQuality_t SIGNAL_QUALITY_Get(void)
{
	if (gRssiHistory.count < RSSI_HISTORY_SIZE)
		return QUALITY_POOR;  // Not enough data yet

	// Calculate variance using integer math ONLY
	// variance = (sum_of_squares / N) - (mean * mean)

	int32_t sum = 0;
	int32_t sum_sq = 0;

	for (uint8_t i = 0; i < RSSI_HISTORY_SIZE; i++) {
		int16_t val = gRssiHistory.history[i];
		sum += val;
		sum_sq += (val * val);
	}

	// mean = sum >> 3  (divide by 8, power of 2)
	int16_t mean = sum >> 3;

	// variance = (sum_sq >> 3) - (mean * mean)
	int32_t variance = (sum_sq >> 3) - ((int32_t)mean * mean);

	// Compare against SQUARED thresholds (no sqrt needed!)
	// Thresholds: 100, 36, 9 (corresponding to std dev of ~10, ~6, ~3 dBm)

	if (variance < 9)
		return QUALITY_EXCELLENT;  // < 3dB variation
	else if (variance < 36)
		return QUALITY_GOOD;       // < 6dB variation
	else if (variance < 100)
		return QUALITY_FAIR;       // < 10dB variation
	else
		return QUALITY_POOR;       // >= 10dB variation (heavy fading)
}