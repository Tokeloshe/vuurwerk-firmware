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

#ifndef BANDSCOPE_H
#define BANDSCOPE_H

#include <stdint.h>
#include <stdbool.h>

void BANDSCOPE_Init(void);
void BANDSCOPE_SetEnabled(bool enabled);
bool BANDSCOPE_IsEnabled(void);
void BANDSCOPE_Process(void);
void BANDSCOPE_RecordHop(uint32_t freq_10Hz, uint8_t rssi_level);
void BANDSCOPE_SetNoiseFloor(uint8_t level);
void BANDSCOPE_Render(uint8_t *framebuffer_line);

#endif // BANDSCOPE_H