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

#include "scan_rate.h"

static uint8_t step_counter;
static uint8_t last_cps;
static uint8_t tick_counter;
static uint8_t quiet_ticks;

void SCAN_RATE_NoteStep(void)
{
	if (quiet_ticks >= 50)
		last_cps = 0;
	quiet_ticks = 0;
	if (step_counter < 0xFF)
		step_counter++;
}

void SCAN_RATE_Tick10ms(void)
{
	if (quiet_ticks < 0xFF)
		quiet_ticks++;
	if (++tick_counter >= 100) {
		tick_counter = 0;
		last_cps     = step_counter;
		step_counter = 0;
	}
}

uint8_t SCAN_RATE_ChannelsPerSec(void)
{
	return last_cps;
}

void SCAN_RATE_Reset(void)
{
	step_counter = 0;
	last_cps     = 0;
	tick_counter = 0;
	quiet_ticks  = 0;
}
