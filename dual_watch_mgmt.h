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

#ifndef DUAL_WATCH_MGMT_H
#define DUAL_WATCH_MGMT_H

#include <stdint.h>

typedef struct {
    uint16_t dwell_time_ms[2];
    uint16_t activity_count[2];
    int16_t avg_rssi[2];
    uint8_t priority_vfo;
} DualWatchMgmt_t;

extern DualWatchMgmt_t gDualWatchMgmt;

void DUAL_WATCH_MGMT_Init(void);
void DUAL_WATCH_MGMT_Update(uint8_t vfo, int16_t rssi, bool active);
uint16_t DUAL_WATCH_MGMT_GetDwellTime(uint8_t vfo);
void DUAL_WATCH_MGMT_ReportActivity(uint8_t vfo);

#endif