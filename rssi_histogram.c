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

#include "rssi_histogram.h"

RSSI_Histogram_t gRSSI_Histogram[2];

// v1.2.7: Init removed. The C runtime zero-clears BSS at boot
// (init.c BSS_Init), giving us the same state Reset(vfo) used to
// produce: every bin=0, total_samples=0, noise_floor_dbm=0,
// valid=false. The first Update call increments from BSS zero,
// behaviour-identical to the prior path.

static void Analyze(uint8_t vfo) {
    RSSI_Histogram_t *h = &gRSSI_Histogram[vfo];

    if (h->total_samples < 100) {
        h->valid = false;
        return;
    }

    // Find peak bin (mode of distribution = noise floor)
    uint16_t max_count = 0;
    uint8_t peak_bin = 0;

    for (uint8_t i = 0; i < RSSI_HIST_BINS; i++) {
        if (h->bins[i] > max_count) {
            max_count = h->bins[i];
            peak_bin = i;
        }
    }

    // Noise floor is center of peak bin.
    // VUURWERK v1.3.3: bin 0 computes -130 + 0 + 1 = -129, which overflows the
    // int8_t field to +127 -- and smart_squelch then reads a bogus noise floor
    // of (127+160)*2 = 574, pinning its SNR term negative and silently killing
    // the voice-open bias. Clamp to int8_t; a floor below -128 dBm is not
    // physically meaningful anyway.
    const int16_t nf = RSSI_HIST_MIN + (peak_bin * RSSI_HIST_BIN_WIDTH) + (RSSI_HIST_BIN_WIDTH / 2);
    h->noise_floor_dbm = (nf < -128) ? -128 : (int8_t)nf;
    h->valid = true;
}

void RSSI_HISTOGRAM_Update(uint8_t vfo, int16_t rssi_dbm) {
    if (vfo > 1) return;

    RSSI_Histogram_t *h = &gRSSI_Histogram[vfo];

    // Bin index without a divide (LAW 4: Cortex-M0 has no divider). Clamp
    // to the table's span first, then x/3 == (x*171)>>9, exact for x < 512.
    // The clamped x tops out at 95, which lands in the last bin directly.
    int16_t x = rssi_dbm - RSSI_HIST_MIN;
    if (x < 0) x = 0;
    if (x > RSSI_HIST_BINS * RSSI_HIST_BIN_WIDTH - 1) x = RSSI_HIST_BINS * RSSI_HIST_BIN_WIDTH - 1;
    const uint8_t bin_idx = (uint8_t)(((uint16_t)x * 171u) >> 9);

    h->bins[bin_idx]++;

    // v1.5.1: exponential forgetting. total_samples used to climb to 65535
    // and wrap to 0, which flagged the histogram invalid for the next 100
    // samples (about once every 11 minutes of continuous RX the smart squelch
    // lost its noise floor and the parked pre-bias let go for a second), and
    // the bins saturated into a lifetime noise floor that never adapted to a
    // new site. Halving at 0x8000 keeps every count in range forever and
    // weights the floor toward recent conditions. No bin can now reach
    // 0xFFFF, so the old per-bin saturation test is gone with it.
    if (++h->total_samples >= 0x8000) {
        for (uint8_t i = 0; i < RSSI_HIST_BINS; i++)
            h->bins[i] >>= 1;
        h->total_samples >>= 1;
    }

    // Auto-analyze every 256 samples
    if ((h->total_samples & 0xFF) == 0) {
        Analyze(vfo);
    }
}
