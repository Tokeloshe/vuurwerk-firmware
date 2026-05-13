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
 * leaves the radio in a clean RX-link state (REG_30 reset, AF muted)
 * so the caller's existing audio-path management can resume normally.
 *
 * Register ownership: REG_70/REG_71/REG_30/REG_50 are touched here in
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

#define PALETTE_TONE_MS 50

void AUDIO_PALETTE_PlayFoundVoice(void)
{
	AUDIO_AudioPathOn();

	BK4819_PlayTone(800, true);
	BK4819_ExitTxMute();
	SYSTEM_DelayMs(PALETTE_TONE_MS);

	BK4819_PlayTone(1200, true);
	BK4819_ExitTxMute();
	SYSTEM_DelayMs(PALETTE_TONE_MS);

	BK4819_TurnsOffTones_TurnsOnRX();
	AUDIO_AudioPathOff();
}
