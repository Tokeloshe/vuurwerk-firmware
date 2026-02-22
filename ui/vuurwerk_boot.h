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

#ifndef VUURWERK_BOOT_H
#define VUURWERK_BOOT_H

#include <stdbool.h>

void VUURWERK_BOOT_Show(void);
bool VUURWERK_BOOT_IsActive(void);

#endif
