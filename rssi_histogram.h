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

#ifndef RSSI_HISTOGRAM_H
#define RSSI_HISTOGRAM_H

#include <stdint.h>
#include <stdbool.h>

#define RSSI_HIST_BINS 32
#define RSSI_HIST_MIN -130
#define RSSI_HIST_MAX -35
#define RSSI_HIST_BIN_WIDTH 3 // dBm per bin

typedef struct {
    uint16_t bins[RSSI_HIST_BINS];
    uint16_t total_samples;
    int8_t noise_floor_dbm;
    int8_t optimal_squelch_dbm;
    bool valid;
} RSSI_Histogram_t;

extern RSSI_Histogram_t gRSSI_Histogram[2];

void RSSI_HISTOGRAM_Init(void);
void RSSI_HISTOGRAM_Update(uint8_t vfo, int16_t rssi_dbm);
void RSSI_HISTOGRAM_Analyze(uint8_t vfo);
int8_t RSSI_HISTOGRAM_GetOptimalSquelch(uint8_t vfo);
void RSSI_HISTOGRAM_Reset(uint8_t vfo);

#endif