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

#include "squelch.h"

// Global state
AdaptiveSquelch_t gAdaptiveSquelch = {
	.noise_floor_dBm = -120,
	.squelch_open_count = 0,
	.noise_samples = 0
};

void SQUELCH_Init(void)
{
	gAdaptiveSquelch.noise_floor_dBm = -120;
	gAdaptiveSquelch.squelch_open_count = 0;
	gAdaptiveSquelch.noise_samples = 0;
}