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

#include "smart_squelch.h"
#include "driver/bk4819.h"
#include "functions.h"
#include "radio.h"
#include "settings.h"
#include "rssi_histogram.h"

SmartSquelchState_t gSmartSquelch = {
	.noise_smooth  = 127,
	.glitch_smooth = 255,
	.rssi_smooth   = 0,
	.noise_floor   = 100,
	.voice_prob    = 0,
};

// Integer EWMA: alpha = 1/8, no division, no floats
// tau = 1/(1-alpha) * tick = 8 * 10ms = 80ms
static inline uint16_t ewma_update(uint16_t old_val, uint16_t new_sample)
{
	return old_val + (((int16_t)(new_sample - old_val)) >> 3);
}

// Core voice probability scoring — integer only
static uint8_t compute_voice_probability(
	uint16_t rssi, uint16_t noise, uint16_t glitch, int16_t noise_floor)
{
	int16_t score = 0;

	// Noise indicator (strongest voice predictor)
	// Low noise after demod = clean audio = voice
	if      (noise < 30)  score += 35;
	else if (noise < 60)  score += 25;
	else if (noise < 100) score += 10;
	else if (noise > 200) score -= 15;

	// Glitch indicator (interference rejector)
	// Low glitch = stable signal, not transient interference
	if      (glitch < 15)  score += 20;
	else if (glitch < 40)  score += 10;
	else if (glitch > 150) score -= 20;
	else if (glitch > 80)  score -= 5;

	// RSSI vs noise floor (SNR estimate)
	int16_t snr = (int16_t)rssi - noise_floor;
	if      (snr > 25) score += 25;
	else if (snr > 15) score += 15;
	else if (snr > 8)  score += 5;
	else               score -= 10;

	// Cross-correlation: noise vs glitch
	// Voice: low noise AND low-moderate glitch (not dead-still)
	if (noise < 60 && glitch < 40 && glitch > 5)
		score += 10;
	// Double-bad: definitely not voice
	if (noise > 150 && glitch > 100)
		score -= 10;

	if (score < 0)   score = 0;
	if (score > 100) score = 100;

	return (uint8_t)score;
}

void SMART_SQUELCH_Update(void)
{
	// Only run during active RX
	if (gCurrentFunction != FUNCTION_RECEIVE &&
	    gCurrentFunction != FUNCTION_INCOMING &&
	    gCurrentFunction != FUNCTION_MONITOR)
		return;

	// Read all three BK4819 indicators
	const uint16_t rssi_raw   = BK4819_GetRSSI();
	const uint16_t noise_raw  = BK4819_GetExNoiceIndicator();
	const uint16_t glitch_raw = BK4819_GetGlitchIndicator();

	// Smooth with EWMA (alpha=1/8, tau=80ms)
	gSmartSquelch.rssi_smooth   = ewma_update(gSmartSquelch.rssi_smooth, rssi_raw);
	gSmartSquelch.noise_smooth  = ewma_update(gSmartSquelch.noise_smooth, noise_raw);
	gSmartSquelch.glitch_smooth = ewma_update(gSmartSquelch.glitch_smooth, glitch_raw);

	// Get adaptive noise floor from histogram module
	const uint8_t vfo = gEeprom.RX_VFO;
	if (gRSSI_Histogram[vfo].valid)
		gSmartSquelch.noise_floor = (int16_t)(gRSSI_Histogram[vfo].noise_floor_dbm + 160) * 2;
	// Convert dBm back to raw RSSI units: raw = (dBm + 160) * 2

	// Compute voice probability
	gSmartSquelch.voice_prob = compute_voice_probability(
		gSmartSquelch.rssi_smooth,
		gSmartSquelch.noise_smooth,
		gSmartSquelch.glitch_smooth,
		gSmartSquelch.noise_floor);

	// Hangover: hold voice_prob >= 50 for 200ms after last voice
	static uint8_t voice_hold = 0;
	if (gSmartSquelch.voice_prob >= 50)
		voice_hold = 20;           // 200ms at 10ms tick
	else if (voice_hold > 0) {
		voice_hold--;
		gSmartSquelch.voice_prob = 50;  // hold at decision threshold
	}

	// Dynamic squelch adjustment: only write REG_78 when value changes
	const int8_t adj = SMART_SQUELCH_GetAdjustment();
	static int8_t prev_adj = 127;
	if (adj != prev_adj) {
		prev_adj = adj;
		// Only LOOSEN squelch (negative adj) dynamically.
		// Never TIGHTEN (positive adj) to avoid closing during pauses.
		const int8_t safe_adj = (adj < 0) ? adj : 0;
		int16_t open  = (int16_t)gRxVfo->SquelchOpenRSSIThresh  + safe_adj;
		int16_t close = (int16_t)gRxVfo->SquelchCloseRSSIThresh + safe_adj;
		if (open < 0)    open = 0;
		if (open > 255)  open = 255;
		if (close < 0)   close = 0;
		if (close > 255) close = 255;
		BK4819_WriteRegister(BK4819_REG_78,
			((uint16_t)open << 8) | (uint16_t)close);
	}
}

int8_t SMART_SQUELCH_GetAdjustment(void)
{
	const uint8_t vp = gSmartSquelch.voice_prob;

	if (vp >= 70) return -6;  // High confidence voice: open 6 steps easier
	if (vp >= 50) return -3;  // Probable voice
	if (vp >= 30) return  0;  // Uncertain: use default
	if (vp >= 15) return  2;  // Probably not voice
	return 4;                 // Almost certainly noise: harder to open
}