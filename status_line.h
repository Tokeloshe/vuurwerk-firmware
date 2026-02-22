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

#ifndef STATUS_LINE_H
#define STATUS_LINE_H

#include <stdint.h>
#include <stdbool.h>

#define STATUS_LINE_WIDTH 16

typedef enum {
    STATUS_CONTEXT_IDLE = 0,
    STATUS_CONTEXT_RX,
    STATUS_CONTEXT_TX,
    STATUS_CONTEXT_SCAN,
    STATUS_CONTEXT_MENU,
    STATUS_CONTEXT_SPECTRUM,
} StatusContext_t;

typedef struct {
    StatusContext_t context;
    char line[STATUS_LINE_WIDTH + 1];
    bool needs_update;
} StatusLine_t;

extern StatusLine_t gVuurwerkStatusLine;

void STATUS_LINE_Init(void);
void STATUS_LINE_SetContext(StatusContext_t context);
void STATUS_LINE_Update(void);
const char* STATUS_LINE_Get(void);

#endif