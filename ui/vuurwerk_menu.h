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
#ifndef VUURWERK_MENU_H
#define VUURWERK_MENU_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_CATEGORY_ITEMS  20   // largest category is CONFIG with 15 items

typedef enum {
    MCAT_RECEIVE = 0,   // "RECEIVE"
    MCAT_TONE,          // "TONE"
    MCAT_TRANSMIT,      // "TX"
    MCAT_SCAN,          // "SCAN"
    MCAT_CHANNEL,       // "CHANNEL"
    MCAT_CONFIG,        // "CONFIG"  (was DISPLAY + SYSTEM merged)
    MCAT_UNLOCK,        // "UNLOCK"  (hidden, gF_LOCK only)
    MCAT_COUNT          // = 7
} MenuCategory_t;

// Category state
extern bool     gMenuShowCategoryPicker;  // true = category picker visible
extern int8_t   gSelectedCategory;        // -1 = none selected, 0-6 = active category
extern uint8_t  gCategoryPickerCursor;    // cursor in category picker (0 to visible_count-1)

// Filtered item list for current category
extern uint8_t  gCategoryItems[MAX_CATEGORY_ITEMS];  // MenuList[] indices
extern uint8_t  gCategoryItemCount;                    // items in current category
extern uint8_t  gCategoryItemIdx;                      // cursor within category items

// API
void     VUURWERK_MENU_Init(void);
const char* VUURWERK_MENU_GetCategoryName(uint8_t cat);
uint8_t  VUURWERK_MENU_GetVisibleCategoryCount(void);
uint8_t  VUURWERK_MENU_GetCategoryForMenuId(uint8_t menu_id);

// Build filtered item list for a category
void     VUURWERK_MENU_SelectCategory(uint8_t cat);

// Enter category picker mode
void     VUURWERK_MENU_EnterCategoryPicker(void);

// Sync gMenuCursor from gCategoryItems[gCategoryItemIdx]
void     VUURWERK_MENU_SyncCursor(void);

#endif
