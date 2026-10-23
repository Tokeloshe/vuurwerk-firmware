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

#include <string.h>

#include "vfo_split.h"
#include "app/chFrScanner.h"          // G046: gScanStateDir scan yield
#include "app/scanner.h"              // G046: SCANNER_IsScanning scan yield
#include "audio.h"                    // v1.3.1 (U2): alert beep
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "external/printf/printf.h"  // v1.3.1 (U2): alert toast text
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "toast.h"                    // v1.3.1 (U2): alert toast

// G044: extern so gain staging and AM fix can hold off while the chip is on B.
SplitState_t split_state = SPLIT_STATE_IDLE;
static uint16_t hop_countdown = 0;
// G047: one bit per memory channel, set once its activity has alerted.
static uint8_t alerted[25];

// VUURWERK v1.3.0 (A2): dead scan sources removed (see vfo_split.h).
VfoSplit_t gVfoSplit = {
	.mode = SPLIT_OFF,
	.speed = B_SPEED_NORMAL,
	.hop_timer_ms = HOP_INTERVAL_NORMAL,
	.current_channel = 0,
	.current_freq_10Hz = 0,
	.scan_progress = 0,
	.alert = {.active = false}
};

// Advance to next VFO B frequency/channel
static void AdvanceVfoB(void)
{
	// VUURWERK v1.3.0 (A2): memory-channel walk is the only source.
	uint8_t start = gVfoSplit.current_channel;

	for (uint8_t i = 0; i < 200; i++) {
		if (++gVfoSplit.current_channel >= 200)
			gVfoSplit.current_channel = 0;
		if (RADIO_CheckValidChannel(gVfoSplit.current_channel, false, 0)) {
			// VUURWERK v1.3.0 (F3a): actually load the walked channel.
			// RADIO_ConfigureChannel reads gEeprom.ScreenChannel[1], so
			// commit the walked channel to VFO B before the reload; B's
			// display genuinely walks the list (live progress indication).
			gEeprom.MrChannel[1]     = gVfoSplit.current_channel;
			gEeprom.ScreenChannel[1] = gVfoSplit.current_channel;
			RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
			break;
		}
		if (gVfoSplit.current_channel == start)
			break;  // wrapped around, no valid channels
	}

	gVfoSplit.scan_progress++;
}

void VFO_SPLIT_Process(void)
{
	// VUURWERK v1.3.3: TX owns the hardware. During transmit, do NOT fall into
	// the RETURN_TO_A teardown below -- its RADIO_SetupRegisters(true) zeroes the
	// PA and drops to FOREGROUND ~10-20ms after keying, and because PTT is
	// edge-triggered nothing re-keys, so the operator transmits dead air (no
	// carrier, no CTCSS tail, no compressor stop). Just reset and yield; the
	// TX path retunes from the TX VFO regardless, and RX/split resume cleanly
	// on unkey (PTT release -> RADIO_SetupRegisters restores RX on VFO A).
	// G043: MONITOR likewise; ACTION_Monitor has already retuned to the
	// operator VFO, and the restore would drop monitor with gMonitor latched.
	if (gCurrentFunction == FUNCTION_TRANSMIT ||
	    gCurrentFunction == FUNCTION_MONITOR) {
		split_state = SPLIT_STATE_IDLE;
		return;
	}

	// HARD GATE: Only run during idle (squelch closed, no TX, no scanning)
	// G042: a disarm mid-hop takes the same restore, so the chip never
	// stays parked on B's channel.
	if (gCurrentFunction != FUNCTION_FOREGROUND || gVfoSplit.mode == SPLIT_OFF) {
		if (split_state != SPLIT_STATE_IDLE) {
			// We were mid-hop -- abort and restore VFO A immediately
			split_state = SPLIT_STATE_RETURN_TO_A;
		} else {
			// G047: a re-arm starts with no channel marked as alerted
			if (gVfoSplit.mode == SPLIT_OFF)
				memset(alerted, 0, sizeof(alerted));
			return;
		}
	}

	// VUURWERK v1.3.3: yield to dual watch / crossband. Split owns VFO B and
	// its RETURN_TO_A assumes gRxVfo == gTxVfo == VFO A. Dual watch flips
	// RX_VFO and crossband sets RX_VFO = !TX_VFO, so with either armed split
	// re-tunes the wrong VFO and the two engines thrash the tuner (same
	// unguarded-boundary class as the PTT-during-hop bug). Both modes also
	// want VFO B, so split simply pauses hopping while they're on -- the SP
	// indicator still shows armed, and hopping resumes when they're turned
	// off. State reset to IDLE; the active mode owns retuning from here.
	// G033: also pause while VFO B is the active VFO, or the walk moves the
	// operator's own VFO and PTT keys up on the walked channel. G046: and
	// during any scan (each one retunes the chip itself).
	if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF ||
	    gEeprom.CROSS_BAND_RX_TX != CROSS_BAND_OFF ||
	    gEeprom.TX_VFO != 0 ||
	    gScanStateDir != SCAN_OFF ||
	    SCANNER_IsScanning()) {
		split_state = SPLIT_STATE_IDLE;
		return;
	}

	switch (split_state) {
		case SPLIT_STATE_IDLE:
			if (hop_countdown > 0) {
				hop_countdown--;
				return;
			}
			// v1.3.3: don't LAUNCH a hop while the operator is holding a
			// key (same yield as dual watch). A hop rewrites
			// ScreenChannel[1] and retunes mid-keyhold; with the active
			// VFO on B that flipped manual frequency stepping into
			// memory-channel stepping under the operator's thumb.
			// In-flight hops (states below) still complete their
			// RETURN_TO_A restore untouched.
			if (gKeyBeingHeld)
				return;
			split_state = SPLIT_STATE_HOP_TO_B;
			break;

		case SPLIT_STATE_HOP_TO_B:
			// G053: take the hop frequency from VFO B itself (AdvanceVfoB
			// loads each walked channel there), never the 0 Hz boot value.
			gVfoSplit.current_freq_10Hz = gEeprom.VfoInfo[1].pRX->Frequency;
			// Use stock function to tune to VFO B frequency
			BK4819_SetFrequency(gVfoSplit.current_freq_10Hz);
			// G048: measure through B's own band front end (VHF/UHF LNA)
			BK4819_PickRXFilterPathBasedOnFrequency(gVfoSplit.current_freq_10Hz);
			// Restart RX chain for new frequency using stock function
			BK4819_RX_TurnOn();

			split_state = SPLIT_STATE_SETTLE;
			break;

		case SPLIT_STATE_SETTLE:
			// Check abort condition AGAIN after settle tick
			if (gCurrentFunction != FUNCTION_FOREGROUND) {
				split_state = SPLIT_STATE_RETURN_TO_A;
				break;
			}
			// Wait one tick (10ms) for RSSI to settle
			split_state = SPLIT_STATE_READ_RSSI;
			break;

		case SPLIT_STATE_READ_RSSI:
		{
			uint16_t rssi_raw = BK4819_ReadRegister(BK4819_REG_67);
			int16_t rssi_dBm = (int16_t)(rssi_raw >> 1) - 160;
			// G047: alert once per quiet-to-active transition; the bit
			// clears only 6 dB below the threshold (hysteresis).
			uint8_t *flags = &alerted[gVfoSplit.current_channel >> 3];
			const uint8_t bit = 1u << (gVfoSplit.current_channel & 7);

			if (rssi_dBm < RSSI_THRESHOLD_DBM - 6) {
				*flags &= ~bit;
			} else if (rssi_dBm > RSSI_THRESHOLD_DBM && !(*flags & bit)) {
				*flags |= bit;
				// Activity detected on VFO B
				gVfoSplit.alert.freq_10Hz = gVfoSplit.current_freq_10Hz;
				gVfoSplit.alert.rssi_dBm = rssi_dBm;
				gVfoSplit.alert.active = true;
				gVfoSplit.alert.alert_timer_ms = ALERT_DISPLAY_TIME_MS;

				// === VUURWERK v1.3.1 (U2): surface the alert ===
				// Until now nothing anywhere rendered gVfoSplit.alert; the
				// feature's entire output was invisible. Toast the frequency
				// and sound the 880Hz/200ms cue. The beep is deliberately in
				// the BEEP_CONTROL-exempt class (like the low-battery
				// warning): arming split IS opting into an audible alert,
				// and eyes-off monitoring is the feature's purpose.
				{
					char s[16];
					sprintf(s, "ACT %u.%05u",
						gVfoSplit.current_freq_10Hz / 100000,
						gVfoSplit.current_freq_10Hz % 100000);
					TOAST_Show(s);
					gUpdateDisplay = true;
					AUDIO_PlayBeep(BEEP_880HZ_200MS);
					gVfoSplit.alert.active = false;   // consumed
				}
			}

			// Advance to next VFO B frequency for next hop
			AdvanceVfoB();

			split_state = SPLIT_STATE_RETURN_TO_A;
			break;
		}

		case SPLIT_STATE_RETURN_TO_A:
			// CRITICAL: Full restore of VFO A via stock functions
			// This guarantees all registers are set correctly
			// G052: no EEPROM reload. A hop never writes VFO A's RAM, so
			// reprogramming from it is a full restore and keeps RAM-only
			// tuning (F+9, a held step) instead of snapping it back.
			RADIO_SetupRegisters(true);

			// Reset hop timer (convert ms to 10ms ticks)
			hop_countdown = gVfoSplit.hop_timer_ms / 10;
			split_state = SPLIT_STATE_IDLE;
			break;
	}
}
