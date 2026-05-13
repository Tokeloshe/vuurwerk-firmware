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

#include "flashlight_watchdog.h"

#ifdef ENABLE_FLASHLIGHT

#include <stdint.h>

#include "app/flashlight.h"
#include "driver/gpio.h"
#include "bsp/dp32g030/gpio.h"
#include "helper/battery.h"

#define FW_TIMEOUT_SEC      1800u   /* 30 minutes of unattended ON / BLINK */
#define FW_TIMEOUT_LOW_SEC   600u   /* 10 minutes when battery is low */
#define FW_LOW_LEVEL_MAX        2   /* gBatteryDisplayLevel <= this is LOW */
#define FW_TICKS_PER_SEC     100u   /* 10 ms tick -> 100 ticks per second */

static uint8_t  prev_state;
static uint8_t  prescaler;
static uint16_t seconds_on;

void FLASHLIGHT_WATCHDOG_Tick(void)
{
	uint8_t s = (uint8_t)gFlashLightState;

	if (s != prev_state) {
		prev_state = s;
		prescaler = 0;
		seconds_on = 0;
		return;
	}

	if (s == FLASHLIGHT_OFF || s == FLASHLIGHT_SOS)
		return;

	if (++prescaler < FW_TICKS_PER_SEC)
		return;
	prescaler = 0;

	uint16_t timeout = (gBatteryDisplayLevel <= FW_LOW_LEVEL_MAX)
	                   ? FW_TIMEOUT_LOW_SEC : FW_TIMEOUT_SEC;
	if (++seconds_on >= timeout) {
		gFlashLightState = FLASHLIGHT_OFF;
		GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
		prev_state = FLASHLIGHT_OFF;
		seconds_on = 0;
	}
}

#endif
