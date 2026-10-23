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
 * VUURWERK audio palette implementation.
 *
 * Drives BK4819_PlayTone directly to emit short chord sequences for
 * VUURWERK events that have no representation in the stock 5-frequency
 * AUDIO_PlayBeep vocabulary. Each entry point is synchronous and
 * leaves the radio in a clean RX-link state (REG_30 reset, AF type
 * restored, speaker path off) so the caller's existing audio-path
 * management can resume normally.
 *
 * Register ownership: REG_70/REG_71/REG_30/REG_47/REG_50 are touched here in
 * the same set-then-restore pattern used by audio.c; safe because
 * every caller invokes a palette helper from a single-threaded
 * idle/keyboard-handler context where no other module is mid-tone.
 *
 *
 */

#include "audio_palette.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "settings.h"

#define PALETTE_TONE_MS 50

// Two-tone chord: f1 then f2, each PALETTE_TONE_MS. Restores REG_47 (AF type,
// so AM/USB survive) with the speaker path off, so the caller's audio-path
// management resumes.
static void play_chord(uint16_t f1, uint16_t f2)
{
	const uint16_t reg47 = BK4819_ReadRegister(BK4819_REG_47);

	AUDIO_AudioPathOn();

	BK4819_PlayTone(f1, true);
	BK4819_ExitTxMute();
	SYSTEM_DelayMs(PALETTE_TONE_MS);

	BK4819_PlayTone(f2, true);
	BK4819_ExitTxMute();
	SYSTEM_DelayMs(PALETTE_TONE_MS);

	BK4819_TurnsOffTones_TurnsOnRX();
	AUDIO_AudioPathOff();
	BK4819_WriteRegister(BK4819_REG_47, reg47);   // tones left AF type MUTE (0)
}

void AUDIO_PALETTE_PlayFoundVoice(void)
{
	if (!gEeprom.BEEP_CONTROL)
		return;   // Beep OFF: silent hop, LockVoiceBin still reopens the audio

	play_chord(800, 1200);   // ascending: voice found
}
