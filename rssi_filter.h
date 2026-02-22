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

#ifndef RSSI_FILTER_H
#define RSSI_FILTER_H

#include <stdint.h>
#include <stdbool.h>

// EWMA filter state
typedef struct {
    int16_t filtered_rssi;
    uint8_t alpha_shift;    // alpha = 1/(2^shift), lower = smoother
    bool    initialized;    // true after first sample received
} RSSI_Filter_t;

extern RSSI_Filter_t gRSSI_Filter[2]; // One per VFO

void RSSI_FILTER_Init(void);
int16_t RSSI_FILTER_Update(uint8_t vfo, int16_t raw_rssi);
void RSSI_FILTER_Reset(uint8_t vfo);

#endif