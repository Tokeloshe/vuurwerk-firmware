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

#include <stdint.h>

#include "backlight_fade.h"
#include "driver/backlight.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"

#define BACKLIGHT_FADE_TAIL_TICKS 4U

void BACKLIGHT_FADE_Tick500ms(void)
{
	if (!BACKLIGHT_IsOn())
		return;

	const uint16_t cd = gBacklightCountdown_500ms;
	if (cd == 0U || cd > BACKLIGHT_FADE_TAIL_TICKS)
		return;

	const uint8_t max = gEeprom.BACKLIGHT_MAX;
	const uint8_t floor = (uint8_t)(gEeprom.BACKLIGHT_MIN + 1U);
	if (max <= floor)
		return;

	/* Power-of-two TAIL keeps the divide a free shift on Cortex-M0. */
	const uint8_t range = (uint8_t)(max - floor);
	const uint8_t step  = (uint8_t)(BACKLIGHT_FADE_TAIL_TICKS - cd + 1U);
	const uint8_t cut   = (uint8_t)((range * step) >> 2);
	const uint8_t target = (cut < range) ? (uint8_t)(max - cut) : floor;
	BACKLIGHT_SetBrightness(target);
}

void BACKLIGHT_FADE_ArmDuringActivity(void)
{
	const FUNCTION_Type_t f = gCurrentFunction;
	const uint8_t bits = (uint8_t)gSetting_backlight_on_tx_rx;
	const bool tx = (f == FUNCTION_TRANSMIT) && (bits & BACKLIGHT_ON_TR_TX);
	const bool rx = (f == FUNCTION_RECEIVE || f == FUNCTION_INCOMING
	                 || f == FUNCTION_MONITOR) && (bits & BACKLIGHT_ON_TR_RX);
	if (tx || rx)
		BACKLIGHT_TurnOn();
}
