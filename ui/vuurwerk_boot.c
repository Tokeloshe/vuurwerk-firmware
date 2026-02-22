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

#include "ui/vuurwerk_boot.h"
#include "driver/st7565.h"
#include "ui/helper.h"
#include <string.h>

static bool boot_active = false;
static uint16_t boot_timer = 0;

void VUURWERK_BOOT_Show(void) {
    boot_active = true;
    boot_timer = 200; // 2 seconds

    ST7565_FillScreen(0x00);

    // Display "VUURWERK"
    UI_PrintStringSmallBold("VUURWERK", 20, 127, 1);

    // Display "v1.0.9.2"
    UI_PrintStringSmallNormal(VERSION_STRING, 36, 127, 3);

    // Display subtitle
    UI_PrintStringSmallNormal("Precision RF", 24, 127, 5);

    ST7565_BlitFullScreen();
}

bool VUURWERK_BOOT_IsActive(void) {
    if (boot_active && boot_timer > 0) {
        boot_timer--;
        if (boot_timer == 0) {
            boot_active = false;
        }
    }
    return boot_active;
}
