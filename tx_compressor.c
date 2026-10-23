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

#include "tx_compressor.h"
#include "driver/bk4819.h"
#include "radio.h"

#ifdef ENABLE_TX_COMP_DIAG
// v1.3.1 (T3): bench diagnostic. The v1.3.0 audit hypothesis -- threshold=18
// against an env_actual scale that reaches the hundreds for speech, so the
// compressor pegs at max reduction on any voice -- is structurally confirmed
// from the audio-bar scaling (ui/main.c voice_amp*8 against a 16-bit range).
// This readout captures the real numbers needed to recalibrate: env_actual
// at (a) silence, (b) normal speech 30cm, (c) loud speech 10cm.
#include <string.h>
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "ui/helper.h"
static uint16_t diag_env, diag_rms;
static uint8_t  diag_gain;
#endif

// Configuration. v1.5.1: const. Nothing ever writes it, and as a
// mutable global every division below (attack, release, ratio) was a
// libgcc call on every 10 ms TX tick; folded, they are shifts and constants.
static const CompressorConfig_t gCompressorConfig = {
	.enabled     = true,
	.threshold   = 18,    // 0-31 mic level where compression starts
	.ratio_x10   = 30,    // 30 = 3:1 compression ratio
	.attack_ms   = 5,     // Fast attack catches peaks
	.release_ms  = 300,   // Slow release avoids pumping
	.makeup_gain = 3      // Post-compression boost
};

static uint16_t original_reg_7d;
static uint8_t  base_gain;
static uint8_t  last_gain;
static bool     compressor_active = false;

// Envelope follower state
static uint32_t rms_accumulator = 0;
static uint8_t  rms_count = 0;
static uint16_t rms_level = 0;
static uint32_t envelope = 0;        // Fixed-point (<<8)

#define RMS_WINDOW      4            // 4 samples = 40ms at 10ms/tick
#define ENVELOPE_SHIFT  8
#define MIC_GAIN_MIN    4
#define MIC_GAIN_MAX    31
#define MIC_GAIN_MASK   0x1F
#define MIC_LEVEL_REG   0x64

// Integer square root (Newton's method)
static uint16_t isqrt32(uint32_t n) {
	if (n == 0) return 0;
	uint32_t x = n;
	uint32_t y = (x + 1) >> 1;
	while (y < x) { x = y; y = (x + n / x) >> 1; }
	return (uint16_t)(x > 0xFFFF ? 0xFFFF : x);
}

void TX_COMPRESSOR_Start(void) {
	if (!gCompressorConfig.enabled) return;

	// VUURWERK v1.3.3: FM only. On AM/USB the per-syllable REG_7D mic-gain
	// pumping reintroduces the modulation-depth pumping that the AM/USB TX
	// path deliberately kills via MIC-AGC-off (REG_19=0x9041, radio.c). The
	// compressor's envelope/deviation model is FM-oriented anyway. Leaving
	// compressor_active false makes Process a no-op and Stop harmless.
	if (gCurrentVfo->Modulation != MODULATION_FM) return;

	original_reg_7d = BK4819_ReadRegister(BK4819_REG_7D);
	base_gain = original_reg_7d & MIC_GAIN_MASK;
	last_gain = base_gain;

	rms_accumulator = 0;
	rms_count = 0;
	rms_level = 0;
	envelope = 0;
	compressor_active = true;
}

void TX_COMPRESSOR_Process(void) {
	if (!compressor_active || !gCompressorConfig.enabled) return;

	// Step 1: Read mic amplitude
	uint16_t mic_raw = BK4819_ReadRegister(MIC_LEVEL_REG) & 0x7FFF;

	// Step 2: RMS calculation (rolling window)
	uint16_t scaled = mic_raw >> 4;  // prevent uint32 overflow
	rms_accumulator += (uint32_t)scaled * scaled;
	rms_count++;

	if (rms_count >= RMS_WINDOW) {
		rms_level = isqrt32(rms_accumulator / RMS_WINDOW);
		rms_accumulator = 0;
		rms_count = 0;
	}

	// Step 3: Envelope follower (attack/release asymmetry)
	uint32_t rms_shifted = (uint32_t)rms_level << ENVELOPE_SHIFT;

	if (rms_shifted > envelope) {
		// ATTACK: fast
		uint8_t attack_ticks = gCompressorConfig.attack_ms / 10;
		if (attack_ticks < 1) attack_ticks = 1;
		uint32_t coeff = 256 / attack_ticks;
		envelope += ((rms_shifted - envelope) * coeff) >> 8;
	} else {
		// RELEASE: slow
		uint16_t release_ticks = gCompressorConfig.release_ms / 10;
		if (release_ticks < 1) release_ticks = 1;
		uint32_t coeff = 256 / release_ticks;
		envelope -= ((envelope - rms_shifted) * coeff) >> 8;
	}

	uint16_t env_actual = (uint16_t)(envelope >> ENVELOPE_SHIFT);

	// Step 4: Compute gain reduction
	uint8_t gain_reduction = 0;
	if (env_actual > gCompressorConfig.threshold) {
		uint16_t excess = env_actual - gCompressorConfig.threshold;
		// reduction = excess * (1 - 10/ratio)
		uint16_t red_x10 = (excess * (gCompressorConfig.ratio_x10 - 10)) / gCompressorConfig.ratio_x10;
		gain_reduction = (uint8_t)(red_x10 >> 1);  // scale to gain steps
		if (gain_reduction > 15) gain_reduction = 15;
	}

	// Step 5: Apply to REG_7D
	int16_t final_gain = (int16_t)base_gain - gain_reduction + gCompressorConfig.makeup_gain;
	if (final_gain < MIC_GAIN_MIN) final_gain = MIC_GAIN_MIN;
	if (final_gain > MIC_GAIN_MAX) final_gain = MIC_GAIN_MAX;

	if ((uint8_t)final_gain != last_gain) {
		uint16_t new_7d = (original_reg_7d & 0xFFE0) | ((uint16_t)final_gain & MIC_GAIN_MASK);
		BK4819_WriteRegister(BK4819_REG_7D, new_7d);
		last_gain = (uint8_t)final_gain;
	}

#ifdef ENABLE_TX_COMP_DIAG
	diag_env  = env_actual;
	diag_rms  = rms_level;
	diag_gain = (uint8_t)final_gain;
#endif
}

#ifdef ENABLE_TX_COMP_DIAG
// v1.3.1 (T3): render "E<env> R<rms> G<gain>" on line 3 during TX, same
// self-blit pattern as the stock audio bar (which also blits mid-TX).
void TX_COMPRESSOR_DiagDraw(void)
{
	char s[20];
	memset(gFrameBuffer[3], 0, sizeof(gFrameBuffer[3]));
	sprintf(s, "E%u R%u G%u", diag_env, diag_rms, diag_gain);
	UI_PrintStringSmallNormal(s, 0, 127, 3);
	ST7565_BlitFullScreen();
}
#endif

void TX_COMPRESSOR_Stop(void) {
	if (!compressor_active) return;
	BK4819_WriteRegister(BK4819_REG_7D, original_reg_7d);
	compressor_active = false;
}