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

#ifndef SIDE_TOAST_H
#define SIDE_TOAST_H

#include "settings.h" /* enum ACTION_OPT_t */

/* VUURWERK side-button toast feedback. Maps an ACTION_OPT_t (the enum
 * the stock action.c dispatcher uses to identify side-button bindings)
 * to a 1-second on-screen confirmation via the existing TOAST_Show()
 * API. Touches no BK4819 register. Safe to call with any opt value;
 * unsupported or screen-owning actions (FLASHLIGHT, SPECTRUM, ALARM,
 * 1750, FM, VOX, NONE) are silently skipped. */
void VUURWERK_SideToast(enum ACTION_OPT_t opt);

#endif
