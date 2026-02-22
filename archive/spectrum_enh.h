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

#ifndef SPECTRUM_ENH_H
#define SPECTRUM_ENH_H

#include <stdint.h>
#include <stdbool.h>

#define SPECTRUM_WIDTH 128
#define WATERFALL_HEIGHT 8
#define WATERFALL_BYTES 128 // 1 bit per pixel

typedef enum {
    SPEC_MODE_NORMAL = 0,
    SPEC_MODE_PEAK_HOLD,
    SPEC_MODE_DIFFERENTIAL,
    SPEC_MODE_WATERFALL,
    SPEC_MODE_COUNT
} SpectrumMode_t;

typedef struct {
    int16_t noise_floor[SPECTRUM_WIDTH];
    uint16_t peak_hold[SPECTRUM_WIDTH];
    uint16_t prev_sweep[SPECTRUM_WIDTH];
    uint8_t waterfall[WATERFALL_BYTES];
    uint8_t waterfall_row;
    SpectrumMode_t mode;
    uint8_t peak_decay_rate;
} SpectrumEnh_t;

extern SpectrumEnh_t gSpectrumEnh;

void SPECTRUM_ENH_Init(void);
void SPECTRUM_ENH_ProcessSample(uint8_t idx, uint16_t rssi);
void SPECTRUM_ENH_UpdateNoiseFloor(void);
void SPECTRUM_ENH_UpdatePeakHold(void);
void SPECTRUM_ENH_UpdateWaterfall(void);
uint16_t SPECTRUM_ENH_GetDisplay(uint8_t idx);
void SPECTRUM_ENH_CycleMode(void);
const char* SPECTRUM_ENH_GetModeName(void);

#endif