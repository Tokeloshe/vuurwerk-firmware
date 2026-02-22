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

#include "ui/vuurwerk_about.h"
#include "driver/st7565.h"
#include "ui/helper.h"

void VUURWERK_ABOUT_Show(void) {
    ST7565_FillScreen(0x00);

    UI_PrintStringSmallBold("VUURWERK " VERSION_STRING, 8, 127, 0);
    UI_PrintStringSmallNormal("Precision RF", 20, 127, 1);
    UI_PrintStringSmallNormal("", 0, 127, 2);
    UI_PrintStringSmallNormal("Features:", 0, 127, 3);
    UI_PrintStringSmallNormal("Adaptive Squelch", 2, 127, 4);
    UI_PrintStringSmallNormal("Signal Quality", 2, 127, 5);
    UI_PrintStringSmallNormal("Spectrum+", 2, 127, 6);
    UI_PrintStringSmallNormal("TX Compressor", 2, 127, 7);

    ST7565_BlitFullScreen();
}
