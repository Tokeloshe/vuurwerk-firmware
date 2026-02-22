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

#include "vfo_split.h"
#include "bandscope.h"
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
	.alert_beep = true,
	.vfo_b_active = false,
	.hop_timer_ms = HOP_INTERVAL_NORMAL,
	.settle_timer_ms = 0,
	.current_channel = 0,
	.current_freq_10Hz = 0,
	.scan_progress = 0,
	.range_start_10Hz = 0,
	.range_end_10Hz = 0,
	.range_step_10Hz = 1250,
	.alert = {.active = false},
	.hits_this_session = 0,
	.last_hit_freq_10Hz = 0,
	.last_hit_time_s = 0,
	.saved_vfo_a_state = false
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

void VFO_SPLIT_Init(void)
{
	gVfoSplit.mode = SPLIT_OFF;
	gVfoSplit.vfo_b_active = false;
	gVfoSplit.hop_timer_ms = HOP_INTERVAL_NORMAL;
	gVfoSplit.settle_timer_ms = 0;
	gVfoSplit.current_channel = 0;
	gVfoSplit.current_freq_10Hz = 0;
	gVfoSplit.scan_progress = 0;
	gVfoSplit.alert.active = false;
	gVfoSplit.hits_this_session = 0;
	gVfoSplit.saved_vfo_a_state = false;
	split_state = SPLIT_STATE_IDLE;
	hop_countdown = 0;
}

void VFO_SPLIT_SetMode(SplitMode_t mode)
{
	gVfoSplit.mode = mode;
	if (mode != SPLIT_OFF)
		VFO_SPLIT_Reset();
	else {
		split_state = SPLIT_STATE_IDLE;
		hop_countdown = 0;
	}
}

void VFO_SPLIT_SetSource(BScanSource_t source)
{
	gVfoSplit.source = source;
	VFO_SPLIT_Reset();
}

void VFO_SPLIT_SetSpeed(BScanSpeed_t speed)
{
	gVfoSplit.speed = speed;
	switch (speed) {
		case B_SPEED_SLOW:   gVfoSplit.hop_timer_ms = HOP_INTERVAL_SLOW; break;
		case B_SPEED_NORMAL: gVfoSplit.hop_timer_ms = HOP_INTERVAL_NORMAL; break;
		case B_SPEED_FAST:   gVfoSplit.hop_timer_ms = HOP_INTERVAL_FAST; break;
	}
}

void VFO_SPLIT_SetAlertBeep(bool enabled)
{
	gVfoSplit.alert_beep = enabled;
}

void VFO_SPLIT_SetRange(uint32_t start_10Hz, uint32_t end_10Hz, uint32_t step_10Hz)
{
	gVfoSplit.range_start_10Hz = start_10Hz;
	gVfoSplit.range_end_10Hz = end_10Hz;
	gVfoSplit.range_step_10Hz = step_10Hz;
}

void VFO_SPLIT_Process(void)
{
	// HARD GATE: Only run during idle (squelch closed, no TX, no scanning)
	if (gCurrentFunction != FUNCTION_FOREGROUND) {
		if (split_state != SPLIT_STATE_IDLE) {
			// We were mid-hop — abort and restore VFO A immediately
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

			gVfoSplit.vfo_b_active = true;
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

			// Feed bandscope Option B (spectrum from hops)
			BANDSCOPE_RecordHop(gVfoSplit.current_freq_10Hz, (uint8_t)(rssi_raw >> 1));

			if (rssi_dBm > RSSI_THRESHOLD_DBM) {
				// Activity detected on VFO B
				gVfoSplit.hits_this_session++;
				gVfoSplit.last_hit_freq_10Hz = gVfoSplit.current_freq_10Hz;
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

			gVfoSplit.vfo_b_active = false;

			// Reset hop timer (convert ms to 10ms ticks)
			hop_countdown = gVfoSplit.hop_timer_ms / 10;
			split_state = SPLIT_STATE_IDLE;
			break;
	}
}

void VFO_SPLIT_GetStatus(uint8_t *channel, uint32_t *freq, uint8_t *progress)
{
	if (channel) *channel = gVfoSplit.current_channel;
	if (freq) *freq = gVfoSplit.current_freq_10Hz;
	if (progress) {
		if (gVfoSplit.source == B_SCAN_FREQ_RANGE &&
		    gVfoSplit.range_end_10Hz > gVfoSplit.range_start_10Hz)
		{
			uint32_t range = gVfoSplit.range_end_10Hz - gVfoSplit.range_start_10Hz;
			uint32_t pos = gVfoSplit.current_freq_10Hz - gVfoSplit.range_start_10Hz;
			*progress = (uint8_t)((pos * 100) / range);
		} else {
			*progress = 0;
		}
	}
}

BScanAlert_t* VFO_SPLIT_GetAlert(void)
{
	return &gVfoSplit.alert;
}

void VFO_SPLIT_ClearAlert(void)
{
	gVfoSplit.alert.active = false;
	gVfoSplit.alert.alert_timer_ms = 0;
}

void VFO_SPLIT_SwitchToB(void)
{
	VFO_SPLIT_ClearAlert();
}

void VFO_SPLIT_Reset(void)
{
	gVfoSplit.current_channel = 0;
	gVfoSplit.current_freq_10Hz = gVfoSplit.range_start_10Hz;
	gVfoSplit.scan_progress = 0;
	gVfoSplit.alert.active = false;
	split_state = SPLIT_STATE_IDLE;
	hop_countdown = 0;
}

uint16_t VFO_SPLIT_GetHitCount(void)
{
	return gVfoSplit.hits_this_session;
}

uint32_t VFO_SPLIT_GetLastHitFreq(void)
{
	return gVfoSplit.last_hit_freq_10Hz;
}

uint16_t VFO_SPLIT_GetLastHitTime(void)
{
	return gVfoSplit.last_hit_time_s;
}