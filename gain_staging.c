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

#include "gain_staging.h"
#include "am_fix.h"
#include "driver/bk4819.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "signal_classifier.h"

GainStaging_t gGainStaging[2];

// Frequency change detection (per-VFO, like AM_fix)
static uint32_t last_frequency[2] = {0, 0};

// Target RSSI in raw BK4819 units: (dBm + 160) * 2
// FM: -75 dBm target (FM limiter has headroom, run higher gain)
#define FM_TARGET_RSSI  ((int16_t)((-75 + 160) * 2))

void GAIN_STAGING_Init(void)
{
    for (uint8_t i = 0; i < 2; i++) {
        gGainStaging[i].table_index      = 0;  // Index 0 = stock gain (-7dB)
        gGainStaging[i].table_index_prev = 0;
        gGainStaging[i].prev_rssi        = 0;
        gGainStaging[i].hold_counter     = 0;
        gGainStaging[i].target_rssi_dBm  = -75;
        last_frequency[i] = 0;
    }
}

void GAIN_STAGING_Reset(uint8_t vfo)
{
    if (vfo > 1) return;
    gGainStaging[vfo].table_index      = 0;  // Back to stock gain
    gGainStaging[vfo].table_index_prev = 0;
    gGainStaging[vfo].prev_rssi        = 0;
    gGainStaging[vfo].hold_counter     = 0;
}

void GAIN_STAGING_10ms(uint8_t vfo)
{
    if (vfo > 1) return;

    // === GUARD: FM mode only — AM_fix owns REG_13 in AM/USB ===
    if (gRxVfo->Modulation != MODULATION_FM)
        return;

    // === GUARD: Only during FOREGROUND (idle) and RX — skip TX and power save ===
    if (gCurrentFunction != FUNCTION_FOREGROUND && !FUNCTION_IsRx())
        return;

    // === FREQUENCY CHANGE DETECTION (like AM_fix) ===
    const uint32_t freq = gEeprom.VfoInfo[vfo].pRX->Frequency;
    if (freq != last_frequency[vfo]) {
        last_frequency[vfo] = freq;
        GAIN_STAGING_Reset(vfo);
        // Write stock gain immediately so the new channel starts clean
        BK4819_WriteRegister(BK4819_REG_13, gain_table[0].reg_val);
        return;  // Let next tick start fresh
    }

    // === READ RSSI ===
    const int16_t new_rssi = (int16_t)BK4819_GetRSSI();
    GainStaging_t *gs = &gGainStaging[vfo];

    // Average with previous reading (spike immunity)
    int16_t rssi = (gs->prev_rssi > 0) ? (gs->prev_rssi + new_rssi) / 2 : new_rssi;
    gs->prev_rssi = new_rssi;

    // === UPDATE HOLD COUNTER ===
    if (gs->hold_counter > 0)
        gs->hold_counter--;

    // === COMPUTE ERROR ===
    // diff_dB: positive = signal too strong, negative = too weak
    int16_t diff_dB = (rssi - FM_TARGET_RSSI) / 2;

    if (diff_dB > 0) {
        // Signal too strong — DECREASE gain (fast attack)
        unsigned int index = gs->table_index;

        if (diff_dB >= 10) {
            // Large overshoot: jump aggressively
            const int16_t desired_gain_dB = (int16_t)gain_table[index].gain_dB - diff_dB + 8;
            while (index > 1)
                if (gain_table[--index].gain_dB <= desired_gain_dB)
                    break;
        } else {
            // Small overshoot: single step down
            if (index > 1)
                index--;
        }

        if (index < 1) index = 1;

        if (gs->table_index != index) {
            gs->table_index = index;
            // Adaptive hold time based on signal classification
            SignalClass_t sc = SIGNAL_CLASSIFIER_GetClass(vfo);
            if (sc == SIGNAL_CLASS_FAST)
                gs->hold_counter = 15;    // 150ms for FM/digital
            else if (sc == SIGNAL_CLASS_SLOW)
                gs->hold_counter = 50;    // 500ms for CW/carrier
            else
                gs->hold_counter = 30;    // 300ms default
        }
    }

    // 6dB hysteresis band: don't release hold until signal drops well below target
    if (diff_dB >= -6)
        gs->hold_counter = 30;

    if (gs->hold_counter == 0) {
        // Hold expired — free to INCREASE gain (slow release)
        // This is the critical path: when squelch is closed and no signal,
        // gain ramps UP one step at a time until either:
        //   (a) a signal is found (squelch opens, then we pull down if needed)
        //   (b) we reach max gain (table_index = gain_table_size-1)
        unsigned int index = gs->table_index + 1;
        if (index >= gain_table_size)
            index = gain_table_size - 1;
        gs->table_index = index;
    }

    // === APPLY TO REG_13 ===
    // Write only when the index has actually changed (minimize SPI bus traffic)
    if (gs->table_index != gs->table_index_prev) {
        BK4819_WriteRegister(BK4819_REG_13, gain_table[gs->table_index].reg_val);
        gs->table_index_prev = gs->table_index;
    }
}

int8_t GAIN_STAGING_GetGainDiff(uint8_t vfo)
{
    if (vfo > 1) return 0;

    // Same formula as AM_fix: original_dB - current_dB
    // Positive = gain was reduced, add back to RSSI for true signal strength
    return gain_table[0].gain_dB - gain_table[gGainStaging[vfo].table_index].gain_dB;
}