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

#ifndef SIGNAL_CLASSIFIER_H
#define SIGNAL_CLASSIFIER_H

#include <stdint.h>

typedef enum {
    SIGNAL_CLASS_NOISE = 0,
    SIGNAL_CLASS_FAST = 1,   // F: FM, digital
    SIGNAL_CLASS_NORMAL = 2, // N: SSB, AM
    SIGNAL_CLASS_SLOW = 3,   // S: carriers, CW
} SignalClass_t;

typedef struct {
    int16_t prev_rssi;
    uint16_t rise_time_ms;
    uint8_t stable_count;
    SignalClass_t classification;
} SignalClassifier_t;

extern SignalClassifier_t gSignalClassifier[2];

void SIGNAL_CLASSIFIER_Init(void);
void SIGNAL_CLASSIFIER_Update(uint8_t vfo, int16_t rssi_dbm);
SignalClass_t SIGNAL_CLASSIFIER_GetClass(uint8_t vfo);
char SIGNAL_CLASSIFIER_GetSymbol(uint8_t vfo);

#endif