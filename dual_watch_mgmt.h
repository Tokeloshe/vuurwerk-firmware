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

#ifndef DUAL_WATCH_MGMT_H
#define DUAL_WATCH_MGMT_H

#include <stdint.h>

void     DUAL_WATCH_MGMT_Init(void);
uint16_t DUAL_WATCH_MGMT_GetDwellTime(uint8_t vfo);
// weight is the RX duration in 10ms ticks (1..255, clamped); a
// single-tick blip still registers as one unit so weight=0 is
// promoted to weight=1 internally.
void     DUAL_WATCH_MGMT_ReportActivity(uint8_t vfo, uint16_t weight);

#endif
