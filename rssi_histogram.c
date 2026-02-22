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

#include "rssi_histogram.h"
#include <string.h>

RSSI_Histogram_t gRSSI_Histogram[2];

void RSSI_HISTOGRAM_Init(void) {
    for (uint8_t i = 0; i < 2; i++) {
        RSSI_HISTOGRAM_Reset(i);
    }
}

void RSSI_HISTOGRAM_Update(uint8_t vfo, int16_t rssi_dbm) {
    if (vfo > 1) return;

    RSSI_Histogram_t *h = &gRSSI_Histogram[vfo];

    // Convert RSSI to bin index
    int16_t bin_idx = (rssi_dbm - RSSI_HIST_MIN) / RSSI_HIST_BIN_WIDTH;

    if (bin_idx < 0) bin_idx = 0;
    if (bin_idx >= RSSI_HIST_BINS) bin_idx = RSSI_HIST_BINS - 1;

    // Increment bin (with saturation)
    if (h->bins[bin_idx] < 0xFFFF) {
        h->bins[bin_idx]++;
    }

    h->total_samples++;

    // Auto-analyze every 256 samples
    if ((h->total_samples & 0xFF) == 0) {
        RSSI_HISTOGRAM_Analyze(vfo);
    }
}

void RSSI_HISTOGRAM_Analyze(uint8_t vfo) {
    if (vfo > 1) return;

    RSSI_Histogram_t *h = &gRSSI_Histogram[vfo];

    if (h->total_samples < 100) {
        h->valid = false;
        return;
    }

    // Find peak bin (mode of distribution = noise floor)
    uint16_t max_count = 0;
    uint8_t peak_bin = 0;

    for (uint8_t i = 0; i < RSSI_HIST_BINS; i++) {
        if (h->bins[i] > max_count) {
            max_count = h->bins[i];
            peak_bin = i;
        }
    }

    // Noise floor is center of peak bin
    h->noise_floor_dbm = RSSI_HIST_MIN + (peak_bin * RSSI_HIST_BIN_WIDTH) + (RSSI_HIST_BIN_WIDTH / 2);

    // Optimal squelch is 6dB above noise floor
    h->optimal_squelch_dbm = h->noise_floor_dbm + 6;

    h->valid = true;
}

int8_t RSSI_HISTOGRAM_GetOptimalSquelch(uint8_t vfo) {
    if (vfo > 1) return -110;

    if (!gRSSI_Histogram[vfo].valid) {
        return -110; // Default
    }

    return gRSSI_Histogram[vfo].optimal_squelch_dbm;
}

void RSSI_HISTOGRAM_Reset(uint8_t vfo) {
    if (vfo > 1) return;

    memset(&gRSSI_Histogram[vfo], 0, sizeof(RSSI_Histogram_t));
    gRSSI_Histogram[vfo].noise_floor_dbm = -120;
    gRSSI_Histogram[vfo].optimal_squelch_dbm = -110;
}