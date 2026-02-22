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

#ifndef GAIN_STAGING_H
#define GAIN_STAGING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  table_index;       // Current index into am_fix gain_table[]
    uint8_t  table_index_prev;  // Previous index (for change detection)
    int16_t  prev_rssi;         // Previous RSSI for averaging
    uint16_t hold_counter;      // Hold timer (in 10ms ticks, prevents hunting)
    int16_t  target_rssi_dBm;   // Target RSSI (varies by modulation)
} GainStaging_t;

extern GainStaging_t gGainStaging[2];

void GAIN_STAGING_Init(void);

// Main 10ms tick — call every 10ms from APP_TimeSlice10ms.
// Handles its own state checking: runs during FOREGROUND + RX,
// skips TX and power save. Auto-resets on frequency change.
// Reads RSSI, updates gain, writes REG_13 as needed.
void GAIN_STAGING_10ms(uint8_t vfo);

// Reset gain state (call on frequency/channel change)
void GAIN_STAGING_Reset(uint8_t vfo);

// Get gain compensation for S-meter display
// Returns: dB of gain reduction applied (0 = stock, positive = reduced)
int8_t GAIN_STAGING_GetGainDiff(uint8_t vfo);

#endif