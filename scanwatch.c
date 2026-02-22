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

#include <string.h>
#include "scanwatch.h"

ScanWatch_t gScanWatch;

void SCANWATCH_Init(void)
{
    memset(&gScanWatch, 0, sizeof(gScanWatch));
}

void SCANWATCH_Enable(uint8_t scan_vfo)
{
    gScanWatch.state            = SCANWATCH_SCANNING;
    gScanWatch.scan_vfo         = scan_vfo & 1;
    gScanWatch.watch_vfo        = (scan_vfo & 1) ^ 1;
    gScanWatch.step_counter     = 0;
    gScanWatch.dwell_timer_10ms = 0;
}

void SCANWATCH_Disable(void)
{
    memset(&gScanWatch, 0, sizeof(gScanWatch));
}

bool SCANWATCH_OnScanStep(void)
{
    if (gScanWatch.state != SCANWATCH_SCANNING)
        return false;

    if (++gScanWatch.step_counter >= SCANWATCH_CHECK_EVERY_N_STEPS)
    {
        gScanWatch.step_counter     = 0;
        gScanWatch.state            = SCANWATCH_CHECKING;
        gScanWatch.dwell_timer_10ms = SCANWATCH_DWELL_10MS;
        return true;   // caller switches to watch VFO
    }
    return false;
}

bool SCANWATCH_Update(bool signal_detected)
{
    switch (gScanWatch.state)
    {
        case SCANWATCH_CHECKING:
            if (signal_detected)
            {
                gScanWatch.state            = SCANWATCH_LISTENING;
                gScanWatch.dwell_timer_10ms = SCANWATCH_HOLD_10MS;
                return false;   // stay on watch VFO
            }
            if (gScanWatch.dwell_timer_10ms > 0)
                gScanWatch.dwell_timer_10ms--;
            if (gScanWatch.dwell_timer_10ms == 0)
            {
                gScanWatch.state = SCANWATCH_SCANNING;
                return true;    // go back to scan VFO
            }
            return false;

        case SCANWATCH_LISTENING:
            if (signal_detected)
            {
                gScanWatch.dwell_timer_10ms = SCANWATCH_HOLD_10MS;
                return false;   // signal present, keep listening
            }
            if (gScanWatch.dwell_timer_10ms > 0)
                gScanWatch.dwell_timer_10ms--;
            if (gScanWatch.dwell_timer_10ms == 0)
            {
                gScanWatch.state = SCANWATCH_SCANNING;
                return true;    // signal gone, resume scan
            }
            return false;

        default:
            return false;
    }
}