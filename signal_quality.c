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
 *
 * Commercial licensing inquiries: jhoniball4@gmail.com
 */

#include "signal_quality.h"

RssiHistory_t gRssiHistory;

// Last RX frequency seen at Update time; a change discards the
// stale ring buffer so variance reflects within-channel noise
// rather than the inter-channel RSSI delta during VFO flips.
static uint32_t last_frequency = 0;

// G051: keep every 4th 10 ms sample, so the 8-entry window spans
// 320 ms and catches mobile flutter instead of 80 ms of one fade.
static uint8_t decimate = 0;

void SIGNAL_QUALITY_Update(int16_t rssi_dBm, uint32_t frequency)
{
	if (frequency != last_frequency) {
		last_frequency = frequency;
		gRssiHistory.index = 0;
		gRssiHistory.count = 0;
	}

	if (++decimate & 3)
		return;

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
	// variance = (N * sum_of_squares - sum * sum) / (N * N)

	int32_t sum = 0;
	int32_t sum_sq = 0;

	for (uint8_t i = 0; i < RSSI_HISTORY_SIZE; i++) {
		int16_t val = gRssiHistory.history[i];
		sum += val;
		sum_sq += (val * val);
	}

	// G051: exact form. The old floored mean (sum >> 3 of a negative dBm
	// sum) drove most windows negative, which read as EXCELLENT.
	// |sum| <= 8 * 160, so neither product overflows.
	int32_t variance = ((sum_sq << 3) - sum * sum) >> 6;

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