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

#ifndef VFO_SPLIT_H
#define VFO_SPLIT_H

#include <stdint.h>
#include <stdbool.h>

// Split operation modes
typedef enum {
	SPLIT_OFF = 0,
	SPLIT_ALERT,
	SPLIT_AUTO_SWITCH,
	SPLIT_LOG_ONLY
} SplitMode_t;

// VUURWERK v1.3.0 (A2): BScanSource_t removed. The menu only ever
// configured B_SCAN_MEMORIES; the FREQ_RANGE and LIST sources had no
// setter anywhere, so their enum, the source field, the range_* fields
// and the dead AdvanceVfoB branches are gone.

// VFO B scan speed
typedef enum {
	B_SPEED_SLOW = 0,
	B_SPEED_NORMAL,
	B_SPEED_FAST
} BScanSpeed_t;

// Internal state machine states
typedef enum {
	SPLIT_STATE_IDLE,
	SPLIT_STATE_HOP_TO_B,
	SPLIT_STATE_SETTLE,
	SPLIT_STATE_READ_RSSI,
	SPLIT_STATE_RETURN_TO_A
} SplitState_t;

// VFO B activity alert
typedef struct {
	bool active;
	uint32_t freq_10Hz;
	uint8_t channel;
	int16_t rssi_dBm;
	uint16_t ctcss_freq_10Hz;
	uint16_t alert_timer_ms;
} BScanAlert_t;

// VFO B scan state
typedef struct {
	SplitMode_t mode;
	BScanSpeed_t speed;

	uint16_t hop_timer_ms;

	uint8_t current_channel;
	uint32_t current_freq_10Hz;
	uint16_t scan_progress;

	BScanAlert_t alert;
} VfoSplit_t;

extern VfoSplit_t gVfoSplit;

// G044: != SPLIT_STATE_IDLE while a hop is in flight (chip away from VFO A).
extern SplitState_t split_state;

// Hop interval durations (milliseconds)
#define HOP_INTERVAL_SLOW    500
#define HOP_INTERVAL_NORMAL  300
#define HOP_INTERVAL_FAST    150

// Timing constants
#define ALERT_DISPLAY_TIME_MS 5000
#define RSSI_THRESHOLD_DBM   -100

// Main process function: call every 10ms tick from main loop.
// ONLY active during FUNCTION_FOREGROUND. Activation is wired through
// direct gVfoSplit writes in app/menu.c MENU_SCANWATCH case.
void VFO_SPLIT_Process(void);

#endif // VFO_SPLIT_H
