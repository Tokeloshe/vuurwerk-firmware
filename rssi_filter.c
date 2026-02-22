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

#include "rssi_filter.h"

RSSI_Filter_t gRSSI_Filter[2];

void RSSI_FILTER_Init(void) {
    for (uint8_t i = 0; i < 2; i++) {
        gRSSI_Filter[i].filtered_rssi = -160;
        gRSSI_Filter[i].alpha_shift = 3; // alpha = 1/8
        gRSSI_Filter[i].initialized = false;
    }
}

int16_t RSSI_FILTER_Update(uint8_t vfo, int16_t raw_rssi) {
    if (vfo > 1) return raw_rssi;

    RSSI_Filter_t *f = &gRSSI_Filter[vfo];

    // First sample initializes filter
    if (!f->initialized) {
        f->filtered_rssi = raw_rssi;
        f->initialized = true;
        return raw_rssi;
    }

    // EWMA: y[n] = alpha*x[n] + (1-alpha)*y[n-1]
    // Using fixed-point: y = (x + (2^shift - 1)*y_prev) >> shift
    int32_t temp = (int32_t)raw_rssi + ((1 << f->alpha_shift) - 1) * (int32_t)f->filtered_rssi;
    f->filtered_rssi = (int16_t)(temp >> f->alpha_shift);

    return f->filtered_rssi;
}

void RSSI_FILTER_Reset(uint8_t vfo) {
    if (vfo < 2) {
        gRSSI_Filter[vfo].filtered_rssi = -160;
        gRSSI_Filter[vfo].initialized = false;
    }
}