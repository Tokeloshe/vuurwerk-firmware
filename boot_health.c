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
 * SPDX-License-Identifier: GPL-3.0-or-later
 * VUURWERK boot-time hardware health probe implementation.
 *
 * Probe runs once at boot, immediately after BK4819_Init() resets
 * REG_00. A healthy chip with a working SPI bus reads back the
 * just-written control-register value (or any chip-specific value
 * that is decisively not the SPI-floating signature 0xFFFF). A
 * wedged SPI returns 0xFFFF on every read.
 *
 * The 0xFFFF check is conservative: it cannot detect every
 * conceivable BK4819 fault, but it does catch the failure mode
 * that produces the most-confusing operator experience -- a radio
 * that boots normally, the welcome screen renders, and yet every
 * downstream feature (squelch, RSSI, CTCSS, AGC) silently fails
 * because each register read returns the same 0xFFFF.
 *
 * The EEPROM probe reads 8 bytes from the factory-programmed battery
 * calibration page (0x1F40) and checks two fault
 * signatures. If every byte equals 0xFF, the I2C bus is floating
 * (no slave responding, pull-ups carry both lines high) or the
 * EEPROM chip is blanked (failed reflash, mass-erase before
 * settings re-init). If every byte equals 0x00, SDA is stuck low
 * (slave or external short to GND holding SDA while the master
 * clocks SCL); the bit-bang I2C_Read in driver/i2c.c reads SDA
 * via GPIO_CheckBit, so a stuck-low line returns 0 on every clocked
 * bit and 0x00 across the page. v1.5.1: the second read of the
 * same page and its byte compare, which caught marginal-bus reads
 * that are neither all-0xFF nor all-0x00 but vary between passes,
 * was retired to fund PATROL (8 bytes); only the two static
 * signatures remain. settings.c:295-301 partial-recovery invariants
 * (gBatteryCalibration[0]/[1] never both zero on a factory unit)
 * bound false-positive risk to zero across both signatures.
 * Both map to the same FAULT_BIT_EEPROM flag because the
 * welcome-screen banner ("EEPROM FAULT / calib lost") is general
 * enough to cover any of the failure modes; diagnosis path is
 * identical (replace EEPROM or send for hardware service).
 *
 * Register ownership: REG_00 read-only liveness check (BK4819 side).
 * No BK4819 ownership conflict. EEPROM access via the public
 * EEPROM_ReadBuffer API only; no driver/eeprom.c modification.
 * FEATURES.md ownership table updated in the same commit.
 *
 *
 */

#include <string.h>

#include "boot_health.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "ui/helper.h"

#define BK4819_FLOATING_SPI_SIGNATURE 0xFFFF
#define EEPROM_BATTERY_CAL_ADDRESS    0x1F40

#define FAULT_BIT_BK4819 0x01
#define FAULT_BIT_EEPROM 0x02

static uint8_t s_fault;

void BOOT_HEALTH_Probe(void)
{
	if (BK4819_ReadRegister(BK4819_REG_00) == BK4819_FLOATING_SPI_SIGNATURE) {
		s_fault |= FAULT_BIT_BK4819;
		// G040: RADIO_SetupRegisters spins forever on a wedged bus, so draw
		// the banner now and hold it while REG_00 still floats. A one-off
		// glitch boots on.
		UI_PrintString("BK4819", 0, 127, 0, 12);
		UI_PrintString("FAULT", 0, 127, 3, 12);
		ST7565_BlitFullScreen();
		BACKLIGHT_SetBrightness(10);
		while (BK4819_ReadRegister(BK4819_REG_00) == BK4819_FLOATING_SPI_SIGNATURE) {}
	}

	uint8_t b[8];
	EEPROM_ReadBuffer(EEPROM_BATTERY_CAL_ADDRESS, b, sizeof(b));
	uint8_t a = b[0] & b[1] & b[2] & b[3] & b[4] & b[5] & b[6] & b[7];
	uint8_t o = b[0] | b[1] | b[2] | b[3] | b[4] | b[5] | b[6] | b[7];
	if (a == 0xFF || o == 0) {
		s_fault |= FAULT_BIT_EEPROM;
	}

}

bool BOOT_HEALTH_HasFault(void)
{
	return s_fault != 0;
}

bool BOOT_HEALTH_HasBk4819Fault(void)
{
	return (s_fault & FAULT_BIT_BK4819) != 0;
}

bool BOOT_HEALTH_HasEepromFault(void)
{
	return (s_fault & FAULT_BIT_EEPROM) != 0;
}
