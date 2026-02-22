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

#include "tx_compressor.h"
#include "driver/bk4819.h"
#include "radio.h"

// Configuration (user-adjustable via menu in future)
CompressorConfig_t gCompressorConfig = {
	.enabled     = true,
	.threshold   = 18,    // 0-31 mic level where compression starts
	.ratio_x10   = 30,    // 30 = 3:1 compression ratio
	.attack_ms   = 5,     // Fast attack catches peaks
	.release_ms  = 300,   // Slow release avoids pumping
	.makeup_gain = 3      // Post-compression boost
};

static uint16_t original_reg_7d;
static uint8_t  base_gain;
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

void TX_COMPRESSOR_Init(void) {
	compressor_active = false;
}

void TX_COMPRESSOR_Start(void) {
	if (!gCompressorConfig.enabled) return;

	original_reg_7d = BK4819_ReadRegister(BK4819_REG_7D);
	base_gain = original_reg_7d & MIC_GAIN_MASK;

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

	uint16_t new_7d = (original_reg_7d & 0xFFE0) | ((uint16_t)final_gain & MIC_GAIN_MASK);
	BK4819_WriteRegister(BK4819_REG_7D, new_7d);
}

void TX_COMPRESSOR_Stop(void) {
	if (!compressor_active) return;
	BK4819_WriteRegister(BK4819_REG_7D, original_reg_7d);
	compressor_active = false;
}

uint8_t TX_COMPRESSOR_GetGainReduction(void) {
	// For UI display: current gain reduction in steps
	if (!compressor_active) return 0;
	uint16_t env_actual = (uint16_t)(envelope >> ENVELOPE_SHIFT);
	if (env_actual <= gCompressorConfig.threshold) return 0;
	uint16_t excess = env_actual - gCompressorConfig.threshold;
	uint16_t red = (excess * (gCompressorConfig.ratio_x10 - 10)) / gCompressorConfig.ratio_x10;
	uint8_t gr = (uint8_t)(red >> 1);
	return (gr > 15) ? 15 : gr;
}