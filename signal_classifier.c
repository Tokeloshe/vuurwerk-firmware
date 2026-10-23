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

#include "signal_classifier.h"

// G050: classifier retired. Rise time cannot tell FM, SSB and carriers
// apart (any keyed carrier rises within one tick), so it showed '~' on
// nearly every reception and its class-based holds never engaged. The
// GetClass stub keeps the API: NOISE folds the gain-staging and AM-fix
// holds to the stock 30 ticks under LTO.

SignalClass_t SIGNAL_CLASSIFIER_GetClass(uint8_t vfo) {
    (void)vfo;
    return SIGNAL_CLASS_NOISE;
}
