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

#include "smart_squelch.h"
#include "app/chFrScanner.h"   // v1.3.1 (T1): gScanStateDir scan gate
#include "app/scanner.h"       // v1.3.1 (T1): SCANNER_IsScanning gate
#include "driver/bk4819.h"
#include "functions.h"
#include "misc.h"              // v1.3.1 (T1): gCssBackgroundScan gate
#include "radio.h"
#include "settings.h"
#include "rssi_histogram.h"
#include "vfo_split.h"         // v1.3.1 (T1): split-armed gate

SmartSquelchState_t gSmartSquelch = {
	.noise_smooth  = 127,
	.glitch_smooth = 255,
	.rssi_smooth   = 0,
	.noise_floor   = 100,
	.voice_prob    = 0,
};

// Integer EWMA: alpha = 1/8, no division, no floats
// tau = 1/(1-alpha) * tick = 8 * 10ms = 80ms
// G049: +4 rounds to nearest; the bare shift floored and settled up to 7 low.
static inline uint16_t ewma_update(uint16_t old_val, uint16_t new_sample)
{
	return old_val + ((((int16_t)(new_sample - old_val)) + 4) >> 3);
}

// v1.3.1 (T1): single REG_78 baseline writer, shared by the F7 RX-end
// restore and the parked pre-bias restore below.
static void write_baseline_reg78(void)
{
	BK4819_WriteRegister(BK4819_REG_78,
		((uint16_t)gRxVfo->SquelchOpenRSSIThresh << 8) |
		 (uint16_t)gRxVfo->SquelchCloseRSSIThresh);
}

// v1.3.1 (T1): shared smoothing reset (freq change, RX or parked).
static void reset_smoothing(void)
{
	gSmartSquelch.rssi_smooth   = 0;
	gSmartSquelch.noise_smooth  = 127;
	gSmartSquelch.glitch_smooth = 255;
	gSmartSquelch.voice_prob    = 0;
}

// Core voice probability scoring -- integer only
static uint8_t compute_voice_probability(
	uint16_t rssi, uint16_t noise, uint16_t glitch, int16_t noise_floor)
{
	int16_t score = 0;

	// Noise indicator (strongest voice predictor)
	// Low noise after demod = clean audio = voice
	// G114: REG_65 is 7-bit, so noise stays in 0..127
	if      (noise < 30)  score += 35;
	else if (noise < 60)  score += 25;
	else if (noise < 100) score += 10;

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

	if (score < 0)   score = 0;
	if (score > 100) score = 100;

	return (uint8_t)score;
}

void SMART_SQUELCH_Update(void)
{
	// Function-local state: hoisted so the freq-change guard
	// below can reset all of it in one block.
	static uint8_t  voice_hold     = 0;
	static int8_t   prev_adj       = 127;
	static uint32_t last_frequency = 0;
	static bool     was_rx         = false;
	static bool     parked_bias    = false;  // v1.3.1 (T1)
	static uint8_t  parked_div     = 0;      // v1.3.1 (T1) 50ms decimator

	// Only run the in-RX path during active RX
	if (gCurrentFunction != FUNCTION_RECEIVE &&
	    gCurrentFunction != FUNCTION_INCOMING &&
	    gCurrentFunction != FUNCTION_MONITOR) {
		// VUURWERK v1.3.0 (F7): falling-edge restore. A loosened (-3/-6)
		// REG_78 written during voice RX used to persist while parked,
		// leaving the squelch ~3dB easier to open on noise until the next
		// channel change rewrote the baseline. Restore the channel's
		// baseline thresholds once when RX ends, and re-arm prev_adj so
		// the next RX session starts from a known state.
		if (was_rx) {
			was_rx = false;
			if (prev_adj < 0)
				write_baseline_reg78();
			prev_adj = 127;
		}

		// === VUURWERK v1.3.1 (T1): parked voice pre-bias ===
		// The chip computes RSSI/noise/glitch continuously, but this module
		// historically only ran after the hardware squelch had already
		// opened -- it could keep a weak voice open, never help it open.
		// While parked, evaluate at 50ms cadence and bias the REG_78 open/
		// close thresholds down by at most 2 steps (~1dB) when the voice
		// score is high, restoring baseline the moment confidence drops.
		//
		// HARD GATES (MR scan stop fence): the pre-bias never runs during
		// any scan (gScanStateDir, SCANNER_IsScanning, gCssBackgroundScan),
		// while VFO Split is armed (split owns retuning), or outside
		// FUNCTION_FOREGROUND (excludes TX and power-save sleep). Spectrum
		// is structurally excluded: APP_RunSpectrum blocks the main loop,
		// so this tick never executes there.
		if (gCurrentFunction != FUNCTION_FOREGROUND ||
		    gScanStateDir    != SCAN_OFF            ||
		    SCANNER_IsScanning()                    ||
		    gCssBackgroundScan                      ||
		    gVfoSplit.mode   != SPLIT_OFF) {
			if (parked_bias) {
				parked_bias = false;
				write_baseline_reg78();
			}
			return;
		}

		if (++parked_div < 5)
			return;
		parked_div = 0;

		const uint8_t  pvfo  = gEeprom.RX_VFO;
		const uint32_t pfreq = gEeprom.VfoInfo[pvfo].pRX->Frequency;

		// Channel change while parked: drop stale smoothing and any bias
		// flag without a write -- the channel switch itself has already
		// rewritten the REG_78 baseline via RADIO_SetupRegisters.
		if (pfreq != last_frequency) {
			last_frequency = pfreq;
			parked_bias = false;
			reset_smoothing();
			return;
		}

		// G027: the histogram is fed here, while parked, so its mode is the
		// idle noise floor rather than the level of the stations heard.
		const uint16_t prssi = BK4819_GetRSSI();
		RSSI_HISTOGRAM_Update(pvfo, (int16_t)(prssi >> 1) - 160);

		// No adaptive floor yet: stay strictly at baseline.
		if (!gRSSI_Histogram[pvfo].valid) {
			if (parked_bias) {
				parked_bias = false;
				write_baseline_reg78();
			}
			return;
		}

		gSmartSquelch.rssi_smooth   = ewma_update(gSmartSquelch.rssi_smooth,   prssi);
		gSmartSquelch.noise_smooth  = ewma_update(gSmartSquelch.noise_smooth,  BK4819_GetExNoiceIndicator());
		gSmartSquelch.glitch_smooth = ewma_update(gSmartSquelch.glitch_smooth, BK4819_GetGlitchIndicator());
		gSmartSquelch.noise_floor   = (int16_t)(gRSSI_Histogram[pvfo].noise_floor_dbm + 160) * 2;

		gSmartSquelch.voice_prob = compute_voice_probability(
			gSmartSquelch.rssi_smooth,
			gSmartSquelch.noise_smooth,
			gSmartSquelch.glitch_smooth,
			gSmartSquelch.noise_floor);

		// Hysteresis band 40..59 holds the current state.
		if (gSmartSquelch.voice_prob >= 60 && !parked_bias) {
			parked_bias = true;
			int16_t open  = (int16_t)gRxVfo->SquelchOpenRSSIThresh  - 2;
			int16_t close = (int16_t)gRxVfo->SquelchCloseRSSIThresh - 2;
			if (open  < 0) open  = 0;
			if (close < 0) close = 0;
			BK4819_WriteRegister(BK4819_REG_78,
				((uint16_t)open << 8) | (uint16_t)close);
		}
		else if (gSmartSquelch.voice_prob < 40 && parked_bias) {
			parked_bias = false;
			write_baseline_reg78();
		}
		return;
	}
	was_rx = true;
	// v1.3.1 (T1): RX hand-off. The in-RX path rewrites REG_78 on its first
	// tick (prev_adj sentinel), so the parked flag is dropped without a
	// redundant write here.
	parked_bias = false;

	const uint8_t  vfo  = gEeprom.RX_VFO;
	const uint32_t freq = gEeprom.VfoInfo[vfo].pRX->Frequency;

	// Discard prior-channel state on frequency change so squelch
	// decisions for the new channel are not biased by ~200ms of
	// hangover or ~80ms of EWMA bleed from a different signal source.
	// Pattern matches gain_staging.c:64-72 and signal_quality.c.
	if (freq != last_frequency) {
		last_frequency = freq;
		voice_hold = 0;
		prev_adj   = 127;
		reset_smoothing();
	}

	// Read all three BK4819 indicators
	const uint16_t rssi_raw   = BK4819_GetRSSI();
	const uint16_t noise_raw  = BK4819_GetExNoiceIndicator();
	const uint16_t glitch_raw = BK4819_GetGlitchIndicator();

	// Smooth with EWMA (alpha=1/8, tau=80ms)
	gSmartSquelch.rssi_smooth   = ewma_update(gSmartSquelch.rssi_smooth, rssi_raw);
	gSmartSquelch.noise_smooth  = ewma_update(gSmartSquelch.noise_smooth, noise_raw);
	gSmartSquelch.glitch_smooth = ewma_update(gSmartSquelch.glitch_smooth, glitch_raw);

	// Get adaptive noise floor from histogram module
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
	if (gSmartSquelch.voice_prob >= 50)
		voice_hold = 20;           // 200ms at 10ms tick
	else if (voice_hold > 0) {
		voice_hold--;
		gSmartSquelch.voice_prob = 50;  // hold at decision threshold
	}

	// Dynamic squelch adjustment: only write REG_78 when value changes
	const int8_t adj = SMART_SQUELCH_GetAdjustment();
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
	// v1.3.1 (T1): the old +2/+4 tighten returns for low probabilities were
	// dead in effect -- the only caller clamps positive adjustments to 0
	// (never tighten during RX, by design). Return the truth directly.
	return 0;
}