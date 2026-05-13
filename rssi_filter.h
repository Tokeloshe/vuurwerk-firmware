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

#ifndef RSSI_FILTER_H
#define RSSI_FILTER_H

#include <stdint.h>

void RSSI_FILTER_Init(void);
int16_t RSSI_FILTER_Update(uint8_t vfo, int16_t raw_rssi);

#endif
