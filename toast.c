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

#include "toast.h"

char    toast_msg[TOAST_MSG_SIZE] = "";
uint8_t toast_timer = 0;

void TOAST_Show(const char *msg)
{
	strncpy(toast_msg, msg, TOAST_MSG_SIZE - 1);
	toast_msg[TOAST_MSG_SIZE - 1] = 0;
	toast_timer = TOAST_DURATION_TICKS;
}

void TOAST_Tick(void)
{
	if (toast_timer > 0)
		toast_timer--;
}
