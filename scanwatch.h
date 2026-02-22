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

#ifndef SCANWATCH_H
#define SCANWATCH_H

#include <stdint.h>
#include <stdbool.h>

// Tuning constants
#define SCANWATCH_CHECK_EVERY_N_STEPS  4     // Check watch VFO every N scan steps
#define SCANWATCH_DWELL_10MS           10    // 100ms dwell on watch VFO
#define SCANWATCH_HOLD_10MS            200   // 2s hold on watch VFO after signal

typedef enum {
    SCANWATCH_OFF = 0,
    SCANWATCH_SCANNING,    // On scan VFO, scanning normally
    SCANWATCH_CHECKING,    // Briefly on watch VFO, checking for signal
    SCANWATCH_LISTENING    // Signal found on watch VFO, listening
} ScanWatch_State_t;

typedef struct {
    ScanWatch_State_t state;
    uint8_t  scan_vfo;          // 0=A or 1=B — which VFO is scanning
    uint8_t  watch_vfo;         // the other VFO (always !scan_vfo)
    uint8_t  step_counter;      // counts scan steps until next watch check
    uint16_t dwell_timer_10ms;  // countdown timer for dwell/hold (in 10ms ticks)
} ScanWatch_t;

extern ScanWatch_t gScanWatch;

// Initialize all state to zero
void SCANWATCH_Init(void);

// Enable scan+watch. scan_vfo: 0=scan A watch B, 1=scan B watch A
void SCANWATCH_Enable(uint8_t scan_vfo);

// Disable scan+watch, reset state
void SCANWATCH_Disable(void);

// Called after each scan step completes (in CHFRSCANNER_ContinueScanning path)
// Returns true if caller should switch to watch VFO now
bool SCANWATCH_OnScanStep(void);

// Called every 10ms tick while state is CHECKING or LISTENING
// signal_detected: true if squelch is open on watch VFO
// Returns true if caller should switch back to scan VFO (resume scanning)
bool SCANWATCH_Update(bool signal_detected);

// Convenience check
static inline bool SCANWATCH_IsActive(void) {
    return gScanWatch.state != SCANWATCH_OFF;
}

// Convenience check — are we currently on the watch VFO?
static inline bool SCANWATCH_IsOnWatchVFO(void) {
    return gScanWatch.state == SCANWATCH_CHECKING || gScanWatch.state == SCANWATCH_LISTENING;
}

#endif