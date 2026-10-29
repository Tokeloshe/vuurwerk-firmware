/* SPDX-License-Identifier: Apache-2.0
 *
 * Runs the real firmware's Main() on the PC. main.c is compiled with
 * -DAPP_Update=host_loop, so every pass of the firmware's main loop first
 * comes through here: we advance simulated time, read the next command from
 * the script on stdin, and then hand over to the real APP_Update().
 *
 * Script (one command per line):
 *   wait MS                    let the firmware run for MS milliseconds
 *   key NAME [HOLD_MS]         press a key: MENU UP DOWN EXIT STAR F 0-9 SIDE1 SIDE2
 *   ptt down|up                the PTT line
 *   signal F_HZ RSSI NOISE HALFBW_HZ [tone]   add (or update) a signal on the air; the
 *                              squelch and tone decoder follow it by themselves
 *   off F_HZ                   switch that signal off
 *   sql open|close             force the squelch interrupt (normally automatic)
 *   tone on|off                force the tone decoder (normally automatic)
 *   ctcss HZx10|none           the tone the tone scanner finds
 *   batt RAW                   ADC reading for the battery
 *   ee ADDR HEXBYTES           patch the EEPROM image before boot
 *   frame NAME                 print the LCD as 8 rows of 128 hex bytes
 *   quit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "host.h"
#include "misc.h"
#include "functions.h"
#include "app/chFrScanner.h"
extern uint16_t host_bk_irq_pending;
extern bool gRxIdleMode;

void SystickHandler(void);
void Main(void);

static uint32_t key_release_at = 0;
static uint32_t wait_until = 0;
static bool booted = false;

static bool run_command(void);

/* Simulated time moves here, and only here. The script is read as time
 * reaches each "wait", so it works the same inside the spectrum's own loop
 * (which never returns to the main loop) as on the main screen. */
void host_advance_ms(uint32_t ms)
{
	static bool in_script = false;
	while (ms--) {
		host_ms++;
		if (host_ms % 10 == 0)
			SystickHandler();
		host_bk_tick();
		if (host_key != KEY_INVALID && host_ms >= key_release_at)
			host_key = KEY_INVALID;
		if (host_ms >= wait_until && !in_script) {
			in_script = true;
			if (!run_command())
				exit(0);
			in_script = false;
		}
	}
}

static KEY_Code_t key_by_name(const char *s)
{
	static const struct { const char *n; KEY_Code_t k; } T[] = {
		{"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4}, {"5", KEY_5},
		{"6", KEY_6}, {"7", KEY_7}, {"8", KEY_8}, {"9", KEY_9}, {"MENU", KEY_MENU}, {"UP", KEY_UP},
		{"DOWN", KEY_DOWN}, {"EXIT", KEY_EXIT}, {"STAR", KEY_STAR}, {"F", KEY_F},
		{"SIDE1", KEY_SIDE1}, {"SIDE2", KEY_SIDE2}, {"PTT", KEY_PTT},
	};
	for (unsigned i = 0; i < sizeof(T) / sizeof(T[0]); i++)
		if (!strcmp(T[i].n, s))
			return T[i].k;
	fprintf(stderr, "unknown key %s\n", s);
	exit(2);
}

static void dump_frame(const char *name)
{
	printf("frame %s %u\n", name, host_ms);
	for (int r = 0; r < 8; r++) {
		for (int c = 0; c < 128; c++)
			printf("%02x", host_lcd[r][c]);
		printf("\n");
	}
	fflush(stdout);
}

static void set_signal(uint32_t f, uint16_t rssi, uint8_t noise, uint32_t halfbw, bool on, bool tone)
{
	for (int i = 0; i < host_nsignals; i++)
		if (host_signals[i].freq == f) {
			host_signals[i].rssi = rssi; host_signals[i].noise = noise;
			host_signals[i].half_bw = halfbw; host_signals[i].on = on; host_signals[i].tone = tone;
			return;
		}
	if (host_nsignals < HOST_MAX_SIGNALS) {
		host_signals[host_nsignals++] = (struct host_signal){f, halfbw, rssi, noise, on, tone};
	}
}

/* returns false at end of script */
static bool run_command(void)
{
	static char line[4096];
	for (;;) {
		if (!fgets(line, sizeof line, stdin))
			return false;
		char cmd[32] = "", a[1024] = "", b[1024] = "", c[64] = "", d[64] = "", e[64] = "";
		int n = sscanf(line, "%31s %1023s %1023s %63s %63s %63s", cmd, a, b, c, d, e);
		if (n < 1 || cmd[0] == '#')
			continue;
		if (!strcmp(cmd, "wait")) {
			wait_until = host_ms + (uint32_t)strtoul(a, NULL, 10);
			return true;
		}
		if (!strcmp(cmd, "key")) {
			host_key = key_by_name(a);
			key_release_at = host_ms + (n > 2 ? (uint32_t)strtoul(b, NULL, 10) : 150);
			continue;
		}
		if (!strcmp(cmd, "ptt")) {
			if (!strcmp(a, "down"))
				GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_PTT);
			else
				GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_PTT);
			continue;
		}
		if (!strcmp(cmd, "signal")) {
			set_signal((uint32_t)(strtoull(a, NULL, 10) / 10), (uint16_t)atoi(b), (uint8_t)atoi(c),
			           (uint32_t)(strtoull(d, NULL, 10) / 10), true, n > 5 && !strcmp(e, "tone"));
			continue;
		}
		if (!strcmp(cmd, "off")) {
			set_signal((uint32_t)(strtoull(a, NULL, 10) / 10), 0, 0, 0, false, false);
			continue;
		}
		if (!strcmp(cmd, "sql")) {
			/* BK4819 naming: SQUELCH_LOST (bit 2) = the squelch opened, a
			 * signal is there; SQUELCH_FOUND (bit 3) = it closed again */
			if (!strcmp(a, "open")) { host_bk_sql_open = true;  host_bk_raise(1u << 2); }
			else                    { host_bk_sql_open = false; host_bk_raise(1u << 3); }
			continue;
		}
		if (!strcmp(cmd, "tone")) {
			/* CTCSS_LOST (bit 6) = a tone is being decoded; CTCSS_FOUND (bit 7) = it went away */
			host_bk_raise(!strcmp(a, "on") ? 1u << 6 : 1u << 7);
			continue;
		}
		if (!strcmp(cmd, "ctcss")) {
			/* the tone the BK4819's tone scanner reports: "ctcss 1000" = 100.0 Hz, "ctcss none" */
			if (!strcmp(a, "none")) host_bk_ctcss_code = 0x8000;
			else host_bk_ctcss_code = (uint16_t)(((uint32_t)atoi(a) * 10000 + 4842) / 4843) & 0x1FFF;
			continue;
		}
		if (!strcmp(cmd, "batt")) {
			host_adc_batt = (uint16_t)atoi(a);
			continue;
		}
		if (!strcmp(cmd, "ee")) {
			unsigned addr = (unsigned)strtoul(a, NULL, 0);
			for (size_t i = 0; i + 1 < strlen(b) && addr + i / 2 < 0x2000; i += 2) {
				char h[3] = {b[i], b[i + 1], 0};
				host_eeprom[addr + i / 2] = (uint8_t)strtoul(h, NULL, 16);
			}
			continue;
		}
		if (!strcmp(cmd, "frame")) {
			dump_frame(a);
			continue;
		}
		if (!strcmp(cmd, "dump")) {
			fprintf(stderr, "dump ms=%u fn=%d sqlLost=%d rxIdle=%d irq=%04x bkfreq=%u rxmode=%d scandir=%d pause=%u listen=%d reduced=%d\n", host_ms,
			        gCurrentFunction, g_SquelchLost, gRxIdleMode, host_bk_irq_pending, host_bk_freq,
			        gRxReceptionMode, gScanStateDir, gScanPauseDelayIn_10ms, gScheduleScanListen, gReducedService);
			continue;
		}
		if (!strcmp(cmd, "quit"))
			return false;
		fprintf(stderr, "unknown command: %s", line);
		exit(2);
	}
}

/* called in place of APP_Update() from the firmware's main loop */
void host_loop(void)
{
	booted = true;
	host_advance_ms(1);
	APP_Update();
}

int main(void)
{
	host_map_mmio();
	memset(host_eeprom, 0xFF, sizeof host_eeprom);
	/* the script's leading ee / batt / signal lines run now, before the
	 * firmware reads its settings; the first "wait" hands control to Main */
	host_advance_ms(1);
	Main();   /* never returns; host_loop() ends the process at "quit" or EOF */
	return 0;
}
