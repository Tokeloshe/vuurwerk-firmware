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

#include "side_toast.h"
#include "toast.h"            /* TOAST_Show */
#include "app/chFrScanner.h"  /* gScanStateDir, SCAN_OFF */
#include "frequencies.h"      /* IS_MR_CHANNEL */
#include "misc.h"             /* gMonitor */
#include "settings.h"         /* gEeprom, ACTION_OPT_* */

void VUURWERK_SideToast(enum ACTION_OPT_t opt)
{
	switch (opt) {
	case ACTION_OPT_POWER:
		TOAST_Show(gTxVfo->OUTPUT_POWER == 0 ? "PWR: LOW"  :
		           gTxVfo->OUTPUT_POWER == 1 ? "PWR: MID"  :
		                                       "PWR: HIGH");
		break;

	case ACTION_OPT_MONITOR:
		TOAST_Show(gMonitor ? "MON ON" : "MON OFF");
		break;

	case ACTION_OPT_SCAN:
		TOAST_Show(gScanStateDir != SCAN_OFF ? "SCAN ON" : "SCAN OFF");
		break;

	case ACTION_OPT_KEYLOCK:
		TOAST_Show(gEeprom.KEY_LOCK ? "LOCKED" : "UNLOCKED");
		break;

	case ACTION_OPT_A_B:
		TOAST_Show(gEeprom.TX_VFO == 0 ? "VFO: A" : "VFO: B");
		break;

	case ACTION_OPT_VFO_MR:
		TOAST_Show(IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE) ? "MR MODE" : "VFO MODE");
		break;

	case ACTION_OPT_SWITCH_DEMODUL:
		TOAST_Show(gTxVfo->Modulation == 0 ? "MOD: FM"  :
		           gTxVfo->Modulation == 1 ? "MOD: AM"  :
		                                     "MOD: USB");
		break;

	default:
		/* FLASHLIGHT, SPECTRUM, ALARM, 1750, FM, VOX, NONE: the
		 * action either owns the screen, lights up the LED, or
		 * is disabled in this build. No toast. */
		break;
	}
}
