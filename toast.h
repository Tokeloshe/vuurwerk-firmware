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

#ifndef TOAST_H
#define TOAST_H

#include <stdint.h>

/* On-screen confirmation overlay. The renderer in ui/main.c reads
 * toast_msg / toast_timer directly each frame. While toast_timer > 0
 * the message is drawn centered in an 88x6 box on row 3. The 10ms
 * tick decrement lives in TOAST_Tick(), called once from
 * APP_TimeSlice10ms. No BK4819 register access. */

#define TOAST_MSG_SIZE        16   /* visible row holds ~14 chars at 6px font */
#define TOAST_DURATION_TICKS  100  /* 100 * 10ms = ~1 second */

extern char    toast_msg[TOAST_MSG_SIZE];
extern uint8_t toast_timer;

void TOAST_Show(const char *msg);
void TOAST_Tick(void);

#endif
