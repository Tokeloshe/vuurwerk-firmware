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

// VFO B scan source
typedef enum {
	B_SCAN_MEMORIES = 0,
	B_SCAN_FREQ_RANGE,
	B_SCAN_LIST_1,
	B_SCAN_LIST_2
} BScanSource_t;

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
	// Configuration
	SplitMode_t mode;
	BScanSource_t source;
	BScanSpeed_t speed;
	bool alert_beep;

	// Runtime state
	bool vfo_b_active;
	uint16_t hop_timer_ms;
	uint16_t settle_timer_ms;

	// Scan position
	uint8_t current_channel;
	uint32_t current_freq_10Hz;
	uint16_t scan_progress;

	// Frequency range scan settings
	uint32_t range_start_10Hz;
	uint32_t range_end_10Hz;
	uint32_t range_step_10Hz;

	// Activity tracking
	BScanAlert_t alert;
	uint16_t hits_this_session;
	uint32_t last_hit_freq_10Hz;
	uint16_t last_hit_time_s;

	// State preservation flag
	bool saved_vfo_a_state;
} VfoSplit_t;

extern VfoSplit_t gVfoSplit;

// Hop interval durations (milliseconds)
#define HOP_INTERVAL_SLOW    500
#define HOP_INTERVAL_NORMAL  300
#define HOP_INTERVAL_FAST    150

// Timing constants
#define ALERT_DISPLAY_TIME_MS 5000
#define RSSI_THRESHOLD_DBM   -100

void VFO_SPLIT_Init(void);

// Configuration
void VFO_SPLIT_SetMode(SplitMode_t mode);
void VFO_SPLIT_SetSource(BScanSource_t source);
void VFO_SPLIT_SetSpeed(BScanSpeed_t speed);
void VFO_SPLIT_SetAlertBeep(bool enabled);
void VFO_SPLIT_SetRange(uint32_t start_10Hz, uint32_t end_10Hz, uint32_t step_10Hz);

// Main process function — call every 10ms tick from main loop
// ONLY active during FUNCTION_FOREGROUND
void VFO_SPLIT_Process(void);

// Status queries
void VFO_SPLIT_GetStatus(uint8_t *channel, uint32_t *freq, uint8_t *progress);
BScanAlert_t* VFO_SPLIT_GetAlert(void);
void VFO_SPLIT_ClearAlert(void);
void VFO_SPLIT_SwitchToB(void);
void VFO_SPLIT_Reset(void);
uint16_t VFO_SPLIT_GetHitCount(void);
uint32_t VFO_SPLIT_GetLastHitFreq(void);
uint16_t VFO_SPLIT_GetLastHitTime(void);

#endif // VFO_SPLIT_H