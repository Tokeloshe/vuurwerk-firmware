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

#include "squelch_tail.h"
#include "driver/bk4819.h"
#include "functions.h"
#include "misc.h"      // v1.3.1 (T4): g_CDCSS_Lost for the DCS arm path
#include "radio.h"
#include "settings.h"
#include "dcs.h"

SquelchTail_t gSquelchTail;

void SQUELCH_TAIL_Process(void)
{
	if (!gEeprom.TAIL_TONE_ELIMINATION)
		return;

	const bool in_rx = (gCurrentFunction == FUNCTION_RECEIVE ||
	                    gCurrentFunction == FUNCTION_INCOMING);

	// Self-activate: enter MONITORING when RX starts with CTCSS or DCS.
	// VUURWERK v1.3.0 (F4): test gCurrentCodeType, not pRX->CodeType.
	// FUNCTION_Init forces gCurrentCodeType to CODE_TYPE_OFF for non-FM,
	// so an AM/USB channel carrying a stray CTCSS code in its channel
	// record no longer arms STE against an unconfigured tone detector
	// (REG_0C reads garbage there, producing a ~5 Hz audio chop cycle).
	// v1.3.1 (T4): DCS channels join the same machine, presence via the
	// interrupt-maintained g_CDCSS_Lost. v1.3.3: arming here proves nothing
	// about whether the code was ever actually received -- a co-channel
	// carrier with a non-matching/absent code would still walk to MUTED and
	// oscillate (5 Hz chop that DEFEATS tone squelch). The `seen` latch below
	// gates the mute path on real detection; reset it on every arm.
	if (gSquelchTail.state == STE_IDLE) {
		if (in_rx && (gCurrentCodeType == CODE_TYPE_CONTINUOUS_TONE ||
		              gCurrentCodeType == CODE_TYPE_DIGITAL ||
		              gCurrentCodeType == CODE_TYPE_REVERSE_DIGITAL)) {
			gSquelchTail.state          = STE_MONITORING;
			gSquelchTail.lost_count     = 0;
			gSquelchTail.mute_count     = 0;
			gSquelchTail.monitor_streak = 0;
			gSquelchTail.seen           = 0;
		}
		return;
	}

	// Left RX while active -- reset
	if (!in_rx) {
		// Stock code handles AF restore on squelch close
		gSquelchTail.state = STE_IDLE;
		return;
	}

	// CTCSS and DCS both use the interrupt-maintained detect state.
	// G054: CTCSS used to poll REG_0C bit 1, documented as the squelch
	// result rather than tone presence; g_CTCSS_Lost is the flag stock
	// HandleIncoming/HandleReceive act on. v1.3.3 POLARITY FIX:
	// in this firmware g_CDCSS_Lost == true means the code IS PRESENT (the
	// "quiet was lost" sense, same as g_SquelchLost/g_VOX_Lost; g_CTCSS_Lost
	// too) -- it is exactly the condition HandleIncoming opens audio on and
	// HandleReceive treats as "keep RX open" (app.c: g_CDCSS_Lost && CDCSS_POSITIVE_CODE).
	// The v1.3.1 T4 code used !g_CDCSS_Lost, which was inverted and chopped
	// real DCS receptions. Match the stock detect condition, polarity and all.
	const bool is_dcs = (gCurrentCodeType != CODE_TYPE_CONTINUOUS_TONE);
	const bool tone_present = is_dcs
		? (g_CDCSS_Lost && gCDCSSCodeType == CDCSS_POSITIVE_CODE)
		: g_CTCSS_Lost;

	switch (gSquelchTail.state) {
	case STE_MONITORING:
		if (tone_present) {
			// Confidence-weighted: track consecutive-present streak
			// (saturate at 8) so loss confirmation can demand a
			// longer window when the recent signal was fluttery.
			if (gSquelchTail.monitor_streak < 8)
				gSquelchTail.monitor_streak++;
			gSquelchTail.seen = 1;   // v1.3.3: proof the code was really here
		} else if (gSquelchTail.seen) {
			// v1.3.3: only arm the loss/mute path if the tone/code was
			// actually detected since arming. A carrier whose code never
			// matched parks here and STE never touches the AF path, so
			// hardware tone squelch is never defeated (no 5 Hz chop).
			gSquelchTail.lost_count = 1;
			gSquelchTail.state = STE_TONE_LOST;
		}
		break;

	case STE_TONE_LOST:
		if (tone_present) {
			// False alarm -- tone came back (flutter/fade).
			// Reset the streak: a false alarm is itself flutter
			// evidence, so the next loss should demand the
			// extended confirmation window.
			gSquelchTail.monitor_streak = 0;
			gSquelchTail.state = STE_MONITORING;
			break;
		}
		gSquelchTail.lost_count++;
		{
			// Stable signal (streak >= 4): 20ms confirm (orig).
			// Fluttery signal (streak < 4): 30ms confirm so brief
			// 20-25ms detector dropouts in fade no longer pre-mute.
			// Real reverse-burst tails (180-250ms) still confirm.
			const uint8_t threshold =
				(gSquelchTail.monitor_streak < 4) ? 3 : 2;
			if (gSquelchTail.lost_count >= threshold) {
				// Confirmed -- MUTE NOW before noise burst
				BK4819_SetAF(BK4819_AF_MUTE);
				gSquelchTail.state = STE_MUTED;
				gSquelchTail.mute_count = 0;
			}
		}
		break;

	case STE_MUTED:
		gSquelchTail.mute_count++;
		if (tone_present && gSquelchTail.mute_count > 3) {
			// Tone came back during mute -- unmute and resume
			// monitoring with a fresh streak (the mute itself was
			// retrospectively a false alarm).
			RADIO_SetModulation(gRxVfo->Modulation);
			gSquelchTail.monitor_streak = 0;
			gSquelchTail.state = STE_MONITORING;
		}
		else if (gSquelchTail.mute_count >= (is_dcs ? 25 : 15)) {
			// 150ms timeout (CTCSS) -- restore audio, return to idle.
			// Must unmute: on repeaters carrier stays up,
			// APP_StartListening won't run again to restore AF.
			// v1.3.1 (T4): DCS gets 250ms; the turn-off code itself runs
			// ~180ms before the carrier drops, so a 150ms unmute would
			// reopen audio into the last slice of code + tail.
			RADIO_SetModulation(gRxVfo->Modulation);
			gSquelchTail.state = STE_IDLE;
		}
		break;

	default:
		break;
	}
}