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

#include "signal_classifier.h"

SignalClassifier_t gSignalClassifier[2];

void SIGNAL_CLASSIFIER_Init(void) {
    for (uint8_t i = 0; i < 2; i++) {
        gSignalClassifier[i].prev_rssi = -127;
        gSignalClassifier[i].rise_time_ms = 0;
        gSignalClassifier[i].stable_count = 0;
        gSignalClassifier[i].classification = SIGNAL_CLASS_NOISE;
    }
}

void SIGNAL_CLASSIFIER_Update(uint8_t vfo, int16_t rssi_dbm) {
    if (vfo > 1) return;

    SignalClassifier_t *sc = &gSignalClassifier[vfo];

    // Check if signal is rising
    if (rssi_dbm > sc->prev_rssi + 3) {
        // Rising edge detected
        if (sc->rise_time_ms == 0) {
            // Start timing
            sc->rise_time_ms = 10; // First 10ms sample
        } else if (sc->rise_time_ms < 500) {
            sc->rise_time_ms += 10; // Increment by 10ms
        }
    } else if (rssi_dbm > sc->prev_rssi - 3 && rssi_dbm < sc->prev_rssi + 3) {
        // Stable signal
        if (sc->rise_time_ms > 0) {
            // Rise complete, classify
            if (sc->rise_time_ms < 50) {
                sc->classification = SIGNAL_CLASS_FAST;
            } else if (sc->rise_time_ms < 200) {
                sc->classification = SIGNAL_CLASS_NORMAL;
            } else {
                sc->classification = SIGNAL_CLASS_SLOW;
            }

            if (sc->stable_count < 255) {
                sc->stable_count++;
            }
        }
    } else {
        // Falling or unstable
        if (sc->stable_count < 3) {
            sc->classification = SIGNAL_CLASS_NOISE;
        }
        sc->rise_time_ms = 0;
        if (sc->stable_count > 0) {
            sc->stable_count--;
        }
    }

    sc->prev_rssi = rssi_dbm;
}

SignalClass_t SIGNAL_CLASSIFIER_GetClass(uint8_t vfo) {
    if (vfo > 1) return SIGNAL_CLASS_NOISE;
    return gSignalClassifier[vfo].classification;
}

char SIGNAL_CLASSIFIER_GetSymbol(uint8_t vfo) {
    if (vfo > 1) return '~';

    switch (gSignalClassifier[vfo].classification) {
        case SIGNAL_CLASS_FAST:   return 'F';
        case SIGNAL_CLASS_NORMAL: return 'N';
        case SIGNAL_CLASS_SLOW:   return 'S';
        default:                  return '~';
    }
}