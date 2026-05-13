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

#include "vfo_split.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"

static SplitState_t split_state = SPLIT_STATE_IDLE;
static uint16_t hop_countdown = 0;

VfoSplit_t gVfoSplit = {
	.mode = SPLIT_OFF,
	.source = B_SCAN_MEMORIES,
	.speed = B_SPEED_NORMAL,
	.hop_timer_ms = HOP_INTERVAL_NORMAL,
	.current_channel = 0,
	.current_freq_10Hz = 0,
	.scan_progress = 0,
	.range_start_10Hz = 0,
	.range_end_10Hz = 0,
	.range_step_10Hz = 1250,
	.alert = {.active = false}
};

// Advance to next VFO B frequency/channel
static void AdvanceVfoB(void)
{
	switch (gVfoSplit.source) {
		case B_SCAN_FREQ_RANGE:
			gVfoSplit.current_freq_10Hz += gVfoSplit.range_step_10Hz;
			if (gVfoSplit.current_freq_10Hz > gVfoSplit.range_end_10Hz)
				gVfoSplit.current_freq_10Hz = gVfoSplit.range_start_10Hz;
			break;

		case B_SCAN_MEMORIES:
		case B_SCAN_LIST_1:
		case B_SCAN_LIST_2:
		{
			// Find next valid memory channel
			uint8_t start = gVfoSplit.current_channel;
			uint8_t scanList = (gVfoSplit.source == B_SCAN_LIST_1) ? 1 :
			                   (gVfoSplit.source == B_SCAN_LIST_2) ? 2 : 0;
			bool checkScanList = (gVfoSplit.source != B_SCAN_MEMORIES);

			for (uint8_t i = 0; i < 200; i++) {
				if (++gVfoSplit.current_channel >= 200)
					gVfoSplit.current_channel = 0;
				if (RADIO_CheckValidChannel(gVfoSplit.current_channel, checkScanList, scanList)) {
					// Read channel frequency from EEPROM
					VFO_Info_t tempInfo;
					RADIO_InitInfo(&tempInfo, gVfoSplit.current_channel, 0);
					RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
					gVfoSplit.current_freq_10Hz = gEeprom.VfoInfo[1].pRX->Frequency;
					break;
				}
				if (gVfoSplit.current_channel == start)
					break;  // wrapped around, no valid channels
			}
			break;
		}
	}

	gVfoSplit.scan_progress++;
}

void VFO_SPLIT_Process(void)
{
	// HARD GATE: Only run during idle (squelch closed, no TX, no scanning)
	if (gCurrentFunction != FUNCTION_FOREGROUND) {
		if (split_state != SPLIT_STATE_IDLE) {
			// We were mid-hop -- abort and restore VFO A immediately
			split_state = SPLIT_STATE_RETURN_TO_A;
		} else {
			return;
		}
	}

	if (gVfoSplit.mode == SPLIT_OFF)
		return;

	// Don't hop if we have no frequency to hop to
	if (gVfoSplit.current_freq_10Hz == 0 &&
	    gVfoSplit.source == B_SCAN_FREQ_RANGE)
		return;

	switch (split_state) {
		case SPLIT_STATE_IDLE:
			if (hop_countdown > 0) {
				hop_countdown--;
				return;
			}
			split_state = SPLIT_STATE_HOP_TO_B;
			break;

		case SPLIT_STATE_HOP_TO_B:
			// Use stock function to tune to VFO B frequency
			BK4819_SetFrequency(gVfoSplit.current_freq_10Hz);
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

			if (rssi_dBm > RSSI_THRESHOLD_DBM) {
				// Activity detected on VFO B
				gVfoSplit.alert.freq_10Hz = gVfoSplit.current_freq_10Hz;
				gVfoSplit.alert.rssi_dBm = rssi_dBm;
				gVfoSplit.alert.active = true;
				gVfoSplit.alert.alert_timer_ms = ALERT_DISPLAY_TIME_MS;
			}

			// Advance to next VFO B frequency for next hop
			AdvanceVfoB();

			split_state = SPLIT_STATE_RETURN_TO_A;
			break;
		}

		case SPLIT_STATE_RETURN_TO_A:
			// CRITICAL: Full restore of VFO A via stock functions
			// This guarantees all registers are set correctly
			RADIO_ConfigureChannel(gEeprom.TX_VFO, VFO_CONFIGURE_RELOAD);
			RADIO_SetupRegisters(true);

			// Reset hop timer (convert ms to 10ms ticks)
			hop_countdown = gVfoSplit.hop_timer_ms / 10;
			split_state = SPLIT_STATE_IDLE;
			break;
	}
}
