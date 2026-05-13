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

#include "dual_watch_mgmt.h"

#define DEFAULT_DWELL_MS 500

typedef struct {
    uint16_t dwell_time_ms[2];
    uint16_t activity_count[2];
} DualWatchMgmt_t;

static DualWatchMgmt_t s_state;

void DUAL_WATCH_MGMT_Init(void) {
    s_state.dwell_time_ms[0] = DEFAULT_DWELL_MS;
    s_state.dwell_time_ms[1] = DEFAULT_DWELL_MS;
}

uint16_t DUAL_WATCH_MGMT_GetDwellTime(uint8_t vfo) {
    if (vfo > 1) return DEFAULT_DWELL_MS;
    return s_state.dwell_time_ms[vfo];
}

void DUAL_WATCH_MGMT_ReportActivity(uint8_t vfo, uint16_t weight) {
    if (vfo > 1) return;

    if (weight == 0) weight = 1;
    if (weight > 255) weight = 255;

    uint32_t new_count = (uint32_t)s_state.activity_count[vfo] + weight;
    s_state.activity_count[vfo] = (new_count > 0xFFFF) ? 0xFFFF : (uint16_t)new_count;

    static uint8_t s_report_counter = 0;
    if (++s_report_counter == 0) {
        s_state.activity_count[0] = (s_state.activity_count[0] * 3) / 4;
        s_state.activity_count[1] = (s_state.activity_count[1] * 3) / 4;
    }

    uint16_t total_activity = s_state.activity_count[0] + s_state.activity_count[1];
    if (total_activity > 10) {
        uint16_t activity_0 = s_state.activity_count[0];
        uint16_t activity_1 = s_state.activity_count[1];

        if (activity_0 > activity_1) {
            s_state.dwell_time_ms[0] = DEFAULT_DWELL_MS - 100;
            s_state.dwell_time_ms[1] = DEFAULT_DWELL_MS + 100;
        } else if (activity_1 > activity_0) {
            s_state.dwell_time_ms[0] = DEFAULT_DWELL_MS + 100;
            s_state.dwell_time_ms[1] = DEFAULT_DWELL_MS - 100;
        } else {
            s_state.dwell_time_ms[0] = DEFAULT_DWELL_MS;
            s_state.dwell_time_ms[1] = DEFAULT_DWELL_MS;
        }
    }
}
