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

#include "dual_watch_mgmt.h"

DualWatchMgmt_t gDualWatchMgmt;

#define DEFAULT_DWELL_MS 500
#define MIN_DWELL_MS 200
#define MAX_DWELL_MS 2000

void DUAL_WATCH_MGMT_Init(void) {
    gDualWatchMgmt.dwell_time_ms[0] = DEFAULT_DWELL_MS;
    gDualWatchMgmt.dwell_time_ms[1] = DEFAULT_DWELL_MS;
    gDualWatchMgmt.activity_count[0] = 0;
    gDualWatchMgmt.activity_count[1] = 0;
    gDualWatchMgmt.avg_rssi[0] = -120;
    gDualWatchMgmt.avg_rssi[1] = -120;
    gDualWatchMgmt.priority_vfo = 0;
}

void DUAL_WATCH_MGMT_Update(uint8_t vfo, int16_t rssi, bool active) {
    if (vfo > 1) return;

    (void)active; // Reserved for future use

    // Update average RSSI (simple moving average)
    gDualWatchMgmt.avg_rssi[vfo] = (gDualWatchMgmt.avg_rssi[vfo] * 3 + rssi) / 4;

    // Calculate dwell times based on activity ratio
    uint16_t total_activity = gDualWatchMgmt.activity_count[0] + gDualWatchMgmt.activity_count[1];

    if (total_activity > 10) {
        // More active channel gets less dwell time (check it less often)
        // Less active channel gets more dwell time (monitor it more)
        uint16_t activity_0 = gDualWatchMgmt.activity_count[0];
        uint16_t activity_1 = gDualWatchMgmt.activity_count[1];

        if (activity_0 > activity_1) {
            // VFO 0 more active, reduce its dwell
            gDualWatchMgmt.dwell_time_ms[0] = DEFAULT_DWELL_MS - 100;
            gDualWatchMgmt.dwell_time_ms[1] = DEFAULT_DWELL_MS + 100;
        } else if (activity_1 > activity_0) {
            // VFO 1 more active, reduce its dwell
            gDualWatchMgmt.dwell_time_ms[0] = DEFAULT_DWELL_MS + 100;
            gDualWatchMgmt.dwell_time_ms[1] = DEFAULT_DWELL_MS - 100;
        } else {
            // Equal activity, balanced dwell
            gDualWatchMgmt.dwell_time_ms[0] = DEFAULT_DWELL_MS;
            gDualWatchMgmt.dwell_time_ms[1] = DEFAULT_DWELL_MS;
        }

        // Clamp dwell times
        for (uint8_t i = 0; i < 2; i++) {
            if (gDualWatchMgmt.dwell_time_ms[i] < MIN_DWELL_MS)
                gDualWatchMgmt.dwell_time_ms[i] = MIN_DWELL_MS;
            if (gDualWatchMgmt.dwell_time_ms[i] > MAX_DWELL_MS)
                gDualWatchMgmt.dwell_time_ms[i] = MAX_DWELL_MS;
        }
    }
}

uint16_t DUAL_WATCH_MGMT_GetDwellTime(uint8_t vfo) {
    if (vfo > 1) return DEFAULT_DWELL_MS;
    return gDualWatchMgmt.dwell_time_ms[vfo];
}

void DUAL_WATCH_MGMT_ReportActivity(uint8_t vfo) {
    if (vfo > 1) return;

    if (gDualWatchMgmt.activity_count[vfo] < 0xFFFF) {
        gDualWatchMgmt.activity_count[vfo]++;
    }

    // Decay activity counters over time
    if ((gDualWatchMgmt.activity_count[vfo] & 0xFF) == 0) {
        gDualWatchMgmt.activity_count[0] = (gDualWatchMgmt.activity_count[0] * 3) / 4;
        gDualWatchMgmt.activity_count[1] = (gDualWatchMgmt.activity_count[1] * 3) / 4;
    }
}