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

	// Self-activate: enter MONITORING when RX starts with CTCSS
	if (gSquelchTail.state == STE_IDLE) {
		if (in_rx && gRxVfo->pRX->CodeType == CODE_TYPE_CONTINUOUS_TONE) {
			gSquelchTail.state          = STE_MONITORING;
			gSquelchTail.lost_count     = 0;
			gSquelchTail.mute_count     = 0;
			gSquelchTail.monitor_streak = 0;
		}
		return;
	}

	// Left RX while active -- reset
	if (!in_rx) {
		// Stock code handles AF restore on squelch close
		gSquelchTail.state = STE_IDLE;
		return;
	}

	// Read CTCSS found bit from BK4819 REG_0C
	const bool tone_present = (BK4819_ReadRegister(BK4819_REG_0C) >> 1) & 1;

	switch (gSquelchTail.state) {
	case STE_MONITORING:
		if (tone_present) {
			// Confidence-weighted: track consecutive-present streak
			// (saturate at 8) so loss confirmation can demand a
			// longer window when the recent signal was fluttery.
			if (gSquelchTail.monitor_streak < 8)
				gSquelchTail.monitor_streak++;
		} else {
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
		else if (gSquelchTail.mute_count >= 15) {
			// 150ms timeout -- restore audio, return to idle
			// Must unmute: on repeaters carrier stays up,
			// APP_StartListening won't run again to restore AF
			RADIO_SetModulation(gRxVfo->Modulation);
			gSquelchTail.state = STE_IDLE;
		}
		break;

	default:
		break;
	}
}