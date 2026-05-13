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

#include <limits.h>
#include "rssi_filter.h"

// alpha = 1/(2^ALPHA_SHIFT). 3 -> alpha = 1/8, tau ~ 80 ms at 10 ms tick.
#define ALPHA_SHIFT 3

// INT16_MAX is the "not yet initialized" sentinel. Real filtered RSSI is
// in dBm and bounded by BK4819 hardware to roughly [-160, -33], so the
// sentinel is collision-free.
typedef struct {
    int16_t filtered_rssi;
} RSSI_Filter_t;

static RSSI_Filter_t s_filter[2];

void RSSI_FILTER_Init(void) {
    s_filter[0].filtered_rssi = INT16_MAX;
    s_filter[1].filtered_rssi = INT16_MAX;
}

int16_t RSSI_FILTER_Update(uint8_t vfo, int16_t raw_rssi) {
    if (vfo > 1) return raw_rssi;

    RSSI_Filter_t *f = &s_filter[vfo];

    // First sample initializes filter
    if (f->filtered_rssi == INT16_MAX) {
        f->filtered_rssi = raw_rssi;
        return raw_rssi;
    }

    // EWMA: y = (x + ((2^ALPHA_SHIFT)-1)*y_prev) >> ALPHA_SHIFT
    int32_t temp = (int32_t)raw_rssi + 7 * (int32_t)f->filtered_rssi;
    f->filtered_rssi = (int16_t)(temp >> ALPHA_SHIFT);

    return f->filtered_rssi;
}
