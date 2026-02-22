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

#include "deviation_cal.h"

DeviationCal_t gDeviationCal = {
	.vhf_offset = 0,
	.uhf_offset = 0
};

void DEVIATION_CAL_Init(void)
{
	gDeviationCal.vhf_offset = 0;
	gDeviationCal.uhf_offset = 0;
}

int8_t DEVIATION_CAL_GetOffset(uint32_t frequency_10Hz)
{
	return (frequency_10Hz < 30000000) ? gDeviationCal.vhf_offset : gDeviationCal.uhf_offset;
}

void DEVIATION_CAL_Adjust(uint8_t band, int8_t delta)
{
	int8_t *offset = (band == 0) ? &gDeviationCal.vhf_offset : &gDeviationCal.uhf_offset;
	int8_t val = *offset + delta;

	if (val < DEVIATION_CAL_OFFSET_MIN)
		val = DEVIATION_CAL_OFFSET_MIN;
	if (val > DEVIATION_CAL_OFFSET_MAX)
		val = DEVIATION_CAL_OFFSET_MAX;

	*offset = val;
}

uint8_t DEVIATION_CAL_Get(uint8_t band)
{
	int8_t offset = (band == 0) ? gDeviationCal.vhf_offset : gDeviationCal.uhf_offset;
	return (uint8_t)(100 + offset * 5);
}