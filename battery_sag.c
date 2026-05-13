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

/*
 * VUURWERK: TX battery sag delta tracker. Latches the per-TX peak
 * voltage sag (true_idle_avg - min_during_TX) in 10mV units and
 * exposes it for downstream display. Builds on Feature #36 (Live
 * Battery Voltage During TX), which keeps gBatteryVoltageAverage
 * live during transmission. Without that feature this module
 * would see a frozen value and report sag = 0 always.
 *
 * Baseline source: s_last_idle_avg mirrors gBatteryVoltageAverage
 * on every idle tick. On the TX-entry edge we capture s_pre_tx
 * from s_last_idle_avg rather than from the already-contaminated
 * gBatteryVoltageAverage (Feature #36's TX-tick ring advance
 * blends 1 TX sample into the 4-sample average BEFORE this tick
 * runs, so a same-tick read is biased ~25% toward the TX-loaded
 * value). The clean-idle baseline aligns reported sag values
 * with the documented operator thresholds in FEATURES.md
 * (Healthy 50-100mV, Tired 200-300mV, Aging 400+mV).
 */

#include <stdbool.h>
#include "battery_sag.h"
#include "functions.h"
#include "helper/battery.h"

static bool     s_was_tx;
static uint16_t s_pre_tx;
static uint16_t s_min_tx;
static uint16_t s_last_sag;
static uint16_t s_last_idle_avg;

void BATTERY_SAG_Tick500ms(void)
{
	bool is_tx = (gCurrentFunction == FUNCTION_TRANSMIT);

	if (is_tx && !s_was_tx) {
		s_pre_tx = s_last_idle_avg;
		s_min_tx = gBatteryVoltageAverage;
	} else if (is_tx) {
		if (gBatteryVoltageAverage < s_min_tx)
			s_min_tx = gBatteryVoltageAverage;
	} else if (s_was_tx) {
		s_last_sag = (s_pre_tx >= s_min_tx) ? (s_pre_tx - s_min_tx) : 0;
	}

	if (!is_tx)
		s_last_idle_avg = gBatteryVoltageAverage;

	s_was_tx = is_tx;
}

uint16_t BATTERY_SAG_GetLast10mV(void)
{
	return s_last_sag;
}
