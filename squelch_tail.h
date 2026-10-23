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

#ifndef SQUELCH_TAIL_H
#define SQUELCH_TAIL_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	STE_IDLE,        // Not monitoring (no CTCSS/DCS active or not in RX)
	STE_MONITORING,  // Code active, watching for tone/code loss
	STE_TONE_LOST,   // Tone/code disappeared, confirming (20-30ms)
	STE_MUTED,       // Audio muted, waiting for carrier to drop
} STE_State_t;

typedef struct {
	STE_State_t state;
	uint8_t     lost_count;       // Ticks since tone lost (10ms each)
	uint8_t     mute_count;       // Ticks since audio muted
	uint8_t     monitor_streak;   // Saturating consecutive-present count
	                              // in MONITORING (caps 8); low values
	                              // demand longer loss confirmation
	uint8_t     seen;             // v1.3.3: latched -- the tone/code was
	                              // affirmatively detected at least once
	                              // since arming. Only then may the machine
	                              // walk to the mute path. Survives the
	                              // flutter streak resets (monitor_streak is
	                              // zeroed mid-burst); cleared only at arm.
} SquelchTail_t;

extern SquelchTail_t gSquelchTail;

// Called every 10ms. Self-activates on RX with CTCSS (G054: interrupt-
// maintained g_CTCSS_Lost) or, v1.3.1 (T4), DCS (g_CDCSS_Lost); the DCS
// turn-off code (134.4Hz, ~180ms) drops the detector exactly like tone loss.
void SQUELCH_TAIL_Process(void);

#endif