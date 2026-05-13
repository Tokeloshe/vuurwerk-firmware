/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <string.h>

#include "bsp/dp32g030/gpio.h"
#include "driver/eeprom.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "helper/battery.h"
#include "settings.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/welcome.h"
#include "ui/status.h"
#include "version.h"
#include "boot_health.h"

void UI_DisplayReleaseKeys(void)
{
	memset(gStatusLine,  0, sizeof(gStatusLine));
	UI_DisplayClear();

	UI_PrintString("RELEASE", 0, 127, 1, 10);
	UI_PrintString("ALL KEYS", 0, 127, 3, 10);

	char buf[6];
	const char *n = NULL;
	if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
		n = "PTT";
	} else {
		KEY_Code_t k = KEYBOARD_Poll();
		if (k <= KEY_9) {
			buf[0] = '0' + (char)k;
			buf[1] = 0;
			n = buf;
		} else {
			switch (k) {
				case KEY_SIDE1: n = "SIDE1"; break;
				case KEY_SIDE2: n = "SIDE2"; break;
				case KEY_MENU:  n = "MENU";  break;
				case KEY_EXIT:  n = "EXIT";  break;
				case KEY_UP:    n = "UP";    break;
				case KEY_DOWN:  n = "DOWN";  break;
				case KEY_STAR:  n = "STAR";  break;
				case KEY_F:     n = "F";     break;
				default: break;
			}
		}
	}
	if (n != NULL) {
		UI_PrintStringSmallNormal(n, 0, 128, 6);
	}

	ST7565_BlitStatusLine();  // blank status line
	ST7565_BlitFullScreen();
}

void UI_DisplayWelcome(void)
{
	char WelcomeString0[16];
	char WelcomeString1[16];

	memset(gStatusLine,  0, sizeof(gStatusLine));
	UI_DisplayClear();

	if (BOOT_HEALTH_HasFault()) {
		const bool eep_only = BOOT_HEALTH_HasEepromFault() && !BOOT_HEALTH_HasBk4819Fault();
		UI_PrintString(eep_only ? "EEPROM" : "BK4819", 0, 127, 0, 12);
		UI_PrintString("FAULT", 0, 127, 3, 12);
		UI_PrintStringSmallNormal(eep_only ? "calib lost" : "RX/TX disabled", 0, 128, 6);
	} else if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_FULL_SCREEN) {
		// FillScreen writes the LCD directly; no Blit pair needed.
		ST7565_FillScreen(0xFF);
		return;
	} else if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_VOLTAGE) {
		// strcpy and sprintf both null-terminate within the 16-byte
		// buffer; the prior memset-to-zero is redundant defensive
		// zeroing and is omitted.
		strcpy(WelcomeString0, "VOLTAGE");
		sprintf(WelcomeString1, "%u.%02uV %u%%",
			gBatteryVoltageAverage / 100,
			gBatteryVoltageAverage % 100,
			BATTERY_VoltsToPercent(gBatteryVoltageAverage));

		UI_PrintString(WelcomeString0, 0, 127, 0, 10);
		UI_PrintString(WelcomeString1, 0, 127, 2, 10);
		UI_PrintStringSmallNormal(Version, 0, 128, 6);
	} else {
		// VUURWERK branding boot screen
		UI_PrintString("VUURWERK", 0, 127, 0, 10);
		UI_PrintStringSmallNormal(VERSION_STRING, 0, 128, 3);
		UI_PrintStringSmallNormal("Precision RF", 0, 128, 5);
	}

	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}
