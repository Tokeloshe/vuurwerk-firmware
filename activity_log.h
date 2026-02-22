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

#ifndef ACTIVITY_LOG_H
#define ACTIVITY_LOG_H

#include <stdint.h>
#include <stdbool.h>

// Activity log entry
typedef struct {
	uint32_t freq_10Hz;          // Frequency in 10 Hz units
	uint16_t timestamp_s;        // Time since boot (seconds, wraps at 65535)
	uint16_t duration_s;         // Activity duration (seconds)
	int16_t rssi_dBm;            // Peak RSSI during activity
	uint16_t ctcss_freq_10Hz;    // CTCSS tone (0 if none)
	uint8_t quality;             // Signal quality (0-3)
} ActivityEntry_t;

// Activity log ring buffer
#define ACTIVITY_LOG_SIZE  20

typedef struct {
	ActivityEntry_t entries[ACTIVITY_LOG_SIZE];
	uint8_t write_index;         // Next write position
	uint8_t count;               // Entries in buffer (0-20)
	uint16_t uptime_s;           // System uptime counter (seconds)
} ActivityLog_t;

extern ActivityLog_t gActivityLog;

// Initialize activity log
void ACTIVITY_LOG_Init(void);

// Add activity entry
void ACTIVITY_LOG_Add(uint32_t freq_10Hz, int16_t rssi_dBm, uint16_t ctcss_freq_10Hz, uint8_t quality);

// Get entry by index (0 = most recent)
// Returns: Pointer to entry, or NULL if index out of range
const ActivityEntry_t* ACTIVITY_LOG_Get(uint8_t index);

// Get number of entries in log
uint8_t ACTIVITY_LOG_GetCount(void);

// Clear all entries
void ACTIVITY_LOG_Clear(void);

// Update uptime counter (call every second)
void ACTIVITY_LOG_UpdateUptime(void);

// Find most recent entry for a frequency (within ±10 kHz)
// Returns: Index of entry, or 255 if not found
uint8_t ACTIVITY_LOG_FindFrequency(uint32_t freq_10Hz);

#endif // ACTIVITY_LOG_H