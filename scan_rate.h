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

#ifndef SCAN_RATE_H
#define SCAN_RATE_H

#include <stdint.h>

void    SCAN_RATE_NoteStep(void);
void    SCAN_RATE_Tick10ms(void);
uint8_t SCAN_RATE_ChannelsPerSec(void);
void    SCAN_RATE_Reset(void);

#endif
