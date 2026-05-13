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

#include <string.h>
#include "ui/vuurwerk_about.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "ui/helper.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "external/printf/printf.h"
#include "battery_sag.h"

static void drain_keys(void) {
    while (KEYBOARD_Poll() != KEY_INVALID)
        SYSTEM_DelayMs(10);
}

void VUURWERK_ABOUT_Show(void) {
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    UI_PrintStringSmallBold("VUURWERK " VERSION_STRING, 8, 127, 0);
    UI_PrintStringSmallNormal("Precision RF", 20, 127, 1);
    UI_PrintStringSmallNormal("Features:", 0, 127, 2);
    static const char *const features[4] = {
        "Adaptive Squelch", "Signal Quality", "Spectrum+", "TX Compressor"
    };
    for (uint8_t i = 0; i < 4; i++)
        UI_PrintStringSmallNormal(features[i], 2, 127, 3 + i);

    char sag_buf[20];
    sprintf(sag_buf, "TX sag %umV", BATTERY_SAG_GetLast10mV() * 10);
    UI_PrintStringSmallNormal(sag_buf, 2, 127, 7);

    ST7565_BlitFullScreen();

    drain_keys();

    uint16_t timeout = 500;
    while (timeout-- > 0) {
        KEY_Code_t key = KEYBOARD_Poll();
        if (key == KEY_EXIT || key == KEY_MENU)
            break;
        SYSTEM_DelayMs(20);
    }

    drain_keys();

    gRequestDisplayScreen = DISPLAY_MENU;
}
