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

#include "app/app.h"
#include "app/generic.h"
#include "app/menu.h"
#include "app/scanner.h"
#include "dtmf.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "toast.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

DCS_CodeType_t    gScanCssResultType;
uint8_t           gScanCssResultCode;
SCAN_SaveState_t  gScannerSaveState;
uint8_t           gScanChannel;
uint32_t          gScanFrequency;
SCAN_CssState_t   gScanCssState;
uint8_t           gScanProgressIndicator;
bool              gScanUseCssResult;

uint8_t           scanHitCount;   // VUURWERK v1.3.0 (A1): stepSetting removed with freq-hunt mode


static void SCANNER_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed)
	{
		if (gScannerSaveState == SCAN_SAVE_CHAN_SEL) {
			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

			INPUTBOX_Append(Key);

			gRequestDisplayScreen = DISPLAY_SCANNER;

			if (gInputBoxIndex < 3) {
#ifdef ENABLE_VOICE
				gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
				return;
			}

			gInputBoxIndex = 0;

			uint16_t chan = ((gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2]) - 1;
			if (IS_MR_CHANNEL(chan)) {
#ifdef ENABLE_VOICE
				gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
				gShowChPrefix = RADIO_CheckValidChannel(chan, false, 0);
				gScanChannel  = (uint8_t)chan;
				return;
			}
		}

		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
	}
}

static void SCANNER_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed) { // short pressed
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

		switch (gScannerSaveState) {
			case SCAN_SAVE_NO_PROMPT:
				SCANNER_Stop();
				gRequestDisplayScreen    = DISPLAY_MAIN;
				break;

			case SCAN_SAVE_CHAN_SEL:
				if (gInputBoxIndex > 0) {
					gInputBox[--gInputBoxIndex] = 10;
					gRequestDisplayScreen       = DISPLAY_SCANNER;
					break;
				}

				// Fallthrough

			case SCAN_SAVE_CHANNEL:
				gScannerSaveState     = SCAN_SAVE_NO_PROMPT;
#ifdef ENABLE_VOICE
				gAnotherVoiceID   = VOICE_ID_CANCEL;
#endif
				gRequestDisplayScreen = DISPLAY_SCANNER;
				break;
		}
	}
}

static void SCANNER_Key_MENU(bool bKeyPressed, bool bKeyHeld)
{
	if (bKeyHeld || !bKeyPressed) // ignore long press or release button events
		return;

	// VUURWERK v1.3.0 (A1): the frequency-hunt mode (SCANNER_Start with
	// singleFreq == false) was unreachable -- every launch path (F+STAR,
	// menu R-CTCS/R-DCS, STAR restart) is single-frequency. Its guards,
	// the step-rounding block, the InitInfo save arm, the SCANNER_Start
	// else arm and the SCAN_CSS_STATE_OFF tick case are removed; the
	// BK4819 frequency-scan driver calls strip out with them.
	if (gScanCssState == SCAN_CSS_STATE_SCANNING) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	if (gScanCssState == SCAN_CSS_STATE_FAILED) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

	switch (gScannerSaveState) {
		case SCAN_SAVE_NO_PROMPT:
			if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
				gScannerSaveState = SCAN_SAVE_CHAN_SEL;
				gScanChannel      = gTxVfo->CHANNEL_SAVE;
				gShowChPrefix     = RADIO_CheckValidChannel(gTxVfo->CHANNEL_SAVE, false, 0);
			}
			else {
				gScannerSaveState = SCAN_SAVE_CHANNEL;
			}

			gScanCssState         = SCAN_CSS_STATE_FOUND;
#ifdef ENABLE_VOICE
			gAnotherVoiceID   = VOICE_ID_MEMORY_CHANNEL;
#endif
			gRequestDisplayScreen = DISPLAY_SCANNER;
			
			gUpdateStatus = true;
			break;

		case SCAN_SAVE_CHAN_SEL:
			if (gInputBoxIndex == 0) {
				gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
				gRequestDisplayScreen = DISPLAY_SCANNER;
				gScannerSaveState     = SCAN_SAVE_CHANNEL;
			}
			break;

		case SCAN_SAVE_CHANNEL:
			// VUURWERK v1.3.0 (A1): single-frequency save path only.
			RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
			RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

			gTxVfo->freq_config_RX.CodeType = gScanCssResultType;
			gTxVfo->freq_config_RX.Code     = gScanCssResultCode;
			gTxVfo->freq_config_TX.CodeType = gScanCssResultType;
			gTxVfo->freq_config_TX.Code     = gScanCssResultCode;

			uint8_t chan;
			if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
				chan = gScanChannel;
				gEeprom.MrChannel[gEeprom.TX_VFO] = chan;
			}
			else {
				chan = gTxVfo->Band + FREQ_CHANNEL_FIRST;
				gEeprom.FreqChannel[gEeprom.TX_VFO] = chan;
			}

			gTxVfo->CHANNEL_SAVE = chan;
			gEeprom.ScreenChannel[gEeprom.TX_VFO] = chan;
#ifdef ENABLE_VOICE	
			gAnotherVoiceID = VOICE_ID_CONFIRM;
#endif
			gRequestDisplayScreen = DISPLAY_SCANNER;
			gRequestSaveChannel = 2;
			gScannerSaveState = SCAN_SAVE_NO_PROMPT;
			break;

		default:
			gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
			break;
	}
}

static void SCANNER_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyHeld && bKeyPressed) {
		gBeepToPlay    = BEEP_1KHZ_60MS_OPTIONAL;
		SCANNER_Start();   // VUURWERK v1.3.0 (A1)
	}
	return;
}

static void SCANNER_Key_UP_DOWN(bool bKeyPressed, bool pKeyHeld, int8_t Direction)
{
	if (pKeyHeld) {
		if (!bKeyPressed)
			return;
	}
	else {
		if (!bKeyPressed)
			return;

		gInputBoxIndex = 0;
		gBeepToPlay    = BEEP_1KHZ_60MS_OPTIONAL;
	}

	if (gScannerSaveState == SCAN_SAVE_CHAN_SEL) {
		gScanChannel          = NUMBER_AddWithWraparound(gScanChannel, Direction, 0, MR_CHANNEL_LAST);
		gShowChPrefix         = RADIO_CheckValidChannel(gScanChannel, false, 0);
		gRequestDisplayScreen = DISPLAY_SCANNER;
	}
	else
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

void SCANNER_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	switch (Key) {
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			SCANNER_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
			break;
		case KEY_MENU:
			SCANNER_Key_MENU(bKeyPressed, bKeyHeld);
			break;
		case KEY_UP:
			SCANNER_Key_UP_DOWN(bKeyPressed, bKeyHeld,  1);
			break;
		case KEY_DOWN:
			SCANNER_Key_UP_DOWN(bKeyPressed, bKeyHeld, -1);
			break;
		case KEY_EXIT:
			SCANNER_Key_EXIT(bKeyPressed, bKeyHeld);
			break;
		case KEY_STAR:
			SCANNER_Key_STAR(bKeyPressed, bKeyHeld);
			break;
		case KEY_PTT:
			GENERIC_Key_PTT(bKeyPressed);
			break;
		default:
			if (!bKeyHeld && bKeyPressed)
				gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			break;
	}
}

void SCANNER_Start(void)
{
	// VUURWERK v1.3.0 (A1): frequency-hunt mode removed -- unreachable.
	gMonitor = false;

#ifdef ENABLE_VOICE
	gAnotherVoiceID = VOICE_ID_SCANNING_BEGIN;
#endif

	BK4819_StopScan();
	RADIO_SelectVfos();

#ifdef ENABLE_NOAA
	if (IS_NOAA_CHANNEL(gRxVfo->CHANNEL_SAVE))
		gRxVfo->CHANNEL_SAVE = FREQ_CHANNEL_FIRST + BAND6_400MHz;
#endif

	uint8_t  backupStep      = gRxVfo->STEP_SETTING;
	uint16_t backupFrequency = gRxVfo->StepFrequency;

	RADIO_InitInfo(gRxVfo, gRxVfo->CHANNEL_SAVE, gRxVfo->pRX->Frequency);

	gRxVfo->STEP_SETTING  = backupStep;
	gRxVfo->StepFrequency = backupFrequency;

	RADIO_SetupRegisters(true);

#ifdef ENABLE_NOAA
	gIsNoaaMode = false;
#endif

	{
		// === VUURWERK v1.2.7 Pre-flight RSSI gate ===
		// Park the receiver on the current frequency for 50 ms to let
		// AGC settle, then read raw RSSI. Below threshold raw 100
		// (~-110 dBm) means the operator parked on dead air; abort
		// with NO SIGNAL toast + 500 Hz double-beep so they can move
		// to a real carrier instead of waiting out the 30 s timeout.
		// VUURWERK: G081 set the frequency and the 0 indicator (UI reads
		// it as NO SIGNAL) before the gate can return
		gScanFrequency         = gRxVfo->pRX->Frequency;
		gScanProgressIndicator = 0;
		SYSTEM_DelayMs(50);
		if (BK4819_GetRSSI() < 100) {
			TOAST_Show("NO SIGNAL");
			gScanCssState  = SCAN_CSS_STATE_FAILED;
			gBeepToPlay    = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			gUpdateStatus  = true;
			// VUURWERK v1.3.0 (F9): this early return used to skip the
			// state-reset block at the bottom of SCANNER_Start, so a
			// STAR-restart from a save prompt kept the stale
			// gScannerSaveState (prompt UI on a FAILED scan) and the
			// previous session's CSS result. Reset what the abort path
			// actually leaves dirty.
			gScannerSaveState = SCAN_SAVE_NO_PROMPT;
			gScanUseCssResult = false;
			return;
		}
		// === end VUURWERK ===

		gScanCssState  = SCAN_CSS_STATE_SCANNING;

		BK4819_PickRXFilterPathBasedOnFrequency(gScanFrequency);
		BK4819_SetScanFrequency(gScanFrequency);

		gUpdateStatus = true;
	}

#ifdef ENABLE_DTMF_CALLING
	DTMF_clear_RX();
#endif

	gScanDelay_10ms        = scan_delay_10ms;
	gScanCssResultCode     = 0xFF;
	gScanCssResultType     = 0xFF;
	scanHitCount          = 0;
	gScanUseCssResult      = false;
	g_CxCSS_TAIL_Found     = false;
	g_CDCSS_Lost           = false;
	gCDCSSCodeType         = 0;
	g_CTCSS_Lost           = false;
#ifdef ENABLE_VOX
	g_VOX_Lost         = false;
#endif
	g_SquelchLost          = false;
	gScannerSaveState      = SCAN_SAVE_NO_PROMPT;
}

void SCANNER_Stop(void)
{
	if(SCANNER_IsScanning()) {
		if (!gCssBackgroundScan)   // VUURWERK: G076 only F+STAR takes the crossband backup
			gEeprom.CROSS_BAND_RX_TX = gBackup_CROSS_BAND_RX_TX;
		gUpdateStatus            = true;
		gCssBackgroundScan 			 = false;
		gScanUseCssResult = false;
#ifdef ENABLE_VOICE
		gAnotherVoiceID          = VOICE_ID_CANCEL;
#endif
		BK4819_StopScan();

		// VUURWERK: G032 undo SCANNER_Start's RADIO_InitInfo and re-arm the
		// receiver now; the deferred reload only ran from a key event
		RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
		RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
		RADIO_SelectVfos();
		RADIO_SetupRegisters(true);
		gUpdateDisplay = true;   // VUURWERK: G081 clear the menu SCAN animation
	}
}

void SCANNER_TimeSlice10ms(void)
{
	if (!SCANNER_IsScanning())
		return;

	if (gScanDelay_10ms > 0) {
		gScanDelay_10ms--;
		return;
	}

	if (gScannerSaveState != SCAN_SAVE_NO_PROMPT) {
		return;
	}

	switch (gScanCssState) {
		// VUURWERK v1.3.0 (A1): SCAN_CSS_STATE_OFF case removed with the
		// frequency-hunt mode; the state value stays (ordering compares
		// and BSS boot value) and falls to default like FOUND/FAILED.
		case SCAN_CSS_STATE_SCANNING: {
			uint32_t cdcssFreq;
			uint16_t ctcssFreq;
			BK4819_CssScanResult_t scanResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);
			if (scanResult == BK4819_CSS_RESULT_NOT_FOUND)
				break;

			BK4819_Disable();

			// === VUURWERK v1.2.7 CDCSS/CTCSS arm merge
			// (CTCSS dropped from 2-confirmation to 1-confirmation
			// to match DCS; the two arms then collapse to one block) ===
			uint8_t Code;
			DCS_CodeType_t CodeType;
			if (scanResult == BK4819_CSS_RESULT_CDCSS) {
				Code     = DCS_GetCdcssCode(cdcssFreq);
				CodeType = CODE_TYPE_DIGITAL;
			} else {
				Code     = DCS_GetCtcssCode(ctcssFreq);
				CodeType = CODE_TYPE_CONTINUOUS_TONE;
			}
			if (Code != 0xFF) {
				gScanCssResultCode = Code;
				gScanCssResultType = CodeType;
				gScanCssState      = SCAN_CSS_STATE_FOUND;
				gScanUseCssResult  = true;
				gUpdateStatus      = true;
				// VUURWERK: G038 the lock cue sounds now, not at the next key
				AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
			}
			// === end VUURWERK ===

			if (gScanCssState < SCAN_CSS_STATE_FOUND) { // scanning or off
				BK4819_SetScanFrequency(gScanFrequency);
				gScanDelay_10ms = scan_delay_10ms;
				break;
			}

			if(gCssBackgroundScan) {
				// VUURWERK: G032 show the result, then restore through
				// SCANNER_Stop (FOUND always carries gScanUseCssResult here)
				MENU_CssScanFound();
				SCANNER_Stop();
			}
			else
				GUI_SelectNextDisplay(DISPLAY_SCANNER);


			break;
		}
		default:
			// VUURWERK: G032 a menu scan that failed (NO SIGNAL or the
			// 30 s watchdog) restores the VFO and the receiver too
			if (gCssBackgroundScan)
				SCANNER_Stop();
			break;
	}

}

void SCANNER_TimeSlice500ms(void)
{
	if (SCANNER_IsScanning() && gScannerSaveState == SCAN_SAVE_NO_PROMPT && gScanCssState < SCAN_CSS_STATE_FOUND) {
		gScanProgressIndicator++;
#ifndef ENABLE_NO_CODE_SCAN_TIMEOUT
		if (gScanProgressIndicator > 32) {
			if (gScanCssState == SCAN_CSS_STATE_SCANNING && !gScanSingleFrequency)
				gScanCssState = SCAN_CSS_STATE_FOUND;
			else
				gScanCssState = SCAN_CSS_STATE_FAILED;

			gUpdateStatus = true;
		}
#endif
		gUpdateDisplay = true;
	}
	else if(gCssBackgroundScan) {
		gUpdateDisplay = true;
	}
}

bool SCANNER_IsScanning(void)
{
	return gCssBackgroundScan || (gScreenToDisplay == DISPLAY_SCANNER);
}