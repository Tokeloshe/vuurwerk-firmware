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

#include "spectrum_enh.h"
#include <string.h>

SpectrumEnh_t gSpectrumEnh;

static const char *mode_names[] = {
    "NORMAL",
    "PEAK",
    "DIFF",
    "WATER"
};

void SPECTRUM_ENH_Init(void) {
    memset(&gSpectrumEnh, 0, sizeof(SpectrumEnh_t));
    gSpectrumEnh.mode = SPEC_MODE_NORMAL;
    gSpectrumEnh.peak_decay_rate = 2;

    // Initialize noise floor to minimum
    for (uint8_t i = 0; i < SPECTRUM_WIDTH; i++) {
        gSpectrumEnh.noise_floor[i] = -130;
    }
}

void SPECTRUM_ENH_ProcessSample(uint8_t idx, uint16_t rssi) {
    if (idx >= SPECTRUM_WIDTH) return;

    // Store previous sweep for differential mode
    gSpectrumEnh.prev_sweep[idx] = rssi;

    // Update peak hold
    if (rssi > gSpectrumEnh.peak_hold[idx]) {
        gSpectrumEnh.peak_hold[idx] = rssi;
    }

    // Update noise floor (slow moving average of minimum)
    int16_t rssi_signed = (int16_t)rssi - 32768;
    if (rssi_signed < gSpectrumEnh.noise_floor[idx] || gSpectrumEnh.noise_floor[idx] < -120) {
        gSpectrumEnh.noise_floor[idx] = rssi_signed;
    } else {
        // Slow rise to track changing noise floor
        gSpectrumEnh.noise_floor[idx] = (gSpectrumEnh.noise_floor[idx] * 15 + rssi_signed) / 16;
    }
}

void SPECTRUM_ENH_UpdateNoiseFloor(void) {
    // Already updated in ProcessSample
}

void SPECTRUM_ENH_UpdatePeakHold(void) {
    // Decay peak hold values
    for (uint8_t i = 0; i < SPECTRUM_WIDTH; i++) {
        if (gSpectrumEnh.peak_hold[i] > gSpectrumEnh.peak_decay_rate) {
            gSpectrumEnh.peak_hold[i] -= gSpectrumEnh.peak_decay_rate;
        } else {
            gSpectrumEnh.peak_hold[i] = 0;
        }
    }
}

void SPECTRUM_ENH_UpdateWaterfall(void) {
    // Scroll waterfall up by copying rows
    if (gSpectrumEnh.waterfall_row < WATERFALL_HEIGHT - 1) {
        gSpectrumEnh.waterfall_row++;
    } else {
        // Wrap to top
        gSpectrumEnh.waterfall_row = 0;
    }

    // Add new row at bottom (packed bits)
    // For now, just clear it - actual data would come from spectrum scan
    uint8_t row_byte_start = (gSpectrumEnh.waterfall_row * SPECTRUM_WIDTH) / 8;
    uint8_t row_byte_end = row_byte_start + (SPECTRUM_WIDTH / 8);

    for (uint8_t i = row_byte_start; i < row_byte_end && i < WATERFALL_BYTES; i++) {
        gSpectrumEnh.waterfall[i] = 0;
    }
}

uint16_t SPECTRUM_ENH_GetDisplay(uint8_t idx) {
    if (idx >= SPECTRUM_WIDTH) return 0;

    uint16_t value = gSpectrumEnh.prev_sweep[idx];

    switch (gSpectrumEnh.mode) {
        case SPEC_MODE_NORMAL:
            // Subtract noise floor
            if (gSpectrumEnh.noise_floor[idx] > -120) {
                int32_t corrected = (int32_t)value - (int32_t)gSpectrumEnh.noise_floor[idx];
                if (corrected < 0) corrected = 0;
                if (corrected > 0xFFFF) corrected = 0xFFFF;
                value = (uint16_t)corrected;
            }
            break;

        case SPEC_MODE_PEAK_HOLD:
            value = gSpectrumEnh.peak_hold[idx];
            break;

        case SPEC_MODE_DIFFERENTIAL:
            // MTI (Moving Target Indicator) - XOR current with previous
            // Shows only CHANGES in spectrum (removes static signals)
            {
                uint16_t current = gSpectrumEnh.prev_sweep[idx];
                uint16_t previous = (idx > 0) ? gSpectrumEnh.prev_sweep[idx - 1] : 0;

                // XOR at bit level to detect changes
                uint16_t diff = current ^ previous;

                // Amplify small changes for visibility
                value = (diff > 0) ? (diff << 2) : 0;
            }
            break;

        case SPEC_MODE_WATERFALL:
            // Return waterfall data
            break;

        default:
            break;
    }

    return value;
}

void SPECTRUM_ENH_CycleMode(void) {
    gSpectrumEnh.mode = (gSpectrumEnh.mode + 1) % SPEC_MODE_COUNT;
}

const char* SPECTRUM_ENH_GetModeName(void) {
    if (gSpectrumEnh.mode < SPEC_MODE_COUNT) {
        return mode_names[gSpectrumEnh.mode];
    }
    return "???";
}