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

#ifndef CTCSS_LEAD_H
#define CTCSS_LEAD_H

#include <stdint.h>
#include <stdbool.h>

#define TONE_LEAD_TICKS  15   // 150ms at 10ms/tick

typedef struct {
	bool    active;        // Currently in tone lead phase
	uint8_t countdown;     // Ticks remaining (10ms each)
} CtcssLead_t;

extern CtcssLead_t gCtcssLead;

void CTCSS_LEAD_Init(void);
void CTCSS_LEAD_Start(void);     // Called once at TX start
void CTCSS_LEAD_Process(void);   // Called every 10ms during TX
void CTCSS_LEAD_Stop(void);      // Called at TX end

#endif // CTCSS_LEAD_H