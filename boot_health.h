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
 * VUURWERK boot-time hardware health probe.
 *
 * Catches two operator-confusing silent-failure modes:
 * - BK4819 SPI wedged (every BK4819 read returning 0xFFFF after
 * an ESD event or SPI bus glitch); RX/TX path silently broken.
 * - EEPROM I2C wedged or chip blanked (every EEPROM read returning
 * 0xFF); battery percent and S-meter silently mis-render because
 * factory calibration is unreadable.
 *
 * Both surface on the welcome screen as a clear FAULT banner so the
 * operator can diagnose at the right layer (hardware) at the right
 * moment (boot, before anything tries to use the chip).
 *
 *
 */

#ifndef BOOT_HEALTH_H
#define BOOT_HEALTH_H

#include <stdbool.h>

void BOOT_HEALTH_Probe(void);
bool BOOT_HEALTH_HasFault(void);
bool BOOT_HEALTH_HasBk4819Fault(void);
bool BOOT_HEALTH_HasEepromFault(void);

#endif
