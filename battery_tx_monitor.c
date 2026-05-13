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
 * Copyright 2026 James Honiball, KC3TFZ
 *
 * VUURWERK v1.2.6 -- Live battery voltage during TX.
 *
 * Stock egzumer/DualTachyon firmware samples battery voltage only when
 * gCurrentFunction != FUNCTION_TRANSMIT (app/app.c:1467 guard pattern,
 * identical across DualTachyon, egzumer, kamilsss655). The on-screen
 * battery icon and voltage/percent text therefore freeze at the
 * pre-PTT-down value for the entire TX duty cycle. Operators on long
 * key-down see no real-time feedback that the battery is sagging.
 *
 * This module re-uses BOARD_ADC_GetBatteryInfo (already-owned SARADC
 * peripheral access via the LAW-1-frozen driver/adc.h API) to advance
 * the rolling battery sample ring during TX, then calls
 * BATTERY_GetReadings to recompute the average and display level.
 *
 * The non-TX sampling loop at app/app.c:1472 is unchanged; the two
 * paths are mutually exclusive on gCurrentFunction. After PTT release,
 * the next non-TX sample naturally restores the ring with unloaded
 * voltages within ~2 s.
 *
 * GPL v3 + commercial dual-license. See LICENSE.
 *
 *
 */

#include "battery_tx_monitor.h"
#include "board.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"

void BATTERY_TX_MONITOR_Tick500ms(void)
{
	if (gCurrentFunction != FUNCTION_TRANSMIT)
		return;

	BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryVoltageIndex],
				 &gBatteryCurrent);
	if (++gBatteryVoltageIndex > 3)
		gBatteryVoltageIndex = 0;

	BATTERY_GetReadings(true);
}
