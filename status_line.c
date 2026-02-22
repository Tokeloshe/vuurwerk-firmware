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

#include "status_line.h"
#include <string.h>

StatusLine_t gVuurwerkStatusLine;

void STATUS_LINE_Init(void) {
    gVuurwerkStatusLine.context = STATUS_CONTEXT_IDLE;
    memset(gVuurwerkStatusLine.line, ' ', STATUS_LINE_WIDTH);
    gVuurwerkStatusLine.line[STATUS_LINE_WIDTH] = '\0';
    gVuurwerkStatusLine.needs_update = true;
}

void STATUS_LINE_SetContext(StatusContext_t context) {
    if (gVuurwerkStatusLine.context != context) {
        gVuurwerkStatusLine.context = context;
        gVuurwerkStatusLine.needs_update = true;
    }
}

void STATUS_LINE_Update(void) {
    if (!gVuurwerkStatusLine.needs_update) return;

    memset(gVuurwerkStatusLine.line, ' ', STATUS_LINE_WIDTH);
    gVuurwerkStatusLine.line[STATUS_LINE_WIDTH] = '\0';

    const char *src;
    switch (gVuurwerkStatusLine.context) {
        case STATUS_CONTEXT_RX:       src = "RX";          break;
        case STATUS_CONTEXT_TX:       src = "TX";          break;
        case STATUS_CONTEXT_SCAN:     src = "SCANNING..."; break;
        case STATUS_CONTEXT_MENU:     src = "MENU";        break;
        case STATUS_CONTEXT_SPECTRUM: src = "SPECTRUM";    break;
        default:                      src = "VUURWERK " VERSION_STRING; break;
    }
    for (uint8_t i = 0; src[i] && i < STATUS_LINE_WIDTH; i++)
        gVuurwerkStatusLine.line[i] = src[i];

    gVuurwerkStatusLine.needs_update = false;
}

const char* STATUS_LINE_Get(void) {
    return gVuurwerkStatusLine.line;
}