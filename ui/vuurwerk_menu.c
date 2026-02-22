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

#include "vuurwerk_menu.h"
#include "menu.h"
#include "../misc.h"

// State variables
bool     gMenuShowCategoryPicker = true;
int8_t   gSelectedCategory       = -1;
uint8_t  gCategoryPickerCursor   = 0;

uint8_t  gCategoryItems[MAX_CATEGORY_ITEMS];
uint8_t  gCategoryItemCount = 0;
uint8_t  gCategoryItemIdx   = 0;

// Category display names
static const char * const sCategoryNames[MCAT_COUNT] = {
	"RECEIVE",
	"TONE",
	"TX",
	"SCAN",
	"CHANNEL",
	"CONFIG",
	"UNLOCK"
};

const char* VUURWERK_MENU_GetCategoryName(uint8_t cat)
{
	if (cat < MCAT_COUNT)
		return sCategoryNames[cat];
	return "?";
}

uint8_t VUURWERK_MENU_GetVisibleCategoryCount(void)
{
	if (gF_LOCK)
		return MCAT_COUNT;     // 7 categories including UNLOCK
	return MCAT_COUNT - 1;     // 6 categories excluding UNLOCK
}

uint8_t VUURWERK_MENU_GetCategoryForMenuId(uint8_t menu_id)
{
	switch (menu_id)
	{
		// RECEIVE
		case MENU_SQL:
		case MENU_STEP:
		case MENU_W_N:
		case MENU_AM:
		case MENU_COMPAND:
#ifdef ENABLE_AM_FIX
		case MENU_AM_FIX:
#endif
		case MENU_TDR:
			return MCAT_RECEIVE;

		// TONE
		case MENU_R_CTCS:
		case MENU_T_CTCS:
		case MENU_R_DCS:
		case MENU_T_DCS:
		case MENU_STE:
		case MENU_RP_STE:
		case MENU_SCR:
			return MCAT_TONE;

		// TRANSMIT (includes former DTMF items)
		case MENU_TXP:
		case MENU_SFT_D:
		case MENU_OFFSET:
		case MENU_TOT:
		case MENU_MIC:
		case MENU_BCL:
		case MENU_ROGER:
		case MENU_UPCODE:       // was DTMF category
		case MENU_DWCODE:       // was DTMF category
		case MENU_PTT_ID:       // was DTMF category
		case MENU_D_ST:         // was DTMF category
		case MENU_D_PRE:        // was DTMF category
		case MENU_D_LIVE_DEC:   // was DTMF category
			return MCAT_TRANSMIT;

		// SCAN
		case MENU_S_LIST:
		case MENU_SLIST1:
		case MENU_SLIST2:
		case MENU_S_ADD1:
		case MENU_S_ADD2:
		case MENU_SC_REV:
		case MENU_SCANWATCH:    // Scan+Watch + VFO Split
			return MCAT_SCAN;

		// CHANNEL
		case MENU_MEM_CH:
		case MENU_DEL_CH:
		case MENU_MEM_NAME:
		case MENU_MDF:
		case MENU_1_CALL:
			return MCAT_CHANNEL;

		// CONFIG (merged DISPLAY + SYSTEM)
		case MENU_ABR:
		case MENU_ABR_MIN:
		case MENU_ABR_MAX:
		case MENU_ABR_ON_TX_RX:
		case MENU_BAT_TXT:
		case MENU_PONMSG:
		case MENU_BEEP:
		case MENU_AUTOLK:
		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG:
		case MENU_VOL:          // was SYSTEM
		case MENU_SAVE:         // was SYSTEM
			return MCAT_CONFIG;

		// UNLOCK (hidden items)
		case MENU_F_LOCK:
		case MENU_200TX:
		case MENU_350TX:
		case MENU_500TX:
		case MENU_350EN:
		case MENU_SCREN:
		case MENU_BATCAL:
		case MENU_BATTYP:
		case MENU_RESET:
			return MCAT_UNLOCK;

		default:
			return MCAT_CONFIG;  // safe fallback
	}
}

void VUURWERK_MENU_SelectCategory(uint8_t cat)
{
	gCategoryItemCount = 0;
	gCategoryItemIdx   = 0;

	// Walk all visible MenuList items and pick those matching the category
	for (uint8_t i = 0; i < gMenuListCount; i++)
	{
		if (VUURWERK_MENU_GetCategoryForMenuId(MenuList[i].menu_id) == cat)
		{
			if (gCategoryItemCount < MAX_CATEGORY_ITEMS)
				gCategoryItems[gCategoryItemCount++] = i;
		}
	}

	VUURWERK_MENU_SyncCursor();
}

void VUURWERK_MENU_SyncCursor(void)
{
	if (gCategoryItemCount > 0)
		gMenuCursor = gCategoryItems[gCategoryItemIdx];
}

void VUURWERK_MENU_EnterCategoryPicker(void)
{
	gMenuShowCategoryPicker = true;
	gSelectedCategory       = -1;
	gCategoryPickerCursor   = 0;
}

void VUURWERK_MENU_Init(void)
{
	gMenuShowCategoryPicker = true;
	gSelectedCategory       = -1;
	gCategoryPickerCursor   = 0;
	gCategoryItemCount      = 0;
	gCategoryItemIdx        = 0;
}
