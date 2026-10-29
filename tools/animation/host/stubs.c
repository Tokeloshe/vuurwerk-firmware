/* SPDX-License-Identifier: Apache-2.0
 *
 * Host stand-ins for the hardware drivers, so the real firmware can run on a
 * PC and draw its screens for the animation. The DP32G030 register space is
 * mapped to plain RAM, so gpio.c, backlight.c and friends run unchanged. Only
 * the chips the firmware talks to over a bus are modelled: the ST7565 LCD
 * (captured), the 24C64 EEPROM (an image in memory), the BK4819 radio (a
 * register file plus an "RF environment" that answers RSSI and noise per
 * frequency, and raises squelch interrupts), the keypad, the ADC and time.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "driver/adc.h"
#include "driver/bk4819-regs.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "host.h"

/* ------------------------------------------------------------- MMIO */
void host_map_mmio(void)
{
	void *p = mmap((void *)0x40000000UL, 0x100000, PROT_READ | PROT_WRITE,
	               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (p == MAP_FAILED) {
		perror("mmap 0x40000000");
		exit(1);
	}
	/* the firmware polls a few "ready" bits in the peripheral blocks; with
	 * every register reading as all-ones those polls fall through at once */
	memset(p, 0xFF, 0x100000);
	memset((uint8_t *)p + 0x1000, 0, 0x1000);   /* DMA: nothing received on the UART */
}

/* ------------------------------------------------------------- time */
uint32_t host_ms = 0;
void SYSTEM_DelayMs(uint32_t Delay) { host_advance_ms(Delay); }
void SYSTEM_ConfigureClocks(void) {}

/* ------------------------------------------------------------- LCD */
uint8_t gStatusLine[LCD_WIDTH];
uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];
uint8_t host_lcd[8][LCD_WIDTH];   /* what the panel shows: status line + 7 rows */

void ST7565_BlitFullScreen(void)
{
	memcpy(host_lcd[1], gFrameBuffer, sizeof(gFrameBuffer));
}
void ST7565_BlitLine(unsigned line)
{
	if (line < FRAME_LINES)
		memcpy(host_lcd[1 + line], gFrameBuffer[line], LCD_WIDTH);
}
void ST7565_BlitStatusLine(void) { memcpy(host_lcd[0], gStatusLine, LCD_WIDTH); }
void ST7565_FillScreen(uint8_t Value)
{
	memset(host_lcd, Value, sizeof(host_lcd));
	memset(gFrameBuffer, Value, sizeof(gFrameBuffer));
	memset(gStatusLine, Value, sizeof(gStatusLine));
}
void ST7565_DrawLine(const unsigned int Column, const unsigned int Line, const uint8_t *pBitmap, const unsigned int Size)
{
	if (Line < 8 && Column + Size <= LCD_WIDTH)
		memcpy(&host_lcd[Line][Column], pBitmap, Size);
}
void ST7565_Init(void) {}
void ST7565_FixInterfGlitch(void) {}
void ST7565_HardwareReset(void) {}
void ST7565_SelectColumnAndLine(uint8_t Column, uint8_t Line) { (void)Column; (void)Line; }
void ST7565_WriteByte(uint8_t Value) { (void)Value; }

/* ------------------------------------------------------------ EEPROM */
uint8_t host_eeprom[0x2000];
void EEPROM_ReadBuffer(uint16_t Address, void *pBuffer, uint8_t Size)
{
	if (pBuffer == NULL || Address >= 0x2000)
		return;
	memcpy(pBuffer, host_eeprom + Address, Size);
}
void EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer)
{
	if (pBuffer == NULL || Address >= 0x2000)
		return;
	memcpy(host_eeprom + Address, pBuffer, 8);
}

/* ------------------------------------------------------------ BK4819 */
uint16_t host_bk_reg[128];
uint16_t host_bk_ctcss_code = 0x8000;   /* REG_68: bit15 set = no tone decoded */
uint32_t host_bk_freq = 0;          /* 10 Hz units, as the firmware writes it */
uint16_t host_bk_irq_pending = 0;   /* REG_0C bit 0: an interrupt is waiting */
static uint16_t host_bk_irq_status;  /* REG_02: which, readable once after the clear */
bool host_bk_sql_open = false;

struct host_signal host_signals[HOST_MAX_SIGNALS];
int host_nsignals = 0;

static uint32_t host_bk_settled_at_us;
extern uint32_t host_us_frac;
static uint32_t host_us(void) { return host_ms * 1000u + host_us_frac; }

static const struct host_signal *sig_at(uint32_t f)
{
	for (int i = 0; i < host_nsignals; i++) {
		const struct host_signal *s = &host_signals[i];
		int32_t d = (int32_t)f - (int32_t)s->freq;
		if (d < 0) d = -d;
		if (s->on && (uint32_t)d <= s->half_bw)
			return s;
	}
	return NULL;
}

/* The chip's squelch and tone decoder, once a millisecond: the squelch opens
 * on a signal at the tuned frequency, the CTCSS decoder locks 120 ms later,
 * and each change raises the interrupt the firmware waits for. */
void host_bk_tick(void)
{
	static bool tone_locked = false;
	static uint32_t open_since = 0;
	const struct host_signal *s = sig_at(host_bk_freq);
	/* after a retune the chip needs its settle time plus a few ms of
	 * squelch integration before it can open; the firmware drains stale
	 * interrupts inside that window, as it does on the radio */
	bool open = s != NULL && host_us() >= host_bk_settled_at_us + 4000;
	if (open != host_bk_sql_open) {
		host_bk_sql_open = open;
		if (getenv("HOST_TRACE"))
			fprintf(stderr, "sql ms=%u f=%u open=%d\n", host_ms, host_bk_freq, open);
		host_bk_raise(open ? 1u << 2 : 1u << 3);
		if (open) open_since = host_ms;
		if (!open && tone_locked) { tone_locked = false; host_bk_raise(1u << 7); }
	}
	if (open && s->tone && !tone_locked && host_ms - open_since >= 120) {
		tone_locked = true;
		host_bk_raise(1u << 6);
	}
}

static uint32_t sig_strength(uint32_t f, uint8_t *noise)
{
	/* nearest listed signal within half a channel; else the noise floor */
	for (int i = 0; i < host_nsignals; i++) {
		const struct host_signal *s = &host_signals[i];
		if (!s->on)
			continue;
		int32_t d = (int32_t)f - (int32_t)s->freq;
		if (d < 0) d = -d;
		if ((uint32_t)d <= s->half_bw) {
			*noise = (uint8_t)(s->noise + (host_ms / 7) % 5);
			return s->rssi + (host_ms / 3) % 4;
		}
	}
	/* idle: a band floor around -128 dBm (64 counts) with grass that varies
	 * by frequency and with time, and a noisy, high noise reading */
	uint32_t g = (f / 2500) * 2654435761u;
	*noise = (uint8_t)(140 + (g >> 28) * 5 + (host_ms / 11) % 7);
	return 56 + (g >> 29) * 2 + (host_ms / 5 + (g >> 20)) % 9;
}

/* The front-end gain the firmware asked for (REG_13) takes the same dB off
 * the RSSI the chip reports, as it does on the real BK4819. gain_table[0]
 * is the stock setting the signal strengths in the script are given at. */
#include "am_fix.h"
static int host_gain_loss_db(void)
{
	uint16_t v = host_bk_reg[0x13];
	for (unsigned i = 0; i < gain_table_size; i++)
		if (gain_table[i].reg_val == v)
			return gain_table[0].gain_dB - gain_table[i].gain_dB;
	return 0;
}

void host_bk_update_from_env(void)
{
	uint8_t noise;
	uint32_t rssi = sig_strength(host_bk_freq, &noise);
	int loss = 2 * host_gain_loss_db();           /* RSSI counts are half-dB */
	if (loss > 0) {
		rssi = (uint32_t)loss < rssi ? rssi - (uint32_t)loss : 0;
		if (rssi < 40) rssi = 40;                  /* the chip's own floor */
	}
	host_bk_reg[0x67] = (uint16_t)(rssi & 0x1FF);
	host_bk_reg[0x65] = noise;
	host_bk_reg[0x63] = (uint16_t)(host_bk_sql_open ? 10 : 200);   /* glitch */
	host_bk_reg[0x6F] = (uint16_t)(host_bk_sql_open ? 40 : 0);     /* AF out */
}

/* Timing: every register access is a 3-wire bus transaction of about 20 us,
 * and after a retune the chip's glitch indicator (REG_63) reads 255 for a
 * couple of milliseconds until the PLL and the RSSI settle. That settle is
 * what paces a spectrum sweep on the real radio (GetRssi polls REG_63). */
void SYSTICK_DelayUs(uint32_t Delay);

uint16_t BK4819_ReadRegister(BK4819_REGISTER_t Register)
{
	unsigned r = (unsigned)Register & 0x7F;
	SYSTICK_DelayUs(20);
	if (r == 0x63 && host_us() < host_bk_settled_at_us)
		return 255;
	if (r == 0x67 || r == 0x65 || r == 0x63 || r == 0x6F)
		host_bk_update_from_env();
	if (getenv("HOST_TRACE") && (r == 0x67 || r == 0x65 || r == 0x02 || (r == 0x0C && host_bk_irq_pending)))
		fprintf(stderr, "trace ms=%u f=%u reg%02X=%u\n", host_ms, host_bk_freq, r, host_bk_reg[r]);
	if (r == 0x68)
		return host_bk_ctcss_code;
	if (r == 0x69)
		return 0x8000;   /* no CDCSS */
	if (r == 0x0C) {
		uint16_t v = host_bk_reg[0x0C] & ~1u;
		if (host_bk_irq_pending)
			v |= 1;
		/* squelch and CTCSS/DCS status bits the firmware reads here */
		v &= ~(3u << 14 | 3u << 12 | 3u << 10);
		return v;
	}
	if (r == 0x02) {
		uint16_t v = host_bk_irq_status;
		host_bk_irq_status = 0;
		if (getenv("HOST_TRACE")) fprintf(stderr, "irq ms=%u read02=%04x\n", host_ms, v);
		return v;
	}
	return host_bk_reg[r];
}

void BK4819_WriteRegister(BK4819_REGISTER_t Register, uint16_t Data)
{
	unsigned r = (unsigned)Register & 0x7F;
	SYSTICK_DelayUs(20);
	host_bk_reg[r] = Data;
	if (r == 0x38 || r == 0x39 || r == 0x30)
		host_bk_settled_at_us = host_us() + 2500;
	if (r == 0x3F) {
		/* the interrupt mask is rewritten right after the firmware drains
		 * old requests on a reconfiguration; anything latched before that
		 * belonged to the previous setup */
		host_bk_irq_status = 0;
		host_bk_irq_pending = 0;
	}
	if (r == 0x02) {
		host_bk_irq_pending = 0;
		/* the write acknowledges the request; the status bits stay until read
		 * (the firmware writes 0 and then reads the status, in that order) */
		if (getenv("HOST_TRACE")) fprintf(stderr, "irq ms=%u write02\n", host_ms);
	}
	if (r == 0x38 || r == 0x39)
		host_bk_freq = ((uint32_t)host_bk_reg[0x39] << 16) | host_bk_reg[0x38];
}

void host_bk_raise(uint16_t bits) { host_bk_irq_status |= bits; host_bk_irq_pending = 1; if (getenv("HOST_TRACE")) fprintf(stderr, "irq ms=%u raise %04x\n", host_ms, bits); }

/* ---------------------------------------------------------- keyboard */
KEY_Code_t host_key = KEY_INVALID;
KEY_Code_t gKeyReading0     = KEY_INVALID;
KEY_Code_t gKeyReading1     = KEY_INVALID;
uint16_t   gDebounceCounter = 0;
bool       gWasFKeyPressed  = false;
void SYSTICK_DelayUs(uint32_t Delay);
/* a real poll takes a few tens of microseconds; the boot screen loop spins on
 * this call alone, so time has to move here too */
KEY_Code_t KEYBOARD_Poll(void) { SYSTICK_DelayUs(50); return host_key; }

/* --------------------------------------------------------------- ADC */
uint16_t host_adc_batt = 2050;   /* raw; the calibration divisor turns it into volts */
uint8_t ADC_GetChannelNumber(ADC_CH_MASK Mask) { (void)Mask; return 0; }
void ADC_Disable(void) {}
void ADC_Enable(void) {}
void ADC_SoftReset(void) {}
uint32_t ADC_GetClockConfig(void) { return 0; }
void ADC_Configure(ADC_Config_t *pAdc) { (void)pAdc; }
void ADC_Start(void) {}
bool ADC_CheckEndOfConversion(ADC_CH_MASK Mask) { (void)Mask; return true; }
uint16_t ADC_GetValue(ADC_CH_MASK Mask) { return Mask == ADC_CH4 ? host_adc_batt : 0; }

/* ------------------------------------------------------- the others */
void SPI0_Init(void) {}
void SPI_WaitForUndocumentedTxFifoStatusBit(void) {}
void SPI_Disable(volatile uint32_t *pCR) { (void)pCR; }
void SPI_Enable(volatile uint32_t *pCR) { (void)pCR; }
void SPI_ToggleMasterMode(volatile uint32_t *pCr, bool bIsMaster) { (void)pCr; (void)bIsMaster; }
void UART_Init(void) {}
void UART_Send(const void *pBuffer, uint32_t Size) { (void)pBuffer; (void)Size; }
void UART_LogSend(const void *pBuffer, uint32_t Size) { (void)pBuffer; (void)Size; }
void CRC_Init(void) {}
uint16_t CRC_Calculate(const void *pBuffer, uint16_t Size) { (void)pBuffer; (void)Size; return 0; }
void I2C_Start(void) {}
void I2C_Stop(void) {}
uint8_t I2C_Read(bool bFinal) { (void)bFinal; return 0xFF; }
int I2C_Write(uint8_t Data) { (void)Data; return 0; }
int I2C_ReadBuffer(void *pBuffer, uint8_t Size) { memset(pBuffer, 0xFF, Size); return 0; }
int I2C_WriteBuffer(const void *pBuffer, uint8_t Size) { (void)pBuffer; (void)Size; return 0; }
void AES_Encrypt(const void *pKey, const void *pIv, const void *pIn, void *pOut, uint8_t NumBlocks)
{ (void)pKey; (void)pIv; memcpy(pOut, pIn, (size_t)NumBlocks * 16); }

/* ----------------------------------------------------------- systick */
void SYSTICK_Init(void) {}
uint32_t host_us_frac;
void SYSTICK_DelayUs(uint32_t Delay)
{
	host_us_frac += Delay;
	uint32_t ms = host_us_frac / 1000;
	host_us_frac %= 1000;
	if (ms)
		host_advance_ms(ms);
}

/* -------------------------------------------------- UART DMA, flash */
uint8_t UART_DMA_Buffer[256];
void FLASH_Init(int ReadMode) { (void)ReadMode; }
void FLASH_ConfigureTrimValues(void) {}
uint32_t FLASH_ReadNvrWord(uint32_t Address) { (void)Address; return 0; }
uint32_t overlay_FLASH_MainClock, overlay_FLASH_ClockMultiplier;
