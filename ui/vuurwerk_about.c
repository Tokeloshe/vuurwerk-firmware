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

#include <string.h>
#include "ui/vuurwerk_about.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "ui/helper.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"

void VUURWERK_ABOUT_Show(void) {
    char buf[16];

    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    UI_PrintStringSmallBold("VUURWERK " VERSION_STRING, 8, 127, 0);
    UI_PrintStringSmallNormal("Precision RF", 20, 127, 1);
    UI_PrintStringSmallNormal("Features:", 0, 127, 2);
    UI_PrintStringSmallNormal("Adaptive Squelch", 2, 127, 3);
    UI_PrintStringSmallNormal("Signal Quality", 2, 127, 4);
    UI_PrintStringSmallNormal("Spectrum+", 2, 127, 5);
    UI_PrintStringSmallNormal("TX Compressor", 2, 127, 6);
    sprintf(buf, "GuardTrips: %u", gScreenChannelGuardTrips);
    UI_PrintStringSmallNormal(buf, 2, 127, 7);

    ST7565_BlitFullScreen();

    while (KEYBOARD_Poll() != KEY_INVALID)
        SYSTEM_DelayMs(10);

    uint16_t timeout = 500;
    while (timeout-- > 0) {
        KEY_Code_t key = KEYBOARD_Poll();
        if (key == KEY_EXIT || key == KEY_MENU)
            break;
        SYSTEM_DelayMs(20);
    }

    while (KEYBOARD_Poll() != KEY_INVALID)
        SYSTEM_DelayMs(10);

    gRequestDisplayScreen = DISPLAY_MENU;
}
