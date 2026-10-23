/* Copyright 2023 fagci
 * https://github.com/fagci
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */
#include "app/spectrum.h"
#include "am_fix.h"
#include "audio.h"
#include "audio_palette.h"
#include "misc.h"

#ifdef ENABLE_SCAN_RANGES
#include "chFrScanner.h"
#endif

#include "driver/backlight.h"
#include "driver/eeprom.h"
#include "flashlight_watchdog.h"   // VUURWERK: G086
#include "frequencies.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "ui/menu.h"      // v1.5.1 (F1): gSubMenu_BACKLIGHT

struct FrequencyBandInfo {
  uint32_t lower;
  uint32_t upper;
  uint32_t middle;
};

#define F_MIN frequencyBandTable[0].lower
#define F_MAX frequencyBandTable[BAND_N_ELEM - 1].upper

const uint16_t RSSI_MAX_VALUE = 65535;

static uint32_t initialFreq;
static char String[32];

bool isInitialized = false;
bool isListening = true;
bool monitorMode = false;
bool redrawStatus = true;
bool redrawScreen = false;
bool newScanStart = true;
bool preventKeypress = true;
bool audioState = true;

State currentState = SPECTRUM, previousState = SPECTRUM;

PeakInfo peak;
ScanInfo scanInfo;
KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0};

#ifdef ENABLE_SCAN_RANGES
static uint16_t blacklistFreqs[15];
static uint8_t blacklistFreqsIdx;
#endif

const char *bwOptions[] = {"  25k", "12.5k", "6.25k"};
const uint8_t modulationTypeTuneSteps[] = {100, 50, 10};
const uint8_t modTypeReg47Values[] = {1, 7, 5};

#define SPECTRUM_TRIGGER_DEFAULT 150   // v1.5.1 (F6): shared by the initializer and PatrolStart

SpectrumSettings settings = {.stepsCount = STEPS_64,
                             .scanStepIndex = S_STEP_25_0kHz,
                             .frequencyChangeStep = 80000,
                             .scanDelay = 3200,
                             .rssiTriggerLevel = SPECTRUM_TRIGGER_DEFAULT,
                             .backlightState = true,
                             .bw = BK4819_FILTER_BW_WIDE,
                             .listenBw = BK4819_FILTER_BW_WIDE,
                             .modulationType = false,
                             .dbMin = -130,
                             .dbMax = -50};

uint32_t fMeasure = 0;
uint32_t currentFreq, tempFreq;
uint16_t rssiHistory[128];

// Spectrum display modes: 0=NORMAL, 1=PEAK, 2=VOICE (VOX)  [MTI removed v1.4.0]
static uint8_t peak_hold[128];
static uint8_t spectrum_mode = 0;   // v1.4.0: 0 NORM, 1 PEAK, 2 VOX (MTI removed)
static uint8_t sweep_count = 0;

// VOIC mode: noise capture and voice probability per bin
static uint8_t noiseHistory[128];
static uint8_t voiceProb[128];

// v1.3.3 VOX (mode 3) hold-rule tuning (bench-adjustable):
#define VOX_RECHECK_MS    300   // re-check cadence (ms) while locked in VOX
#define VOX_BAD_RELEASE     2   // drop after N consecutive non-voice checks (~0.6s; rides 1 flutter)
static uint8_t s_voxBad;        // consecutive non-voice checks while locked

// ===================== VUURWERK v1.5.1 PATROL tuning ======================
// Every empirical patrol number lives here and nowhere else. All of it is
// bench-tunable. None of these numbers is ever shown to the operator; the
// menu only ever says Low / Normal / High, Slow / Normal, Short / Normal /
// Long.
//
// Sensitivity is a pair per setting: the voice-score threshold the sweep
// gates on, and the RSSI trigger that decides which bins are candidates at
// all. Under the trigger every candidate already earns the scorer's signal
// points, so the only scores that occur are 95 or 80 (quiet demod), 85 or
// 70 (moderate), 55 (noisy) and 35 (rejected by the hold rule anyway). Low
// raises the score bar above the noisy tier; High cannot usefully lower it,
// so High reaches deeper instead by lowering the trigger about 7 dB. Normal
// is the literal hold-to-sweep has always used, both numbers.
static const uint8_t  patrolSensThreshold[3] = { 60, 50, 50 };                     // Low, Normal, High
static const uint16_t patrolSensTrigger[3]   = { SPECTRUM_TRIGGER_DEFAULT,
                                                 SPECTRUM_TRIGGER_DEFAULT, 136 };   // Low, Normal, High
// Speed: Slow adds a settle pause between tuning and measuring each bin
// while patrol is actively sweeping (not camped on a hit). Normal adds
// nothing, the native rate. VUURWERK: G007 the pause used to follow the read.
#define PATROL_SLOW_SETTLE_US  1000
// Dwell: the guaranteed listen after a hit before the first recheck. Normal
// reproduces the stock 1000 ms exactly. Read in ToggleRX below.
static const uint16_t patrolHoldMs[3]        = { 500, 1000, 2000 };                // Short, Normal, Long
// Resume: consecutive non-voice rechecks before a camp is released. Normal
// reproduces VOX_BAD_RELEASE exactly.
static const uint8_t  patrolResumeBad[3]     = { 1, VOX_BAD_RELEASE, 4 };          // Short, Normal, Long
// Dwell cap: hard ceiling on one automatic camp, whatever the settings say.
// This is the mandatory unattended safeguard: one carrier that fools the
// scorer must never end the patrol.
#define PATROL_DWELL_CAP_S     60
// A capped frequency is skipped for this many half-seconds of sweeping
// (2 minutes), then it may be visited again: a real conversation that ran
// long comes back, a birdie costs one dwell every few minutes.
#define PATROL_CAP_SKIP_HALF_S 240   // VUURWERK: G003 time, not laps; fits the uint8 count
#define PATROL_CAP_SLOTS       8
#define PATROL_CAP_MENU        0xFF   // VUURWERK: G005 a MENU skip, never ages during the run
// Experimental carrier rejection. OFF by default, not a feature, not
// documented, not promised: during a patrol hold, read the glitch indicator
// on every recheck; an unmodulated carrier holds it dead still while voice
// keeps it moving. Set to 1 to bench it.
#define PATROL_CARRIER_STILL   0
#define PATROL_STILL_GLITCH    2
#define PATROL_STILL_CHECKS    10                                                  // consecutive still rechecks, about 3 s at 300 ms
#if PATROL_CARRIER_STILL
static uint8_t s_stillChecks;   // consecutive dead-still rechecks while camped
#endif
// ================== end VUURWERK v1.5.1 PATROL tuning ======================

// v1.3.6: VOX LOCK -- park on the voice you're hearing (PTT), release with UP/DOWN.
static bool s_voxLock = false;     // pinned to the current voice; arms exit carry-over + LOCKED toast

// v1.2.6 spectrum visual polish state (v1.2.7 dropped the SIDE2
// release/long-press state machine; SIDE2 now mirrors SIDE1).
static uint8_t s_mode_toast_renders = 0;
static const char *specModeNames[] = {"NORM", "PEAK", "VOX "};
static const char *s_toastText = "";   // v1.5.1: what the brief toast shows

// v1.5.1 PATROL: battery percent, captured from DrawStatus()'s existing
// calibrated calculation (see DrawStatus below) so the low-battery
// safeguard reads the exact same number the status bar shows, instead of
// duplicating the calibration formula or trusting gBatteryDisplayLevel --
// which the main scheduler's 10ms battery task refreshes, and that task
// does not run while spectrum.c holds the CPU in its own loop.
static uint8_t s_lastBatteryPercent = 100;
static uint8_t PatrolVoiceThreshold(void);   // defined with the patrol block below

int vfo;
uint8_t freqInputIndex = 0;
uint8_t freqInputDotIndex = 0;
KEY_Code_t freqInputArr[10];
char freqInputString[11];

uint16_t listenT = 0;

uint16_t statuslineUpdateTimer = 0;

// Forward declarations
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax);

static uint8_t DBm2S(int dbm) {
  uint8_t i = 0;
  dbm *= -1;
  for (i = 0; i < ARRAY_SIZE(U8RssiMap); i++) {
    if (dbm >= U8RssiMap[i]) {
      return i;
    }
  }
  return i;
}

static int Rssi2DBm(uint16_t rssi) {
  return (rssi / 2) - 160 + dBmCorrTable[gRxVfo->Band];
}

// Voice probability from single-shot RSSI + noise indicator
static uint8_t ComputeVoiceProb(uint16_t rssi, uint8_t noise)
{
  if (rssi < 50 || noise > 200) return 0;

  int16_t s = 0;
  // v1.3.3: middle noise tier so a readable moderate FM voice (noise 30-49)
  // can reach the 50 hop/listen threshold instead of being stuck at 35.
  if      (noise < 30) s += 45;
  else if (noise < 50) s += 35;
  else if (noise < 70) s += 20;
  if      (rssi > 100) s += 35;
  else if (rssi > 65)  s += 15;
  if (rssi > 80 && noise < 40) s += 15;
  return (s > 95) ? 95 : (uint8_t)s;
}

// GUI functions

static void PutPixel(uint8_t x, uint8_t y, bool fill) {
  UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
}
// VUURWERK v1.3.1 (U3): PutPixelStatus removed with GUI_DisplaySmallest's
// relocation to ui/helper.c (it was that function's only caller).

static void DrawVLine(int sy, int ey, int nx, bool fill) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < 128) {
      PutPixel(nx, i, fill);
    }
  }
}

// VUURWERK v1.3.1 (U3): body relocated verbatim to ui/helper.c as
// UI_DisplaySmallest so the menu hint line can share it; alias keeps every
// call site below diff-free.
#define GUI_DisplaySmallest UI_DisplaySmallest

// Utility functions

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

// VUURWERK: G023 wait until every keypad key is up (PTT is not a keypad key).
// Entry: the key that launched us; exit: the terminal EXIT press.
static void WaitKeysReleased(void) {
  while (KEYBOARD_Poll() != KEY_INVALID)
    SYSTEM_DelayMs(10);
}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

void SetState(State state) {
  previousState = currentState;
  currentState = state;
  redrawScreen = true;
  redrawStatus = true;
}

// Radio functions

static void ToggleAFBit(bool on) {
  // VUURWERK v1.3.1 (C1): RMW helper
  BK4819_UpdateRegisterBits(BK4819_REG_47, 1u << 8, (uint16_t)on << 8);
}

static const BK4819_REGISTER_t registers_to_save[] ={
  BK4819_REG_30,
  BK4819_REG_37,
  BK4819_REG_3D,
  BK4819_REG_43,
  BK4819_REG_47,
  BK4819_REG_48,
  BK4819_REG_7E,
};

static uint16_t registers_stack [sizeof(registers_to_save)];

static void BackupRegisters() {
  for (uint32_t i = 0; i < ARRAY_SIZE(registers_to_save); i++){
    registers_stack[i] = BK4819_ReadRegister(registers_to_save[i]);
  }
}

static void RestoreRegisters() {

  for (uint32_t i = 0; i < ARRAY_SIZE(registers_to_save); i++){
    BK4819_WriteRegister(registers_to_save[i], registers_stack[i]);
  }
}

// VUURWERK v1.3.1 (T2): REG_30 cache for the sweep hot path. SetF() used to
// read REG_30 on every step only to pulse it (write 0, write back), but
// REG_30 never changes mid-sweep -- only ToggleAFDAC and mode init touch it.
// Cache it and re-read only when marked dirty: 2 of ~5 bus transactions per
// sweep step disappear, with byte-identical register traffic semantics.
static uint16_t s_reg30_cache;
static bool s_reg30_dirty = true;

static void ToggleAFDAC(bool on) {
  // VUURWERK v1.3.1 (C1): RMW helper
  BK4819_UpdateRegisterBits(BK4819_REG_30, 1u << 9, (uint16_t)on << 9);
  s_reg30_dirty = true;  // VUURWERK v1.3.1 (T2)
}

static void SetF(uint32_t f) {
  // VUURWERK: G098 the LNA path only flips at 280 MHz: re-pick on a dirty cache or when
  // f and fMeasure sit on opposite sides (the signs of x - 28000000 differ)
  if (s_reg30_dirty || (int32_t)((f - 28000000) ^ (fMeasure - 28000000)) < 0)
    BK4819_PickRXFilterPathBasedOnFrequency(f);
  fMeasure = f;

  BK4819_SetFrequency(fMeasure);
  // VUURWERK v1.3.1 (T2): cached REG_30 pulse (see s_reg30_cache above)
  if (s_reg30_dirty) {
    s_reg30_cache = BK4819_ReadRegister(BK4819_REG_30);
    s_reg30_dirty = false;
  }
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, s_reg30_cache);
}

// Spectrum related

bool IsPeakOverLevel() { return peak.rssi >= settings.rssiTriggerLevel; }

static void ResetPeak() {
  peak.t = 0;
  peak.rssi = 0;
}

bool IsCenterMode() { return settings.scanStepIndex < S_STEP_2_5kHz; }
// scan step in 0.01khz
uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }

uint16_t GetStepsCount()
{
  // VUURWERK: G101 dead scan-range branch removed, so this never exceeds 128
  return 128 >> settings.stepsCount;
}

uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }
uint32_t GetFStart() {
  return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq;
}

uint32_t GetFEnd() { return GetFStart() + GetBW(); }   // VUURWERK: G108 right edge in centre mode too

static void TuneToPeak() {
  scanInfo.f = peak.f;
  scanInfo.rssi = peak.rssi;
  scanInfo.i = peak.i;
  SetF(scanInfo.f);
}

// VUURWERK: G001 EXIT and the PATROL low-battery stop only end the session.
// The whole hand-off (carry-over, restore, resync) runs once at the tail of
// APP_RunSpectrum, after the key release wait. Safe to call inside Tick().
static void DeInitSpectrum() {
  isInitialized = false;
}

uint16_t GetBWRegValueForScan() {
  return scanStepBWRegValues[settings.scanStepIndex];
}

uint16_t GetRssi() {
  uint8_t timeout = 50;
  while ((BK4819_ReadRegister(0x63) & 0b11111111) >= 255) {
    if (--timeout == 0)
      break;
    SYSTICK_DelayUs(100);
  }
  uint16_t rssi = BK4819_GetRSSI();
#ifdef ENABLE_AM_FIX
  if(settings.modulationType==MODULATION_AM && gSetting_AM_fix)
    rssi += AM_fix_get_gain_diff()*2;
#endif
  return rssi;
}

static void ToggleAudio(bool on) {
  if (on == audioState) {
    return;
  }
  audioState = on;
  if (on) {
    AUDIO_AudioPathOn();
  } else {
    AUDIO_AudioPathOff();
  }
}

static void ToggleRX(bool on) {
  isListening = on;

  RADIO_SetupAGC(on, false);
  BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, on);

  ToggleAudio(on);
  ToggleAFDAC(on);
  ToggleAFBit(on);

  if (on) {
    listenT = patrolActive ? patrolHoldMs[patrolSettings.dwell] : 1000;   // v1.5.1 PATROL Dwell
    s_voxBad = 0;   // v1.3.3: fresh VOX hold-rule counter per lock
#if PATROL_CARRIER_STILL
    s_stillChecks = 0;
#endif
    BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
  } else {
    BK4819_WriteRegister(0x43, GetBWRegValueForScan());
    s_voxLock = false;   // v1.3.6: a real RX-off (relaunch/blacklist/freq-input) releases the pin
  }
}

// Scan info

static void ResetScanStats() {
  scanInfo.rssi = 0;
  scanInfo.rssiMax = 0;
  scanInfo.iPeak = 0;
  scanInfo.fPeak = 0;
}

static uint32_t scanFStart;  // cached for VoiceHop

static void InitScan() {
  ResetScanStats();
  scanInfo.i = 0;
  scanFStart = scanInfo.f = GetFStart();

  scanInfo.scanStep = GetScanStep();
  scanInfo.measurementsCount = GetStepsCount();
}

// v1.3.3: lock onto voice bin vi (shared by the manual hop and the sweep-end
// auto-hop). Clears audioState so ToggleRX physically reopens the AF path even
// when chaining from an active listen -- AUDIO_PALETTE_PlayFoundVoice() mutes
// the path without touching the flag, which would otherwise lock in silence.
static void LockVoiceBin(uint8_t vi) {
  peak.i     = vi;
  peak.t     = 0;
  peak.f     = scanFStart + (uint32_t)vi * scanInfo.scanStep;
  peak.rssi  = rssiHistory[vi];
  audioState = false;
  ToggleRX(true);
  TuneToPeak();
}

// v1.3.3: the VOX voice arrays (voiceProb/noiseHistory, 128 entries) map 1:1
// to the display bins. VUURWERK: G101 every span is <= 128 steps now, so the
// voice gate/store/hop only needs the VOX mode test.
static bool VoiceModeActive(void) {
  return spectrum_mode == 2;
}

static void ResetBlacklist() {
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == RSSI_MAX_VALUE)
      rssiHistory[i] = 0;
  }
#ifdef ENABLE_SCAN_RANGES
  // v1.3.3: fill with 0xFFFF (never a valid bin index), not 0 -- the old 0-fill
  // made IsBlacklisted(0) always true, so bin 0 was never scanned or hop-able.
  memset(blacklistFreqs, 0xFF, sizeof(blacklistFreqs));
  blacklistFreqsIdx = 0;
#endif
}

static void RelaunchScan() {
  InitScan();
  ResetPeak();
  ToggleRX(false);
#ifdef SPECTRUM_AUTOMATIC_SQUELCH
  settings.rssiTriggerLevel = RSSI_MAX_VALUE;
#endif
  preventKeypress = true;
  scanInfo.rssiMin = RSSI_MAX_VALUE;
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
    scanInfo.fPeak = scanInfo.f;
    scanInfo.iPeak = scanInfo.i;
  }

  if (scanInfo.rssi < scanInfo.rssiMin) {
    scanInfo.rssiMin = scanInfo.rssi;
    settings.dbMin = Rssi2DBm(scanInfo.rssiMin);
    redrawStatus = true;
  }
}

static void AutoTriggerLevel() {
  if (settings.rssiTriggerLevel == RSSI_MAX_VALUE) {
    settings.rssiTriggerLevel = clamp(scanInfo.rssiMax + 8, 0, RSSI_MAX_VALUE);
  }
}

static void UpdatePeakInfoForce() {
  peak.t = 0;
  peak.rssi = scanInfo.rssiMax;
  peak.f = scanInfo.fPeak;
  peak.i = scanInfo.iPeak;
  AutoTriggerLevel();
}

static void UpdatePeakInfo() {
  if (peak.f == 0 || peak.t >= 1024 || peak.rssi < scanInfo.rssiMax)
    UpdatePeakInfoForce();
}

static void SetRssiHistory(uint16_t idx, uint16_t rssi)
{
  // VUURWERK: G101 dead >128-step compression removed
  rssiHistory[idx] = rssi;
}

static void Measure()
{
  uint16_t rssi = scanInfo.rssi = GetRssi();
  SetRssiHistory(scanInfo.i, rssi);

  // Update spectrum mode buffers
  if (scanInfo.i < 128) {
    uint8_t val = Rssi2PX(rssi, 0, DrawingEndY);   // VUURWERK: G018 same scale as the bars
    if (val > peak_hold[scanInfo.i])
      peak_hold[scanInfo.i] = val;

    // VOIC mode: capture noise indicator (already settled from RSSI delay)
    if (spectrum_mode == 2)
      noiseHistory[scanInfo.i] = BK4819_ReadRegister(0x65) & 0xFF;
  }
}

// Update things by keypress

static uint16_t dbm2rssi(int dBm) {
  return (dBm + 160 - dBmCorrTable[gRxVfo->Band]) * 2;
}

static void ClampRssiTriggerLevel() {
  settings.rssiTriggerLevel =
      clamp(settings.rssiTriggerLevel, dbm2rssi(settings.dbMin),
            dbm2rssi(settings.dbMax));
}

static void UpdateRssiTriggerLevel(bool inc) {
  if (inc)
    settings.rssiTriggerLevel += 2;
  else
    settings.rssiTriggerLevel -= 2;

  ClampRssiTriggerLevel();

  redrawScreen = true;
  redrawStatus = true;
}

static void UpdateDBMax(bool inc) {
  if (inc && settings.dbMax < 10) {
    settings.dbMax += 1;
  } else if (!inc && settings.dbMax > 12+settings.dbMin) {
    settings.dbMax -= 1;
  } else {
    return;
  }

  ClampRssiTriggerLevel();
  redrawStatus = true;
  redrawScreen = true;
  SYSTEM_DelayMs(20);
}

static void UpdateScanStep(bool inc) {
  if (inc) {
    settings.scanStepIndex = settings.scanStepIndex != S_STEP_100_0kHz ? settings.scanStepIndex + 1 : 0;
  } else {
    settings.scanStepIndex = settings.scanStepIndex != 0 ? settings.scanStepIndex - 1 : S_STEP_100_0kHz;
  }

  settings.frequencyChangeStep = GetBW() >> 1;
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void UpdateCurrentFreq(bool inc) {
  if (inc && currentFreq < F_MAX) {
    currentFreq += settings.frequencyChangeStep;
  } else if (!inc && currentFreq > F_MIN) {
    currentFreq -= settings.frequencyChangeStep;
  } else {
    return;
  }
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void UpdateCurrentFreqStill(bool inc) {
  uint8_t offset = modulationTypeTuneSteps[settings.modulationType];
  uint32_t f = fMeasure;
  if (inc && f < F_MAX) {
    f += offset;
  } else if (!inc && f > F_MIN) {
    f -= offset;
  }
  SetF(f);
  redrawScreen = true;
}

static void UpdateFreqChangeStep(bool inc) {
  uint16_t diff = GetScanStep() * 4;
  if (inc && settings.frequencyChangeStep < 200000) {
    settings.frequencyChangeStep += diff;
  } else if (!inc && settings.frequencyChangeStep > 10000) {
    settings.frequencyChangeStep -= diff;
  }
  SYSTEM_DelayMs(100);
  redrawScreen = true;
}

static void ToggleModulation() {
  if (settings.modulationType < MODULATION_UKNOWN - 1) {
    settings.modulationType++;
  } else {
    settings.modulationType = MODULATION_FM;
  }
  RADIO_SetModulation(settings.modulationType);
  s_reg30_dirty = true;  // VUURWERK v1.3.1 (T2): safe-side re-read after mode change

  RelaunchScan();
  redrawScreen = true;
}

static void ToggleListeningBW() {
  if (settings.listenBw == BK4819_FILTER_BW_NARROWER) {
    settings.listenBw = BK4819_FILTER_BW_WIDE;
  } else {
    settings.listenBw++;
  }
  // VUURWERK: G020 apply now; a VOX lock or STILL listen never re-runs ToggleRX
  if (isListening)
    BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
  redrawScreen = true;
}

static void ToggleBacklight() {
  settings.backlightState = !settings.backlightState;
  if (settings.backlightState) {
    BACKLIGHT_TurnOn();
  } else {
    BACKLIGHT_TurnOff();
  }
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  settings.frequencyChangeStep = GetBW() >> 1;
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void ResetFreqInput() {
  tempFreq = 0;
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    if (freqInputIndex == 0 || freqInputDotIndex) {
      return;
    }
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
    if (freqInputDotIndex == freqInputIndex)
      freqInputDotIndex = 0;
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (int i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey - KEY_0 : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += (freqInputArr[i] - KEY_0) * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += (freqInputArr[i] - KEY_0) * base;
      base /= 10;
    }
  }
  redrawScreen = true;
}

static void Blacklist() {
#ifdef ENABLE_SCAN_RANGES
  blacklistFreqs[blacklistFreqsIdx++ % ARRAY_SIZE(blacklistFreqs)] = peak.i;
#endif

  SetRssiHistory(peak.i, RSSI_MAX_VALUE);
  ResetPeak();
  ToggleRX(false);
  ResetScanStats();
}

#ifdef ENABLE_SCAN_RANGES
static bool IsBlacklisted(uint16_t idx)
{
  for(uint8_t i = 0; i < ARRAY_SIZE(blacklistFreqs); i++)
    if(blacklistFreqs[i] == idx)
      return true;
  return false;
}
#endif

// Draw things

// applied x2 to prevent initial rounding
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax) {
  const int DB_MIN = settings.dbMin << 1;
  const int DB_MAX = settings.dbMax << 1;
  const int DB_RANGE = DB_MAX - DB_MIN;

  const uint8_t PX_RANGE = pxMax - pxMin;

  int dbm = clamp(Rssi2DBm(rssi) << 1, DB_MIN, DB_MAX);

  return ((dbm - DB_MIN) * PX_RANGE + DB_RANGE / 2) / DB_RANGE + pxMin;
}

uint8_t Rssi2Y(uint16_t rssi) {
  return DrawingEndY - Rssi2PX(rssi, 0, DrawingEndY);
}

static void DrawSpectrum() {
  switch (spectrum_mode) {
  default:
  case 0: // NORMAL + PEAK HOLD (mode 1 adds peak dots)
  case 1:
    for (uint8_t x = 0; x < 128; ++x) {
      uint16_t rssi = rssiHistory[x >> settings.stepsCount];
      if (rssi != RSSI_MAX_VALUE) {
        DrawVLine(Rssi2Y(rssi), DrawingEndY, x, true);
      }
      if (spectrum_mode == 1) {
        uint8_t bin = x >> settings.stepsCount;
        if (peak_hold[bin] > 0) {
          uint8_t py = DrawingEndY - peak_hold[bin];
          if (py < DrawingEndY)
            PutPixel(x, py, true);
        }
      }
    }
    break;

  case 2: // VOICE mode -- bars only on hop-eligible bins (vp >= 50)
    for (uint8_t x = 0; x < 128; ++x) {
      uint8_t bin = x >> settings.stepsCount;
      uint8_t vp = voiceProb[bin];

      if (vp < PatrolVoiceThreshold()) {   // == 50 unless patrol is active (review V8)
        PutPixel(x, DrawingEndY, true);
        continue;
      }

      uint16_t rssi = rssiHistory[bin];
      if (rssi == RSSI_MAX_VALUE) continue;
      uint8_t h = Rssi2PX(rssi, 0, DrawingEndY);
      if (vp < 75) h >>= 1;

      DrawVLine(DrawingEndY - h, DrawingEndY, x, true);
      PutPixel(x, 0, true);
    }
    break;
  }
}

static void DrawStatus() {
#ifdef SPECTRUM_EXTRA_VALUES
  sprintf(String, "%d/%d P:%d T:%d", settings.dbMin, settings.dbMax,
          Rssi2DBm(peak.rssi), Rssi2DBm(settings.rssiTriggerLevel));
#else
  sprintf(String, "%d/%d", settings.dbMin, settings.dbMax);
#endif
  GUI_DisplaySmallest(String, 0, 1, true, true);

  BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4],
                           &gBatteryCurrent);

  uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] +
                      gBatteryVoltages[2] + gBatteryVoltages[3]) /
                     4 * 760 / gBatteryCalibration[3];

  unsigned perc = BATTERY_VoltsToPercent(voltage);
  s_lastBatteryPercent = (uint8_t)perc;

  // sprintf(String, "%d %d", voltage, perc);
  // GUI_DisplaySmallest(String, 48, 1, true, true);

  gStatusLine[116] = 0b00011100;
  gStatusLine[117] = 0b00111110;
  for (int i = 118; i <= 126; i++) {
    gStatusLine[i] = 0b00100010;
  }

  for (unsigned i = 127; i >= 118; i--) {
    if (127 - i <= (perc + 5) * 9 / 100) {
      gStatusLine[i] = 0b00111110;
    }
  }
}

static void DrawF(uint32_t f) {
  sprintf(String, "%u.%05u", f / 100000, f % 100000);
  UI_PrintStringSmallNormal(String, 8, 119, 0);   // VUURWERK: G009 a 1 GHz reading ends at x 97, clear of the mode label

  sprintf(String, "%3s", gModulationStr[settings.modulationType]);
  GUI_DisplaySmallest(String, 116, 1, false, true);
  sprintf(String, "%s", bwOptions[settings.listenBw]);
  GUI_DisplaySmallest(String, 108, 7, false, true);
}

static void DrawNums() {

  if (currentState == SPECTRUM) {
    sprintf(String, "%ux", GetStepsCount());
    GUI_DisplaySmallest(String, 0, 1, false, true);
    sprintf(String, "%u.%02uk", GetScanStep() / 100, GetScanStep() % 100);
    GUI_DisplaySmallest(String, 0, 7, false, true);
  }

  if (IsCenterMode()) {
    sprintf(String, "%u.%05u \x7F%u.%02uk", currentFreq / 100000,
            currentFreq % 100000, settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 36, 49, false, true);
  } else {
    sprintf(String, "%u.%05u", GetFStart() / 100000, GetFStart() % 100000);
    GUI_DisplaySmallest(String, 0, 49, false, true);

    sprintf(String, "\x7F%u.%02uk", settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 48, 49, false, true);

    sprintf(String, "%u.%05u", GetFEnd() / 100000, GetFEnd() % 100000);
    GUI_DisplaySmallest(String, 93, 49, false, true);
  }
}

static void DrawRssiTriggerLevel() {
  if (settings.rssiTriggerLevel == RSSI_MAX_VALUE || monitorMode)
    return;
  uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
  for (uint8_t x = 0; x < 128; x += 2) {
    PutPixel(x, y, true);
  }
}

static void DrawTicks() {
  uint32_t f = GetFStart();
  uint32_t span = GetFEnd() - GetFStart();
  uint32_t step = span / 128;
  for (uint8_t i = 0; i < 128; i += (1 << settings.stepsCount)) {
    f = GetFStart() + span * i / 128;
    uint8_t barValue = 0b00000001;
    (f % 10000) < step && (barValue |= 0b00000010);
    (f % 50000) < step && (barValue |= 0b00000100);
    (f % 100000) < step && (barValue |= 0b00011000);

    gFrameBuffer[5][i] |= barValue;
  }

  // center
  if (IsCenterMode()) {
    memset(gFrameBuffer[5] + 62, 0x80, 5);
    gFrameBuffer[5][64] = 0xff;
  } else {
    memset(gFrameBuffer[5] + 1, 0x80, 3);
    memset(gFrameBuffer[5] + 124, 0x80, 3);

    gFrameBuffer[5][0] = 0xff;
    gFrameBuffer[5][127] = 0xff;
  }
}

static void DrawArrow(uint8_t x) {
  for (signed i = -2; i <= 2; ++i) {
    signed v = x + i;
    if (!(v & 128)) {
      gFrameBuffer[5][v] |= (0b01111000 << my_abs(i)) & 0b01111000;
    }
  }
}

// ===================== VUURWERK v1.5.1 PATROL =============================
// Latched, unattended version of the existing VOX voice-seeking sweep
// (spectrum_mode == 2, "hold-to-sweep" elsewhere in this file). Patrol does
// not replace or rewrite that sweep -- it extends it: while patrolActive,
// a handful of the sweep's internal constants are read from patrolSettings
// instead of their hardcoded defaults, and the sweep's own "give up,
// rescan" moments additionally advance the window across the stored range
// instead of re-sweeping the same window forever. When patrolActive is
// false (always, for anyone who has never pressed KEY_0 in VOX mode), every
// one of those read sites resolves to the exact literal hold-to-sweep has
// always used -- see PatrolVoiceThreshold() and the resume-hysteresis read
// in UpdateListening() below. Every tunable number is in the PATROL tuning
// block near the top of this file.

PatrolSettings patrolSettings;
bool           patrolActive = false;

static uint32_t patrolRangeLo, patrolRangeHi;   // window starts walk [lo, hi)
static uint8_t  patrolDwellTicks;   // VUURWERK: G013 half-seconds camped, counted in Tick()
static uint8_t  s_stepBeforePatrol = 0xFF;      // the view's own step, put back when patrol stops
static uint16_t s_triggerBeforePatrol;          // the view's own RSSI trigger, put back when patrol stops

#ifdef ENABLE_SCAN_RANGES
// Capped frequencies are remembered by frequency, because the window-local
// blacklist is index based and is reset on every window advance, and a
// capped index would mean a different frequency in the next window. Scan()
// checks this table directly (PatrolIsCapped below).
static uint32_t patrolCapFreq[PATROL_CAP_SLOTS];
static uint8_t  patrolCapLeft[PATROL_CAP_SLOTS];   // half-seconds still to skip; 0 = slot free

// count is PATROL_CAP_SKIP_HALF_S for the dwell cap, or PATROL_CAP_MENU for
// a MENU skip that lasts until patrol stops (VUURWERK: G005). The slot reused is
// the one nearest expiry, so a MENU skip is the last to be overwritten.
static void PatrolRememberCap(uint32_t f, uint8_t count) {
  uint8_t i = 0;
  for (uint8_t j = 1; j < PATROL_CAP_SLOTS; j++)
    if (patrolCapLeft[j] < patrolCapLeft[i])
      i = j;
  patrolCapFreq[i] = f;
  patrolCapLeft[i] = count;
}

// Called from Tick()'s 500 ms block unless an automatic camp is running:
// half a second of sweeping (or PTT lock) is done (VUURWERK: G003, it used
// to count window wraps).
static void PatrolCapAge(void) {
  for (uint8_t i = 0; i < PATROL_CAP_SLOTS; i++)
    if (patrolCapLeft[i] && patrolCapLeft[i] != PATROL_CAP_MENU)   // VUURWERK: G005 never ages
      patrolCapLeft[i]--;
}

// A bin is capped when it lies within one step of a remembered frequency
// whose skip count is still running. Scan() consults this next to the
// window-local blacklist, so a cap never has to be re-marked on a window
// advance (the ring is only 15 deep; copying up to 24 cap entries into it
// per advance used to wrap and drop caps it had just written). A strong
// carrier leaks into the bins either side through the filter skirts and
// they score as voice too, hence the one-step margin. Inert unless patrol
// is latched.
static bool PatrolIsCapped(uint32_t f) {
  if (!patrolActive) return false;
  const uint32_t step = scanInfo.scanStep;
  for (uint8_t i = 0; i < PATROL_CAP_SLOTS; i++) {
    const uint32_t c = patrolCapFreq[i];
    if (patrolCapLeft[i] && f + step >= c && f <= c + step) {
      rssiHistory[scanInfo.i] = 0;   // VUURWERK: G010 never keep another window's reading
      return true;
    }
  }
  return false;
}

// Blacklist the camped bin's neighbours in this window (see above).
static void PatrolBlacklistNeighbours(void) {
  for (int8_t k = -1; k <= 1; k += 2) {
    const int16_t n = (int16_t)peak.i + k;
    if (n < 0 || n >= (int16_t)scanInfo.measurementsCount) continue;
    blacklistFreqs[blacklistFreqsIdx++ % ARRAY_SIZE(blacklistFreqs)] = (uint16_t)n;
    SetRssiHistory((uint16_t)n, RSSI_MAX_VALUE);
  }
}
#endif

// Cap the bin patrol is camped on: remember it, blacklist it and its
// neighbours for this window, release the camp. Shared by the dwell cap,
// MENU while patrol is camped (VUURWERK: G005) and the carrier-still
// experiment.
static void PatrolCapCurrentBin(uint8_t count) {
#ifdef ENABLE_SCAN_RANGES
  PatrolRememberCap(peak.f, count);
  PatrolBlacklistNeighbours();
#endif
  Blacklist();                // existing MENU-key mechanism, unmodified
  patrolDwellTicks = 0;
}

// The voice-score threshold hold-to-sweep and patrol both gate on. Returns
// the exact literal (50) hold-to-sweep has always used unless patrol is
// active, in which case Sensitivity selects it. This is the only place that
// literal is allowed to vary.
static uint8_t PatrolVoiceThreshold(void) {
  return patrolActive ? patrolSensThreshold[patrolSettings.sensitivity] : 50;
}

// EEPROM 0x0F48..0x0F4F: the 8-byte gap between the F_LOCK/misc block and the
// MR channel names, unmapped by CHIRP, never written by any v1.5.0 build, and
// outside every factory-reset window (patrol settings survive a reset).
//   [0] bits 1:0 sensitivity, 3:2 speed, 5:4 dwell, 7:6 resume (3 = unset)
//   [1] bits 3:0 step index captured with the range (0xF = follow the view),
//       bits 7:4 modulation captured with the range (0xF = follow the view;
//       VUURWERK: G095, v1.5.1 wrote 0xF here)
//   [2..4] range start, [5..7] range stop: 24-bit little endian, 100 Hz
//       units, 0xFFFFFF = unset
// Anything unset or out of range decodes to the default that reproduces the
// pre-patrol behaviour, so a radio upgraded from v1.5.0 behaves exactly as it
// did before the upgrade until a setting is deliberately changed.
#define PATROL_EEPROM_ADDR 0x0F48

static uint32_t PatrolUnpackFreq(const uint8_t *p) {
  const uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
  const uint32_t f = v * 10u;
  return (v == 0xFFFFFFu || f < F_MIN || f > F_MAX) ? 0 : f;
}

static void PatrolPackFreq(uint8_t *p, uint32_t f) {
  const uint32_t v = f ? f / 10u : 0xFFFFFFu;
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
}

// Called once from Main() at boot (v1.5.1 F2), so the menu never shows or
// rewrites an unloaded RAM copy.
void PatrolLoadSettings(void) {
  uint8_t d[8];
  EEPROM_ReadBuffer(PATROL_EEPROM_ADDR, d, sizeof(d));

  patrolSettings.sensitivity = (d[0] >> 0) & 3;
  patrolSettings.speed       = (d[0] >> 2) & 3;
  patrolSettings.dwell       = (d[0] >> 4) & 3;
  patrolSettings.resume      = (d[0] >> 6) & 3;
  if (patrolSettings.sensitivity > 2) patrolSettings.sensitivity = 1;   // 3 = unwritten
  if (patrolSettings.speed       > 1) patrolSettings.speed       = 1;   // 2 and 3 = unwritten or retired Fast
  if (patrolSettings.dwell       > 2) patrolSettings.dwell       = 1;
  if (patrolSettings.resume      > 2) patrolSettings.resume      = 1;

  patrolSettings.step = d[1] & 0x0F;
  if (patrolSettings.step > S_STEP_100_0kHz) patrolSettings.step = PATROL_STEP_VIEW;
  patrolSettings.modulation = d[1] >> 4;   // VUURWERK: G095 checked where it is applied

  patrolSettings.rangeStart = PatrolUnpackFreq(&d[2]);
  patrolSettings.rangeStop  = PatrolUnpackFreq(&d[5]);
}

// Called by app/menu.c when a patrol setting is accepted and by the in-view
// range keys. One 8-byte write; the driver skips it when nothing changed.
void PatrolSaveSettings(void) {
  uint8_t d[8];
  d[0] = (uint8_t)((patrolSettings.sensitivity & 3)
       | ((patrolSettings.speed  & 3) << 2)
       | ((patrolSettings.dwell  & 3) << 4)
       | ((patrolSettings.resume & 3) << 6));
  d[1] = (uint8_t)((patrolSettings.modulation << 4) | (patrolSettings.step & 0x0F));   // VUURWERK: G095
  PatrolPackFreq(&d[2], patrolSettings.rangeStart);
  PatrolPackFreq(&d[5], patrolSettings.rangeStop);
  EEPROM_WriteBuffer(PATROL_EEPROM_ADDR, d);
}

// In-view range setting (VOX mode, keys 2 and 8): the window on screen is
// the ruler. Its left edge becomes the range start, its right edge the range
// stop, and the step and modulation in force at that moment are captured
// with it, so the range is always walked as it was defined. This is the only
// place the range is set; it is stored in EEPROM and survives power cycles.
__attribute__((noinline)) static void PatrolSetRangeEdge(bool stop) {
  if (stop)
    patrolSettings.rangeStop  = GetFStart() + GetBW();
  else
    patrolSettings.rangeStart = GetFStart();
  patrolSettings.step       = settings.scanStepIndex;
  patrolSettings.modulation = settings.modulationType;   // VUURWERK: G095
  PatrolSaveSettings();
  s_toastText = stop ? "SET HI" : "SET LO";
  s_mode_toast_renders = 6;
  redrawScreen = true;
}

static bool PatrolBatteryLow(void) {
  return s_lastBatteryPercent <= 5;
}

static void PatrolAdvanceWindow(void) {
  // Work in window-START terms, not currentFreq. In center mode (scan step
  // under 2.5 kHz) GetFStart() sits half a span below currentFreq, so
  // wrapping currentFreq itself back to patrolRangeLo shifted every later
  // lap by half a span. Carry the center offset across the wrap. A top
  // partial window is end-aligned on patrolRangeHi, so the whole range is
  // swept and nothing past it is.
  const uint32_t span     = GetBW();
  const uint32_t off      = currentFreq - GetFStart();   // 0, or span/2 in center mode
  uint32_t       newStart = GetFStart() + span;
  if (newStart >= patrolRangeHi) {   // VUURWERK: G002 wrap only past HI
    newStart = patrolRangeLo;   // wrap: a new lap begins
  } else if (newStart + span > patrolRangeHi) {
    newStart = patrolRangeHi - span;   // VUURWERK: G002 end-align the top partial window
  }
  currentFreq = newStart + off;
  RelaunchScan();
  ResetBlacklist();
  // RelaunchScan sets preventKeypress, which Tick() honors by skipping
  // HandleUserInput entirely. Hold-to-sweep clears it at the end of every
  // sweep, but a quiet patrol advances the window at that exact moment and
  // re-arms it before the next tick ever reads it, so on a quiet band the
  // toggle key and EXIT were never polled at all. That debounce exists to
  // pace the operator's OWN navigation; patrol blocks navigation keys
  // anyway, so there is nothing for it to pace here.
  preventKeypress = false;
}

// Stopping patrol must not force-release an active listen or lock: a
// ToggleRX(false) here would cut audio on a live QSO and clear s_voxLock,
// which the APP_RunSpectrum exit tail reads for the lock carry-over.
// Patrol only ever changes its own state; whatever the radio was doing when
// it stopped keeps doing it. The step change (if any) takes effect at the
// next sweep, which is when every step change takes effect anyway.
static void PatrolStop(void) {
  if (!patrolActive) return;

  if (s_stepBeforePatrol != 0xFF) {
    settings.scanStepIndex = s_stepBeforePatrol;
    s_stepBeforePatrol     = 0xFF;
    ResetBlacklist();   // VUURWERK: G129 its indices were at patrol's step
  }
  settings.rssiTriggerLevel = s_triggerBeforePatrol;   // hold-to-sweep gets its trigger back
  patrolActive = false;
  BACKLIGHT_TurnOn();    // v1.5.1 (F1): the screen may have timed out during patrol
  redrawScreen = true;   // the PTRL glyph lives in RenderSpectrum, not the status line
}

static void PatrolStart(void) {
  if (patrolActive) return;   // no-op: already latched, never re-arm mid-run
  // VUURWERK: G101 dead scan-range session guard removed

  // Range and step: the stored range, walked at the step captured with it,
  // or -- with nothing stored -- the window on screen at the view's own step.
  // Either way the first window starts at the range start.
  {
    uint32_t   lo     = patrolSettings.rangeStart;
    uint32_t   hi     = patrolSettings.rangeStop;
    const bool stored = (lo != 0 && hi != 0 && lo < hi);

    s_stepBeforePatrol = 0xFF;
    if (stored && patrolSettings.step != PATROL_STEP_VIEW
               && patrolSettings.step != settings.scanStepIndex) {
      s_stepBeforePatrol     = settings.scanStepIndex;
      settings.scanStepIndex = patrolSettings.step;
    }
    // VUURWERK: G095 walk the range in the modulation it was defined in. Not
    // undone by PatrolStop (a held camp keeps its audio); the exit restores
    // the VFO's own modulation.
    if (stored && patrolSettings.modulation < MODULATION_UKNOWN)
      RADIO_SetModulation(settings.modulationType = patrolSettings.modulation);

    const uint32_t off = currentFreq - GetFStart();   // 0, or half a span in center mode
    if (!stored) {
      lo = GetFStart();
      hi = lo + GetBW();
    }
    if (hi > F_MAX) hi = F_MAX;   // never wander past what the hardware covers
    patrolRangeLo = lo;
    patrolRangeHi = hi;
    currentFreq   = lo + off;
  }

  // v1.5.1 (F6): Sensitivity owns the RSSI trigger for the whole run. A
  // trigger lowered in an earlier session must not decide what an unattended
  // run auto-listens to. Hold-to-sweep keeps its own value: it is parked here
  // and put back by PatrolStop, exactly like the step.
  s_triggerBeforePatrol     = settings.rssiTriggerLevel;
  settings.rssiTriggerLevel = patrolSensTrigger[patrolSettings.sensitivity];

  patrolDwellTicks = 0;
#ifdef ENABLE_SCAN_RANGES
  memset(patrolCapLeft, 0, sizeof(patrolCapLeft));   // per session, never carried over
#endif

  RelaunchScan();
  ResetBlacklist();
  preventKeypress = false;   // see PatrolAdvanceWindow

  patrolActive = true;
  redrawScreen = true;   // the PTRL glyph lives in RenderSpectrum, not the status line
}

// Called once per Tick() while patrol is latched. Handles the mandatory
// unattended safeguards that have no equivalent in hold-to-sweep: the dwell
// cap and the low-battery stop. The patrol-only backlight timeout lives in
// Tick()'s 500 ms block.
static void PatrolTick(void) {
  if (!patrolActive) return;

  if (PatrolBatteryLow()) {
    // Merely stopping patrol here is useless: the modal spectrum loop keeps
    // running in VOX (still the most power-hungry mode) and the main
    // scheduler's battery task, which is what raises the low-battery
    // warning, never runs while this loop holds the CPU. The only way
    // "patrol stops and the normal warning shows" can be true is to LEAVE
    // the spectrum: the same exit EXIT uses, after which the main loop takes
    // over and warns as usual.
    PatrolStop();
    gLowBatteryConfirmed = false;   // VUURWERK: G012 bring the LOW BATTERY popup back
    DeInitSpectrum();
    return;
  }

  // A PTT lock is a deliberate operator decision and is exempt from the cap
  // entirely: someone who locks onto a real QSO and walks away is never
  // kicked off it. Only the automatic, unconfirmed camp is capped; Tick()'s
  // 500 ms block counts it (VUURWERK: G013). No per-bin bookkeeping is
  // needed: every path that changes the camped bin passes through
  // ToggleRX(false) first, so isListening is false for at least one tick in
  // between and the counter is reset here. MENU sets PATROL_CAP_MENU to
  // cap the camp at once (VUURWERK: G005).
  if (!isListening || s_voxLock)
    patrolDwellTicks = 0;
  else if (patrolDwellTicks >= PATROL_DWELL_CAP_S * 2)
    PatrolCapCurrentBin(patrolDwellTicks == PATROL_CAP_MENU ? PATROL_CAP_MENU
                                                            : PATROL_CAP_SKIP_HALF_S);
}
// ================== end VUURWERK v1.5.1 PATROL =============================

static void OnKeyDown(uint8_t key) {
  // v1.5.1 PATROL: patrol owns the radio while it runs. Every key that
  // would change VFO/frequency/step/modulation state is inert while
  // latched -- KEY_0 (the toggle, stop), MENU (blacklist; while camped it
  // skips that frequency until patrol stops, VUURWERK: G005), PTT (voice
  // lock, unmodified) and EXIT (stop patrol, end the session) are the
  // only ones that still do anything. This is a single boolean check when
  // patrol has never been used, so default behavior is unchanged.
  if (patrolActive) {
    // VUURWERK: G004 every key restarts the backlight timer, and the first
    // one on a dark screen only wakes it (EXIT included, as on the main
    // screen). With BackLt OFF the light never comes on and keys act.
    const bool wasOn = BACKLIGHT_IsOn();
    BACKLIGHT_TurnOn();
    if (!wasOn && BACKLIGHT_IsOn())
      return;
    switch (key) {
    case KEY_0:
    case KEY_MENU:
    case KEY_PTT:
    case KEY_EXIT:
      break;
    default:
      return;
    }
  }

  switch (key) {
  case KEY_3:
    UpdateDBMax(true);
    break;
  case KEY_9:
    UpdateDBMax(false);
    break;
  case KEY_1:
    UpdateScanStep(true);
    break;
  case KEY_7:
    UpdateScanStep(false);
    break;
  case KEY_2:
    if (spectrum_mode == 2) { PatrolSetRangeEdge(false); break; }   // v1.5.1 PATROL: range start = this window's left edge
    UpdateFreqChangeStep(true);
    break;
  case KEY_8:
    if (spectrum_mode == 2) { PatrolSetRangeEdge(true); break; }    // v1.5.1 PATROL: range stop = this window's right edge
    UpdateFreqChangeStep(false);
    break;
  case KEY_UP:
  case KEY_DOWN:
    s_voxLock = false;               // v1.3.6: UP/DOWN releases the lock, then seeks/navigates
    s_mode_toast_renders = 0;        // VUURWERK: G021 a released lock must not keep saying LOCKED
    if (VoiceModeActive()) {
      // v1.3.5: directional voice seek. UP walks to the next voice bin ABOVE
      // the cursor, DOWN below -- no wrap, so UP always raises the frequency.
      // If there's no voice that way, fall through to band navigation below
      // (the same UpdateCurrentFreq NORM/PEAK use) -- so UP/DOWN always
      // moves you, and a quiet band simply navigates. The old wrap-around hop
      // could orbit one window forever and dead-ended on a beep. int16 loop so
      // a stale peak.i (after a steps-count change) just falls through safely.
      const int16_t steps = (int16_t)scanInfo.measurementsCount;   // <= 128 (gate)
      const int16_t dir   = (key == KEY_UP) ? 1 : -1;
      const int16_t start = (peak.i < (uint16_t)steps) ? (int16_t)peak.i : steps;
      int16_t vi = start + dir;
      for (; vi >= 0 && vi < steps; vi += dir) {
        if (voiceProb[vi] >= 50 &&
            rssiHistory[vi] != RSSI_MAX_VALUE   // never hop an invisible blacklist bin
#ifdef ENABLE_SCAN_RANGES
            && !IsBlacklisted((uint16_t)vi)
#endif
            ) {
          AUDIO_PALETTE_PlayFoundVoice();       // must precede LockVoiceBin (chord mutes AF)
          LockVoiceBin((uint8_t)vi);
          break;
        }
      }
      if (vi >= 0 && vi < steps)
        break;   // hopped onto a voice -- key consumed
      // no voice that way -> fall through to band navigation (shared below)
    }
    UpdateCurrentFreq(key == KEY_UP);   // VUURWERK: G101 dead scan-range guards removed here and below
    break;
  case KEY_SIDE1:
    UpdateCurrentFreq(true);
    break;
  case KEY_STAR:
    s_voxLock = false;               // v1.3.6: cycling the mode abandons a lock (toast shows MODE)
    if (++spectrum_mode > 2) spectrum_mode = 0;   // v1.4.0: 3 modes (NORM/PEAK/VOX); MTI removed
    memset(peak_hold, 0, sizeof(peak_hold));
    memset(noiseHistory, 255, sizeof(noiseHistory));   // v1.3.5: reject value, not 0 --
    memset(voiceProb, 0, sizeof(voiceProb));           // un-measured bins must not score
    s_toastText = specModeNames[spectrum_mode];
    s_mode_toast_renders = 6;                          // as voice (noise 0 -> +45)
    redrawScreen = true;
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(false);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    if (spectrum_mode == 2) {              // v1.5.1 PATROL: one key toggles; the PTRL glyph shows state
      if (kbd.counter != 3)                // VUURWERK: G006 first fire only, never on auto-repeat
        break;
      if (patrolActive) PatrolStop(); else PatrolStart();
      break;
    }
    ToggleModulation();
    break;
  case KEY_6:
    ToggleListeningBW();
    break;
  case KEY_4:
    ToggleStepsCount();
    break;
  case KEY_SIDE2:
    UpdateCurrentFreq(false);
    break;
  case KEY_PTT:
    if (spectrum_mode == 2) {                  // v1.3.7/v1.4.5: whenever the display says VOX, PTT
      if (VoiceModeActive() && isListening) {  // only ever LOCKS -- never the STILL editor; with
        s_voxLock = true;                      // nothing heard it does nothing
        s_toastText = "LOCKED";
        s_mode_toast_renders = 6;
        redrawScreen = true;                   // VUURWERK: G021 show LOCKED now, not a sweep later
      }
      break;
    }
    SetState(STILL);                           // NORM/PEAK keep the stock park-and-monitor
    TuneToPeak();
    break;
  case KEY_MENU:
    if (patrolActive && isListening) {   // VUURWERK: G005 skip it until patrol stops; the
      if (kbd.counter == 3) {            // window blacklist dies at the next advance. First
        s_voxLock        = false;        // fire only: a held MENU must not skip a later camp
        patrolDwellTicks = PATROL_CAP_MENU;   // PatrolTick caps it this same tick
      }
      break;
    }
    Blacklist();
    break;
  case KEY_EXIT:
    PatrolStop();   // no-op unless patrol was running -- leaves no stale state
    DeInitSpectrum();
    break;
  default:
    break;
  }
}

static void OnKeyDownFreqInput(uint8_t key) {
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT:
    if (freqInputIndex == 0) {
      SetState(previousState);
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU:
    if (tempFreq < F_MIN || tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    currentFreq = tempFreq;
    if (currentState == SPECTRUM) {
      ResetBlacklist();
      RelaunchScan();
    } else {
      SetF(currentFreq);
    }
    break;
  default:
    break;
  }
}

void OnKeyDownStill(KEY_Code_t key) {
  switch (key) {
  case KEY_3:
    UpdateDBMax(true);
    break;
  case KEY_9:
    UpdateDBMax(false);
    break;
  case KEY_UP:
    UpdateCurrentFreqStill(true);
    break;
  case KEY_DOWN:
    UpdateCurrentFreqStill(false);
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(true);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(false);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    ToggleModulation();
    break;
  case KEY_6:
    ToggleListeningBW();
    break;
  case KEY_SIDE1:
    monitorMode = !monitorMode;
    break;
  case KEY_SIDE2:
    ToggleBacklight();
    break;
  case KEY_PTT:
    // TODO: start transmit
    /* BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true); */
    break;
  case KEY_EXIT:
    SetState(SPECTRUM);
    monitorMode = false;
    RelaunchScan();
    break;
  default:
    break;
  }
}

static void RenderFreqInput() { UI_PrintString(freqInputString, 2, 127, 0, 8); }

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}

static void RenderSpectrum() {
  DrawTicks();
  DrawArrow(128u * peak.i / GetStepsCount());
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawF(peak.f);
  DrawNums();
  // Show current spectrum mode
  // VUURWERK: G009 x 100..114 keeps the 4-char label clear of DrawF's modulation
  // (116..126, USB included) and of the frequency (ends by x 97)
  GUI_DisplaySmallest(patrolActive ? "PTRL" : specModeNames[spectrum_mode], 100, 1, false, true);
  // v1.2.6: brief mode-change toast (~6 renders) on STAR press
  if (s_mode_toast_renders) {
    // v1.3.6: LOCKED banner while parked on a voice, else the mode-change confirmation.
    // Point straight at the rodata string -- no sprintf/buffer (reclaims flash for LOCK).
    memset(&gFrameBuffer[2][20], 0, 88);
    memset(&gFrameBuffer[3][20], 0, 88);
    UI_PrintStringSmallNormal(s_toastText, 20, 107, 2);
    s_mode_toast_renders--;
  }
}

// v1.5.1: the register editor that used to sit under the meter (LNAs / LNA /
// PGA / IF, MENU to cycle) was removed to fund PATROL. The meter stays.
static void RenderStill() {
  DrawF(fMeasure);

  const uint8_t METER_PAD_LEFT = 3;

  memset(&gFrameBuffer[2][METER_PAD_LEFT], 0b00010000, 121);

  for (int i = 0; i < 121; i+=5) {
    gFrameBuffer[2][i + METER_PAD_LEFT] = 0b00110000;
  }

  for (int i = 0; i < 121; i+=10) {
    gFrameBuffer[2][i + METER_PAD_LEFT] = 0b01110000;
  }

  uint8_t x = Rssi2PX(scanInfo.rssi, 0, 121);
  for (int i = 0; i < x; ++i) {
    if (i % 5) {
      gFrameBuffer[2][i + METER_PAD_LEFT] |= 0b00000111;
    }
  }

  int dbm = Rssi2DBm(scanInfo.rssi);
  uint8_t s = DBm2S(dbm);
  sprintf(String, "S: %u", s);
  GUI_DisplaySmallest(String, 4, 25, false, true);
  sprintf(String, "%d dBm", dbm);
  GUI_DisplaySmallest(String, 28, 25, false, true);

  if (!monitorMode) {
    uint8_t x = Rssi2PX(settings.rssiTriggerLevel, 0, 121);
    gFrameBuffer[2][METER_PAD_LEFT + x] = 0b11111111;
  }

}

static void Render() {
  UI_DisplayClear();

  switch (currentState) {
  case SPECTRUM:
    RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  }

  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
  kbd.prev = kbd.current;
  kbd.current = GetKey();

  if (kbd.current != KEY_INVALID && kbd.current == kbd.prev) {
    if (kbd.counter < 16)
      kbd.counter++;
    else
      kbd.counter -= 3;
    SYSTEM_DelayMs(20);
  } else {
    kbd.counter = 0;
  }

  // VUURWERK: G022 EXIT never auto-repeats: a held EXIT that clears frequency
  // entry or leaves STILL must not then quit the spectrum.
  if (kbd.counter == 3 || (kbd.counter == 16 && kbd.current != KEY_EXIT)) {
    switch (currentState) {
    case SPECTRUM:
      OnKeyDown(kbd.current);
      break;
    case FREQ_INPUT:
      OnKeyDownFreqInput(kbd.current);
      break;
    case STILL:
      OnKeyDownStill(kbd.current);
      break;
    }
  }

  return true;
}

static void Scan() {
  // v1.3.3: UpdateScan() calls Scan() before its bounds check, so we get one
  // call at scanInfo.i == measurementsCount (the phantom step past the span).
  // At STEPS_128 that indexes rssiHistory[128] -- out of bounds; at any count
  // it lets an off-span reading be crowned "peak". Reject it up front.
  if (scanInfo.i >= scanInfo.measurementsCount)
    return;
  if (rssiHistory[scanInfo.i] != RSSI_MAX_VALUE
#ifdef ENABLE_SCAN_RANGES
  && !IsBlacklisted(scanInfo.i)
  && !PatrolIsCapped(scanInfo.f)   // v1.5.1 PATROL: capped carriers stay skipped across windows
#endif
  ) {
    SetF(scanInfo.f);
    if (patrolActive && patrolSettings.speed == 0)   // v1.5.1 PATROL Speed Slow, VUURWERK: G007
      SYSTICK_DelayUs(PATROL_SLOW_SETTLE_US);
    Measure();
    UpdateScanInfo();
  }
}

static void NextScanStep() {
  ++peak.t;
  ++scanInfo.i;
  scanInfo.f += scanInfo.scanStep;
}

static void UpdateScan() {
  Scan();

  if (scanInfo.i < scanInfo.measurementsCount) {
    NextScanStep();
    return;
  }

  if(scanInfo.measurementsCount < 128)
    memset(&rssiHistory[scanInfo.measurementsCount], 0,
      sizeof(rssiHistory) - scanInfo.measurementsCount*sizeof(rssiHistory[0]));

  // Peak decay: every 10 sweeps, decrement peak_hold
  if (++sweep_count >= 10) {
    sweep_count = 0;
    for (int i = 0; i < 128; i++) {
      if (peak_hold[i]) peak_hold[i]--;
    }
  }

  // VOIC mode: compute voice probability per bin after sweep completes
  if (spectrum_mode == 2) {
    uint16_t steps = GetStepsCount();
    for (uint16_t i = 0; i < steps; i++)   // VUURWERK: G101 steps <= 128, clamp dropped
      voiceProb[i] = ComputeVoiceProb(rssiHistory[i], noiseHistory[i]);
  }

  redrawScreen = true;
  preventKeypress = false;

  UpdatePeakInfo();
  if (IsPeakOverLevel()) {
    // v1.3.3: if the loudest bin isn't voice, seek the strongest voice bin in
    // the span (over trigger, real reading, not blacklisted) and lock it, so
    // one strong non-voice carrier no longer starves auto-listen. None -> just
    // rescan. VoiceModeActive() limits this to VOX.
    if (VoiceModeActive() && !monitorMode && voiceProb[peak.i] < PatrolVoiceThreshold()) {
      uint16_t steps = GetStepsCount();   // VUURWERK: G101 <= 128, clamp dropped
      uint8_t  best  = 0xFF;
      uint16_t bestR = 0;
      for (uint8_t i = 0; i < steps; i++) {
        if (voiceProb[i] >= PatrolVoiceThreshold() &&
            rssiHistory[i] != RSSI_MAX_VALUE &&
            rssiHistory[i] >= settings.rssiTriggerLevel &&
#ifdef ENABLE_SCAN_RANGES
            !IsBlacklisted(i) &&
#endif
            rssiHistory[i] > bestR) {
          bestR = rssiHistory[i];
          best  = i;
        }
      }
      if (best != 0xFF) {
        LockVoiceBin(best);
        return;
      }
      if (patrolActive) PatrolAdvanceWindow();   // nothing found this window -- move on
      newScanStart = true;
      return;
    }
    ToggleRX(true);
    TuneToPeak();
    return;
  }

  if (patrolActive) PatrolAdvanceWindow();   // quiet sweep -- move to the next window
  newScanStart = true;
}

static void UpdateStill() {
  Measure();
  redrawScreen = true;
  preventKeypress = false;

  peak.rssi = scanInfo.rssi;
  AutoTriggerLevel();

  ToggleRX(IsPeakOverLevel() || monitorMode);
}

static void UpdateListening() {
  preventKeypress = false;
  if (currentState == STILL) {
    listenT = 0;
  }
  if (listenT) {
    listenT--;
    SYSTEM_DelayMs(1);
    return;
  }

  const bool vox = VoiceModeActive();

  // v1.3.3: VOX listens at the fixed listen bandwidth -- the noise gate reads
  // REG_65 there, which is the correct measure for voice -- so it skips the
  // scan/listen BW swap the other modes use to sample RSSI, avoiding the
  // periodic audible tick during a lock.
  if (currentState == SPECTRUM && !vox) {
    BK4819_WriteRegister(0x43, GetBWRegValueForScan());
    Measure();
    BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
  } else {
    Measure();
  }

  peak.rssi = scanInfo.rssi;
  redrawScreen = true;

  if (vox)
    noiseHistory[peak.i] = BK4819_ReadRegister(0x65) & 0xFF;

#if PATROL_CARRIER_STILL
  // Experimental, off by default (see the PATROL tuning block).
  if (vox && patrolActive && !s_voxLock && currentState == SPECTRUM) {
    if ((BK4819_ReadRegister(0x63) & 0xFF) <= PATROL_STILL_GLITCH) {
      if (++s_stillChecks >= PATROL_STILL_CHECKS) {   // dead carrier: cap it now
        PatrolCapCurrentBin(PATROL_CAP_SKIP_HALF_S);
        return;
      }
    } else
      s_stillChecks = 0;
  }
#endif

  // v1.3.3 VOX hold rule: the noise gate is the hold authority, independent of
  // the RSSI trigger, so a weak-but-readable hopped voice is not dropped for
  // sitting below the sweep trigger. Hold while voice-like; drop only after 2
  // consecutive non-voice checks so a lone flutter sample doesn't chop a QSO.
  // A paused-but-keyed carrier stays quieted (noise < 70) and holds, so speech
  // gaps of any length ride through. Gated to SPECTRUM state: STILL zeroes the
  // re-check timer, which would otherwise run these strikes every tick.
  // Modes 0-2 / monitor / STILL keep stock behavior.
  if (vox && !monitorMode && currentState == SPECTRUM) {
    if (s_voxLock) {                       // v1.3.6 LOCKED: hold this frequency, never resume the
      listenT = VOX_RECHECK_MS;            // hunt (open-squelch monitor -- you hear the channel)
      return;
    }
    if (noiseHistory[peak.i] < 70) {
      s_voxBad = 0;
      listenT = VOX_RECHECK_MS;
      return;
    }
    // v1.5.1 PATROL Resume: patrol's Resume setting substitutes for the
    // fixed VOX_BAD_RELEASE count; Normal reproduces it exactly, so
    // hold-to-sweep (patrolActive always false) is byte-identical here.
    if (++s_voxBad < (patrolActive ? patrolResumeBad[patrolSettings.resume] : VOX_BAD_RELEASE)) {
      listenT = VOX_RECHECK_MS;
      return;
    }
  } else if (IsPeakOverLevel() || monitorMode) {
    if (!vox || noiseHistory[peak.i] < 70) {
      listenT = 1000;
      return;
    }
  }

  ToggleRX(false);
  ResetScanStats();
}

static void Tick() {
  if (gNextTimeslice) {
    gNextTimeslice = false;
#ifdef ENABLE_AM_FIX
    if(settings.modulationType == MODULATION_AM) {
      AM_fix_10ms(vfo); //allow AM_Fix to apply its AGC action
    }
#endif
#ifdef ENABLE_FLASHLIGHT
    FLASHLIGHT_WATCHDOG_Tick();   // VUURWERK: G086 the main 10 ms slice never runs in here
#endif
  }

  if (gNextTimeslice_500ms) {
    gNextTimeslice_500ms = false;

    // v1.5.1 (F1): patrol-only backlight timeout. The main scheduler's 500 ms
    // slice, which normally runs this countdown, never executes while this
    // loop owns the CPU. Hold-to-sweep keeps the screen lit as before.
    if (patrolActive && gBacklightCountdown_500ms > 0
        && --gBacklightCountdown_500ms == 0
        && gEeprom.BACKLIGHT_TIME < (ARRAY_SIZE(gSubMenu_BACKLIGHT) - 1))
      BACKLIGHT_TurnOff();

    // v1.5.1 (F10): the radio is in use, so keep the battery-save countdown
    // from expiring underneath us. Without this the 10 ms ISR runs it out and
    // the radio drops into power save the instant the spectrum is left.
    gBatterySaveCountdown_10ms = battery_save_count_10ms;

    // VUURWERK: G013 the dwell cap counts wall-clock camp time, so a held
    // key no longer stretches it; G003 cap skips age while sweeping or
    // PTT-locked. Both are inert unless patrol runs (PatrolStart resets them).
    if (isListening && !s_voxLock)
      patrolDwellTicks++;
#ifdef ENABLE_SCAN_RANGES
    else
      PatrolCapAge();
#endif
    // VUURWERK: G101 dead >128-step early-peak block removed
  }

  if (!preventKeypress) {
    HandleUserInput();
    if (!isInitialized)   // v1.5.0: EXIT ran DeInitSpectrum inside HandleUserInput, the
      return;             // session is over: no further sweep step (the restore and resync
  }                       // run after the loop, in APP_RunSpectrum and the ProcessKey tail)
  PatrolTick();   // no-op unless patrol is latched -- dwell cap + low-battery stop
  if (!isInitialized)   // low battery just ran DeInitSpectrum -- same rule as the
    return;             // EXIT case above

  if (newScanStart) {
    InitScan();
    newScanStart = false;
  }
  if (isListening && currentState != FREQ_INPUT) {
    UpdateListening();
  } else {
    if (currentState == SPECTRUM) {
      UpdateScan();
    } else if (currentState == STILL) {
      UpdateStill();
    }
  }
  if (redrawStatus || ++statuslineUpdateTimer > 4096) {
    RenderStatus();
    redrawStatus = false;
    statuslineUpdateTimer = 0;
  }
  if (redrawScreen) {
    Render();
    redrawScreen = false;
  }
}

void APP_RunSpectrum() {
  if (gEeprom.KEY_LOCK) {   // VUURWERK: G127 a side key or M Long must not bypass the keypad lock
    gKeypadLocked = 4;      // the stock "unlock" prompt, 2 seconds
    gBeepToPlay   = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
    return;
  }
  WaitKeysReleased();   // VUURWERK: G023 a long-press launch key must not act in here
  // TX here coz it always? set to active VFO
  vfo = gEeprom.TX_VFO;
  // set the current frequency in the middle of the display
  // VUURWERK: G101 dead scan-range entry block removed
  // VUURWERK: G108 centre mode (steps under 2.5k) centres on currentFreq itself
  currentFreq = initialFreq = gTxVfo->pRX->Frequency -
                              (IsCenterMode() ? 0 : (GetStepsCount() / 2) * GetScanStep());

  BackupRegisters();

  isListening = true; // to turn off RX later
  redrawStatus = true;
  redrawScreen = true;
  newScanStart = true;


  ToggleRX(true), ToggleRX(false); // hack to prevent noise when squelch off
  RADIO_SetModulation(settings.modulationType = gTxVfo->Modulation);

  BK4819_SetFilterBandwidth(settings.listenBw = BK4819_FILTER_BW_WIDE, false);

  RelaunchScan();
  ResetBlacklist();   // v1.3.3: clean blacklist per spectrum session (it was
                      // leaking blacklisted indices across bands/sessions)

  memset(rssiHistory, 0, sizeof(rssiHistory));
  memset(peak_hold, 0, sizeof(peak_hold));
  memset(noiseHistory, 255, sizeof(noiseHistory));   // v1.3.5: reject value (see STAR handler)
  memset(voiceProb, 0, sizeof(voiceProb));
  s_voxLock = false;             // v1.3.6: no stale lock across sessions
  s_mode_toast_renders = 0;      // VUURWERK: G014 no toast replayed from the last session
  kbd.counter = 0;               // VUURWERK: G022 no key repeat count across sessions
  spectrum_mode = 0;
  sweep_count = 0;
  s_reg30_dirty = true;  // VUURWERK v1.3.1 (T2): fresh cache every spectrum entry

  patrolActive = false;   // v1.5.1 PATROL: never latched across sessions

  isInitialized = true;

  while (isInitialized) {
    Tick();
  }

  // v1.5.0: swallow the terminal EXIT press. If the key is still down when this
  // function returns, the main loop re-latches it ~100ms later as a FRESH main-screen press --
  // and EXIT there is the VFO A/B toggle ("exiting flips me to the other side", operator-timing
  // dependent, immune to the v1.4.3 exit determinism because it's a real post-exit keypress).
  WaitKeysReleased();
  // VUURWERK: G001 a PTT still held at exit is marked handled, so the main
  // loop never keys up on it; TX needs a release and a fresh press.
  if (!GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT))
    gPttIsPressed = true;

  // v1.4.2 VOX-lock carry-over: write the sweep's peak.f into the active VFO and persist it.
  // Freq mode + same band only; SETTINGS_SaveChannel for a FREQ channel writes the VFO EEPROM
  // region, NEVER a memory channel.
  if (s_voxLock
      && IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)
      && !gTxVfo->FrequencyReverse   // v1.4.5: reverse swaps pRX/pTX -- a carry would mistune by the offset
      && peak.f >= frequencyBandTable[gTxVfo->Band].lower
      && peak.f <= frequencyBandTable[gTxVfo->Band].upper) {
    // v1.4.5: step-round before persisting -- the reload rounds anyway (radio.c:348-349), so an
    // unrounded save would silently shift by up to half a step at the next reconfigure/reboot.
    gTxVfo->freq_config_RX.Frequency = FREQUENCY_RoundToStep(peak.f, gTxVfo->StepFrequency);
    RADIO_ApplyOffset(gTxVfo);      // derive freq_config_TX now (simplex: TX == RX) for PTT-to-answer
    gTxVfo->Modulation = settings.modulationType;   // VUURWERK: G095 carry what was heard (PATROL may use the range's)
    SETTINGS_SaveChannel(gTxVfo->CHANNEL_SAVE, gEeprom.TX_VFO, gTxVfo, 1);   // persist, no reload
  }

  RestoreRegisters();

  // v1.4.3: deterministic exit -- ALWAYS come back on the operator's own VFO, with a fresh full
  // dual-watch dwell (countdown reloaded BEFORE the flag is cleared, as every other site does).
  // VUURWERK: G116 granted after the release wait, so a long EXIT hold cannot use it up
  // (a held PTT only defers the alternate: the countdown still runs). Same for battery
  // save: a hold can latch gSchedulePowerSave, and the FOREGROUND exit never clears it.
  gDualWatchCountdown_10ms   = dual_watch_count_after_1_10ms;
  gScheduleDualWatch         = false;
  gBatterySaveCountdown_10ms = battery_save_count_10ms;
  gSchedulePowerSave         = false;
  gRxVfoIsActive             = false;
  gUpdateStatus              = true;   // VUURWERK: G024 replace the spectrum's status line
  // VUURWERK: G017/G106/G117 full stock resync. Every caller returns through ProcessKey, whose
  // tail runs RADIO_SelectVfos + RADIO_SetupRegisters(true) (retune, interrupt drain, REG_3F
  // re-arm, LED and speaker off, FUNCTION_FOREGROUND) and then ACTION_Monitor if monitor is on.
  gFlagReconfigureVfos       = true;
}
