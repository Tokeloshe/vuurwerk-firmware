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

#include "bandscope.h"
#include "driver/bk4819.h"
#include "functions.h"
#include "rssi_histogram.h"
#include "settings.h"
#include <string.h>

static uint8_t current_data[128];
static uint8_t peak_data[128];
static uint8_t noise_floor_level = 0;
static uint8_t tick_divider = 0;
static uint8_t decay_counter = 0;
static bool    bandscope_enabled = false;

void BANDSCOPE_SetEnabled(bool enabled) {
	bandscope_enabled = enabled;
	if (!enabled) {
		memset(current_data, 0, sizeof(current_data));
		memset(peak_data, 0, sizeof(peak_data));
	}
}

bool BANDSCOPE_IsEnabled(void) {
	return bandscope_enabled;
}

void BANDSCOPE_Process(void) {
	if (!bandscope_enabled) return;
	if (gCurrentFunction == FUNCTION_TRANSMIT) return;

	if (++tick_divider < 10) return;  // ~100ms sample rate
	tick_divider = 0;

	uint16_t rssi = BK4819_ReadRegister(BK4819_REG_67);
	uint8_t level = (rssi >> 1) & 0xFF;

	// Adopt live noise floor from rssi_histogram (per-VFO, valid after
	// ~100 samples). Render gates on nf_h > 0 so a pre-valid histogram
	// silently keeps the dotted line off.
	const uint8_t vfo = gEeprom.RX_VFO;
	if (gRSSI_Histogram[vfo].valid)
		noise_floor_level = (uint8_t)(gRSSI_Histogram[vfo].noise_floor_dbm + 160);

	// v1.2.7: scroll both parallel arrays left in one loop.
	for (uint8_t i = 0; i < 127; i++) {
		current_data[i] = current_data[i + 1];
		peak_data[i]    = peak_data[i + 1];
	}
	current_data[127] = level;
	if (level > peak_data[127])
		peak_data[127] = level;

	// Decay peaks every ~500ms
	if (++decay_counter >= 5) {
		decay_counter = 0;
		for (uint8_t i = 0; i < 128; i++) {
			if (peak_data[i] > current_data[i] && peak_data[i] > 0)
				peak_data[i]--;
		}
	}
}

void BANDSCOPE_Render(uint8_t *framebuffer_line) {
	memset(framebuffer_line, 0, 128);

	for (uint8_t i = 0; i < 128; i++) {
		// Current signal bar
		uint8_t sig_h = current_data[i] >> 5;
		if (sig_h > 7) sig_h = 7;
		uint8_t sig_mask = (1 << sig_h) - 1;

		// Peak hold dot
		uint8_t peak_h = peak_data[i] >> 5;
		if (peak_h > 7) peak_h = 7;
		uint8_t peak_mask = 0;
		if (peak_h > sig_h)
			peak_mask = 1 << peak_h;

		// Noise floor dotted line (every 4 pixels)
		uint8_t nf_mask = 0;
		uint8_t nf_h = noise_floor_level >> 5;
		if ((i & 3) == 0 && nf_h > 0 && nf_h <= 7)
			nf_mask = 1 << nf_h;

		framebuffer_line[i] = sig_mask | peak_mask | nf_mask;
	}
}