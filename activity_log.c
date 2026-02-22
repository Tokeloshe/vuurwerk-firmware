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

#include <stddef.h>
#include <string.h>
#include "activity_log.h"

ActivityLog_t gActivityLog;

void ACTIVITY_LOG_Init(void)
{
	memset(&gActivityLog, 0, sizeof(gActivityLog));
}

void ACTIVITY_LOG_Add(uint32_t freq_10Hz, int16_t rssi_dBm, uint16_t ctcss_freq_10Hz, uint8_t quality)
{
	// Continuation check: same freq within ±10 kHz and 10 seconds updates existing entry
	if (gActivityLog.count > 0) {
		uint8_t li = (gActivityLog.write_index == 0) ? (ACTIVITY_LOG_SIZE - 1) : (gActivityLog.write_index - 1);
		ActivityEntry_t *last = &gActivityLog.entries[li];
		int32_t fd = (int32_t)freq_10Hz - (int32_t)last->freq_10Hz;
		if (fd < 0) fd = -fd;
		if (fd <= 1000 && (gActivityLog.uptime_s - last->timestamp_s) <= 10) {
			last->duration_s = gActivityLog.uptime_s - last->timestamp_s;
			if (rssi_dBm > last->rssi_dBm) last->rssi_dBm = rssi_dBm;
			if (ctcss_freq_10Hz) last->ctcss_freq_10Hz = ctcss_freq_10Hz;
			if (quality > last->quality) last->quality = quality;
			return;
		}
	}

	ActivityEntry_t *e = &gActivityLog.entries[gActivityLog.write_index];
	e->freq_10Hz = freq_10Hz;
	e->timestamp_s = gActivityLog.uptime_s;
	e->duration_s = 0;
	e->rssi_dBm = rssi_dBm;
	e->ctcss_freq_10Hz = ctcss_freq_10Hz;
	e->quality = quality;

	if (++gActivityLog.write_index >= ACTIVITY_LOG_SIZE)
		gActivityLog.write_index = 0;
	if (gActivityLog.count < ACTIVITY_LOG_SIZE)
		gActivityLog.count++;
}

const ActivityEntry_t* ACTIVITY_LOG_Get(uint8_t index)
{
	if (index >= gActivityLog.count)
		return NULL;

	// Calculate actual index (most recent first)
	// write_index points to next write position
	// Most recent entry is at (write_index - 1)
	int16_t actual_index = (int16_t)gActivityLog.write_index - 1 - index;
	if (actual_index < 0) {
		actual_index += ACTIVITY_LOG_SIZE;
	}

	return &gActivityLog.entries[actual_index];
}

uint8_t ACTIVITY_LOG_GetCount(void)
{
	return gActivityLog.count;
}

void ACTIVITY_LOG_Clear(void)
{
	gActivityLog.write_index = 0;
	gActivityLog.count = 0;
}

void ACTIVITY_LOG_UpdateUptime(void)
{
	if (gActivityLog.uptime_s < 65535) {
		gActivityLog.uptime_s++;
	}
	// If wrapped, all timestamps become relative (duration still valid)
}

uint8_t ACTIVITY_LOG_FindFrequency(uint32_t freq_10Hz)
{
	for (uint8_t i = 0; i < gActivityLog.count; i++) {
		const ActivityEntry_t *entry = ACTIVITY_LOG_Get(i);
		if (entry == NULL) break;

		// Check if frequency matches (within ±10 kHz)
		int32_t freq_diff = (int32_t)freq_10Hz - (int32_t)entry->freq_10Hz;
		if (freq_diff < 0) freq_diff = -freq_diff;

		if (freq_diff <= 1000) {  // ±10 kHz
			return i;
		}
	}

	return 255;  // Not found
}