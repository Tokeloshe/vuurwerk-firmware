/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HOST_H
#define HOST_H
#include <stdbool.h>
#include <stdint.h>
#include "driver/keyboard.h"

#define HOST_MAX_SIGNALS 32

struct host_signal {
	uint32_t freq;      /* 10 Hz units */
	uint32_t half_bw;   /* 10 Hz units: how far off-frequency it still reads */
	uint16_t rssi;      /* BK4819 REG_67 counts (about 2 per dB above -160) */
	uint8_t  noise;     /* REG_65: low = clean voice, high = carrier or noise */
	bool     on;
	bool     tone;      /* carries the CTCSS tone the radio is listening for */
};

extern uint8_t  host_lcd[8][128];
extern uint8_t  host_eeprom[0x2000];
extern uint32_t host_ms;
extern uint16_t host_adc_batt;
extern KEY_Code_t host_key;
extern struct host_signal host_signals[HOST_MAX_SIGNALS];
extern int host_nsignals;
extern bool host_bk_sql_open;
extern uint16_t host_bk_ctcss_code;
extern uint32_t host_bk_freq;

void host_map_mmio(void);
void host_advance_ms(uint32_t ms);
void host_bk_raise(uint16_t bits);
void host_bk_tick(void);
#endif
