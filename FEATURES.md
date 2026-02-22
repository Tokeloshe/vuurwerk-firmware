# VUURWERK Feature Tracker: Single Source of Truth

**Current Version**: v1.2.3
**Last Updated**: 2026-02-21
**Flash Used**: 59,164 / 61,440 bytes (2,276 free)

---

## Active Features (verified compiled + integrated)

### TX Chain

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 1 | TX Soft Start (60ms S-curve ramp) | tx_soft_start.c/h | tx_soft_start.o | functions.c:231 (Begin), app.c:1166 (Process 10ms) | v1.0.0, rewritten v1.0.9.5 |
| 2 | CTCSS Lead-In (150ms tone before voice) | ctcss_lead.c/h | ctcss_lead.o | functions.c:230 (Start), app.c:1165 (Process 10ms), app.c:741 (Stop) | v1.0.0 |
| 3 | TX Audio Compressor (RMS-based ALC) | tx_compressor.c/h | tx_compressor.o | functions.c:232 (Start), app.c:1167 (Process 10ms), app.c:740 (Stop) | v1.0.0 |

#### TX Soft Start Algorithm Detail
- 6-step ramp (`TX_RAMP_STEPS = 6`) at 10ms/tick = 60ms total
- Precomputed S-curve lookup: `{19, 75, 128, 181, 237}` derived from `(1 - cos(pi * step / 5)) / 2 * 255`
- Power at each step: `target_power * s_curve[step-1] / 256`
- Writes REG_36 via `BK4819_SetupPowerAmplifier(power, frequency)`

#### CTCSS Lead-In Algorithm Detail
- Countdown timer: `TONE_LEAD_TICKS = 15` (150ms at 10ms/tick)
- On Start: if channel has CTCSS/DCS, calls `BK4819_EnterTxMute()` (REG_50) to mute mic
- Each tick: decrement; at zero, `BK4819_ExitTxMute()` unmutes mic
- On Stop: safety unmute if PTT released early

#### TX Audio Compressor Algorithm Detail
- **RMS Calculation:** 4-sample window (40ms), integer Newton's method `isqrt32()` for square root
- **Envelope Follower:** Asymmetric attack/release using fixed-point (<<8 shift)
  - Attack coefficient: `256 / (attack_ms / 10)`, default 5ms = 1 tick
  - Release coefficient: `256 / (release_ms / 10)`, default 300ms = 30 ticks
- **Gain Reduction:** `excess * (ratio - 10) / ratio`, then `>> 1` for gain steps, capped at 15
- **Final Gain:** `base_gain - gain_reduction + makeup_gain`, clamped [4, 31]
- **Default Config:** threshold=18, ratio=3:1, attack=5ms, release=300ms, makeup_gain=3
- Reads REG_64 (mic level amplitude bits 14:0). Writes REG_7D (mic gain bits 4:0).

### RX Signal Processing

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 4 | FM Gain Staging (AGC) | gain_staging.c/h | gain_staging.o | app.c:1180 (GAIN_STAGING_10ms) | v1.0.4, rewritten v1.0.9.6 |
| 5 | RSSI EWMA Filter | rssi_filter.c/h | rssi_filter.o | app.c:1320 (RSSI_FILTER_Update) | v1.0.4, rewritten v1.0.9.6 |
| 6 | RSSI Histogram | rssi_histogram.c/h | rssi_histogram.o | app.c:1322 (RSSI_HISTOGRAM_Update) | v1.0.4 |
| 7 | Signal Rise-Time Classifier (FM/AM/SSB/Noise) | signal_classifier.c/h | signal_classifier.o | app.c (Update), ui/main.c (GetSymbol in RX context) | v1.0.4, display v1.2.3 |
| 8 | Signal Quality Metrics | signal_quality.c/h | signal_quality.o | app.c:1321 (SIGNAL_QUALITY_Update) | v1.0.0 |
| 9 | S-Meter Compensation (FM) | gain_staging.c/h, ui/main.c | N/A | ui/main.c (GAIN_STAGING_GetGainDiff) | v1.0.9.6 |
| 10 | AM Fix (modified, gain_table shared) | am_fix.c/h | am_fix.o | app.c AM section | inherited, modified v1.0.9.4 |

#### FM Gain Staging Algorithm Detail
- **Guard:** FM mode only (AM_fix owns REG_13 in AM/USB). Only during FOREGROUND + RX.
- **Frequency change detection:** Auto-resets gain to stock (index 0) on frequency change
- **RSSI averaging:** Spike immunity via `(prev + new) / 2`
- **Error computation:** `diff_dB = (rssi - FM_TARGET_RSSI) / 2` where FM target = -75 dBm (raw: 170)
- **Fast attack (signal too strong):**
  - Large overshoot (>=10 dB): aggressive jump, walks table backwards
  - Small overshoot: single step down. Min index = 1.
- **Adaptive hold timer** (via signal_classifier):
  - FAST (FM): 150ms hold
  - SLOW (CW/carrier): 500ms hold
  - Default: 300ms hold
- **6 dB hysteresis band** keeps hold alive within tolerance
- **Slow release:** gain ramps UP one step per 10ms tick toward max
- **Write optimization:** Only writes REG_13 when table index changes
- Reads REG_67 (RSSI). Writes REG_13 (gain register, using `gain_table[]` from am_fix.c).

#### RSSI EWMA Filter Algorithm Detail
- `y[n] = (x + (2^shift - 1) * y_prev) >> shift`
- Default `alpha_shift = 3` => alpha = 1/8, tau = 80ms
- One instance per VFO (2 total). First sample initializes directly.

#### RSSI Histogram Algorithm Detail
- 32 bins covering -130 to -35 dBm (3 dBm per bin)
- Auto-analysis every 256 samples (bitmask check `total_samples & 0xFF == 0`)
- Noise floor = center of peak (mode) bin
- Optimal squelch = noise floor + 6 dB
- Min 100 samples for valid analysis. Per-VFO instances.

#### Signal Rise-Time Classifier Algorithm Detail
- Rise-time measurement: starts when RSSI increases > 3 dB, stops when stable (within +/-3 dB)
- 4 classifications:
  - `FAST` (F): < 50ms rise (FM voice, digital modes)
  - `NORMAL` (N): 50-200ms rise (SSB, AM voice)
  - `SLOW` (S): > 200ms rise (carriers, CW)
  - `NOISE` (~): unstable, < 3 consecutive stable readings
- Per-VFO instances. Provides single-char symbol for UI display.

#### Signal Quality Algorithm Detail
- 8-sample ring buffer of RSSI readings (in dBm)
- Variance computed integer-only: `variance = (sum_sq >> 3) - (mean * mean)`
- Ring buffer index uses `& 7` (power-of-2 mask, no modulo)
- 4 quality levels: EXCELLENT (<3dB), GOOD (<6dB), FAIR (<10dB), POOR (>=10dB variance)

### Squelch

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 11 | Squelch Tail Elimination (menu-toggleable via STE) | squelch_tail.c/h | squelch_tail.o | app.c:1309 (SQUELCH_TAIL_Process 500ms), main.c:140 init | v1.1.0, toggle v1.2.0 |
| 12 | Intelligence-Based Squelch (always-on, hardware VAD) | smart_squelch.c/h | smart_squelch.o | app.c:1312 (SMART_SQUELCH_Update 500ms) | v1.1.0 |
| 13 | Adaptive Squelch State (data module) | squelch.c/h | squelch.o | main.c:138 init | v1.0.0 |

#### Squelch Tail Elimination Algorithm Detail
- 4-state machine: `STE_IDLE` -> `STE_MONITORING` -> `STE_TONE_LOST` -> `STE_MUTED`
- **Self-activating:** enters MONITORING when RX starts with CTCSS (`CODE_TYPE_CONTINUOUS_TONE`)
- **Gate check:** `gEeprom.TAIL_TONE_ELIMINATION` must be enabled (STE menu toggle, wired v1.2.0)
- **Tone detection:** reads REG_0C bit 1 (CTCSS found flag)
- **Confirmation:** 20ms (2 ticks) of tone loss before muting
- **Mute:** `BK4819_SetAF(BK4819_AF_MUTE)` cuts audio (writes REG_47)
- **Unmute recovery:** tone returns after 30ms+ in MUTED => unmute via `RADIO_SetModulation()`, resume MONITORING
- **Timeout:** 150ms (15 ticks) in MUTED => unmute and return to IDLE
- Handles all three industry STE methods: Motorola 120-deg, Kenwood/Icom 180-deg, Chinese/Baofeng 55Hz

#### Intelligence-Based Squelch Algorithm Detail
- **Three-register hardware VAD:**
  - REG_67: RSSI (signal strength)
  - REG_65: Noise indicator (demodulated audio noise; low = voice). **Most powerful predictor.**
  - REG_63: Glitch indicator (transients; low = stable signal)
- **EWMA smoothing:** alpha = 1/8, tau = 80ms on all three: `ewma_update(old, new) = old + ((new - old) >> 3)`
- **Voice probability scoring (0-100):**
  - Noise indicator: +35 if <30, +25 if <60, +10 if <100, -15 if >200
  - Glitch indicator: +20 if <15, +10 if <40, -20 if >150, -5 if >80
  - SNR estimate (RSSI vs noise floor): +25 if >25dB, +15 if >15dB, +5 if >8dB, -10 otherwise
  - Cross-correlation bonus: +10 if noise<60 AND glitch 5-40 (voice signature)
  - Double-bad penalty: -10 if noise>150 AND glitch>100
- **Hangover:** voice_prob held at 50 for 200ms after last voice detection
- **Dynamic squelch adjustment (writes REG_78):**
  - VP >= 70: -6 steps (easiest to open, high confidence voice)
  - VP >= 50: -3 steps
  - VP >= 30: 0 (default)
  - VP >= 15: +2 steps
  - VP < 15: +4 steps (hardest to open, noise)
  - **Safety:** only loosens dynamically, never tightens to avoid closing during speech pauses
- Reads REG_67, REG_65, REG_63. Writes REG_78 only when adjustment changes.
- Consumes `gRSSI_Histogram[vfo].noise_floor_dbm` from rssi_histogram module.

### Scanning & Monitoring

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 14 | Scan+Watch (dual-VFO scanning) | scanwatch.c/h | scanwatch.o | app.c (6 call sites), app/menu.c | v1.0.9 |
| 15 | Intelligent Dual-Watch | dual_watch_mgmt.c/h | dual_watch_mgmt.o | app.c (Update, GetDwellTime in toggle, ReportActivity on squelch open) | v1.0.4, wired v1.2.3 |
| 16 | VFO Split (TX/RX freq separation) | vfo_split.c/h | vfo_split.o | app.c:1183 (Process 10ms), app/menu.c (SCANWATCH opt 3) | v1.0.0, menu v1.2.3 |
| 17 | Bandscope (mini spectrum on main screen) | bandscope.c/h | bandscope.o | app.c:1177 (Process), app/main.c:172 (F+7 toggle) | v1.0.4 |

#### Scan+Watch Algorithm Detail
- 4-state machine: `SCANWATCH_OFF` -> `SCANNING` -> `CHECKING` -> `LISTENING`
- Every `SCANWATCH_CHECK_EVERY_N_STEPS = 4` scan steps, switches to watch VFO
- Dwell timer: 100ms (`SCANWATCH_DWELL_10MS = 10` ticks)
- Signal hold: 2 seconds (`SCANWATCH_HOLD_10MS = 200` ticks)
- If signal persists, hold resets. If drops and hold expires, resumes scanning.

#### Intelligent Dual-Watch Algorithm Detail
- Default dwell 500ms, range [200ms, 2000ms]
- Activity-based weighting: more active VFO gets -100ms, less active gets +100ms
- Activity decay: every 256 reports, counters *= 3/4
- RSSI averaging: IIR `avg = (avg * 3 + new) / 4`

#### VFO Split Algorithm Detail
- 5-state machine: `IDLE` -> `HOP_TO_B` -> `SETTLE` -> `READ_RSSI` -> `RETURN_TO_A`
- **Hard gate:** only runs during `FUNCTION_FOREGROUND` (idle, squelch closed)
- Hop intervals: Slow=500ms, Normal=300ms, Fast=150ms
- RSSI threshold: -100 dBm, conversion: `REG_67 >> 1 - 160`
- Supports: memory channels, frequency range, scan list 1/2
- Full VFO A restore via stock `RADIO_ConfigureChannel() + RADIO_SetupRegisters(true)`
- Reads REG_67 (RSSI). Hops via `BK4819_SetFrequency()` (REG_38/39) + `BK4819_RX_TurnOn()` (REG_30).

#### Bandscope Algorithm Detail
- 128-pixel scrolling timeline: `memmove()` shifts left, new sample appended right
- Sample rate: ~100ms (tick_divider counts to 10 at 10ms/tick)
- Peak hold with decay: peaks decremented every ~500ms
- Noise floor as dotted line (every 4 pixels)
- Height = rssi >> 5 (max 7 pixels). Reads REG_67 (RSSI).

### Spectrum Analyzer (4 modes via Star key)

Modes implemented in `app/spectrum.c` (modified egzumer/fagci code).

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 18 | NORM: Standard spectrum bars | app/spectrum.c | app/spectrum.o | Spectrum app, mode 0 | inherited |
| 19 | PEAK: Peak hold with decay | app/spectrum.c | app/spectrum.o | Spectrum app, mode 1 | inherited, enhanced |
| 20 | MTI: Moving Target Indicator (XOR diff) | app/spectrum.c | app/spectrum.o | Spectrum app, mode 2 | inherited, enhanced |
| 21 | VOX: Voice-Seeking Spectrum (voice probability + hop) | app/spectrum.c | app/spectrum.o | Spectrum app, mode 3 | v1.2.0 |

#### Spectrum Mode System
- `spectrum_mode` cycles 0-3 via Star key
- Mode labels: `{"NORM", "PEAK", " MTI", "VOX "}` shown top-right
- New data arrays: `peak_hold[128]`, `prev_sweep[128]`, `noiseHistory[128]`, `voiceProb[128]`

#### NORM + PEAK Drawing (modes 0-1, merged)
- Mode 0: standard bars. Mode 1: adds peak hold dots above bars.
- Peak decay: every 10 sweeps, all `peak_hold[]` values decremented.

#### MTI Mode (mode 2)
- XOR of current vs previous sweep values. Shows only signal changes.
- `prev_sweep[]` updated each full sweep.

#### VOX / Voice-Seeking Mode (mode 3), VUURWERK Original

**ComputeVoiceProb(rssi, noise) Algorithm:**
- Integer-only scoring, max return 95 (never 100)
- Early reject: rssi < 50 or noise > 200 returns 0
- Noise scoring: +45 if <30, +20 if <70
- RSSI scoring: +35 if >100, +15 if >65
- Cross-correlation: +15 if rssi>80 AND noise<40

**VOIC Drawing:**
- Bins with VP < 35: floor dot only (no voice)
- Bins with VP 35-74: half-height bars (low confidence)
- Bins with VP >= 75: full-height bars (high confidence)

**Voice-Hop Navigation (UP/DOWN keys):**
- Searches for next bin with `voiceProb[vi] >= 50`
- Uses `ToggleRX(true)` + `TuneToPeak()`; stays in SPECTRUM state (v1.2.1 fix)
- Wraps around step count boundaries

**Voice Gate (auto-listen + listen):**
- Auto-listen skips non-voice bins (voiceProb < 50)
- During listen: reads REG_65, continues only if noise < 70 (voice indicator)
- Noise register check via `BK4819_ReadRegister(0x65) & 0xFF` in `UpdateListening()`

**Measure() Enhancement:**
- In VOIC mode, captures `BK4819_ReadRegister(0x65) & 0xFF` into `noiseHistory[]` per bin
- After sweep, computes `voiceProb[]` for all bins via `ComputeVoiceProb()`

**BK4819 Register (VUURWERK addition):** Reads REG_65 (noise indicator) for voice probability.

### UI / Display

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 22 | RSSI Bar (S-meter with FM compensation) | ui/main.c | N/A | ENABLE_RSSI_BAR=1 | inherited, enabled v1.0.9.6 |
| 23 | Context-Aware Status Line | status_line.c/h | status_line.o | app.c:1336-1342 (SetContext + Update 500ms) | v1.0.4 |
| 24 | Boot Screen (VERSION_STRING) | ui/welcome.c | N/A | Boot sequence | v1.0.0 |
| 25 | About Screen | ui/vuurwerk_about.c/h | ui/vuurwerk_about.o | app/menu.c:1730 (long-press MENU) | v1.0.4 |
| 26 | Categorized Menu System (6 visible + UNLOCK) | ui/vuurwerk_menu.c/h | ui/vuurwerk_menu.o | app/main.c:694, app/menu.c (5+ sites) | v1.0.9 |
| 27 | Activity Log (uptime + RF activity) | activity_log.c/h | activity_log.o | app.c (UpdateUptime 1s, Add on squelch open) | v1.0.4, wired v1.2.3 |

#### Categorized Menu Algorithm Detail
- 7 categories: RECEIVE (7), TONE (7), TX/TRANSMIT (13), SCAN (7 incl SCANWATCH), CHANNEL (5), CONFIG (15), UNLOCK (hidden, 9)
- `GetCategoryForMenuId()`: hard-coded switch mapping every `MENU_*` ID to category
- `SelectCategory()`: walks `MenuList[]`, builds filtered index array (`gCategoryItems[]`, max 20)
- UNLOCK category visible only when `gF_LOCK=true`
- **Total:** 63 menu items across 7 categories (6 visible, 1 hidden)

#### Activity Log Algorithm Detail
- 20-entry ring buffer (`ACTIVITY_LOG_SIZE = 20`)
- Continuation detection: same frequency (+/-10 kHz) within 10 seconds updates existing entry
- Uptime counter saturates at 65535 seconds (~18 hours)

---

## Module Dependency Graph

```
rssi_filter ──feeds──> signal_quality
rssi_filter ──feeds──> rssi_histogram ──consumed by──> smart_squelch
rssi_filter ──feeds──> signal_classifier ──consumed by──> gain_staging
rssi_filter ──feeds──> dual_watch_mgmt
vfo_split ──calls──> bandscope (RecordHop, currently no-op placeholder)
gain_staging ──uses──> am_fix (gain_table[] extern)
squelch_tail (independent, reads stock radio state)
smart_squelch (independent except rssi_histogram noise floor)
tx_compressor (independent)
ctcss_lead (independent)
tx_soft_start (independent)
scanwatch (independent)
activity_log (independent)
status_line (independent)
vuurwerk_menu (independent, reads stock menu structures)
vuurwerk_about (independent)
```

---

## BK4819 Register Ownership (v1.2.3)

| Register | Owner Module | Direction | Purpose |
|----------|-------------|-----------|---------|
| REG_0C | squelch_tail.c | Read | CTCSS found flag (bit 1) |
| REG_13 | gain_staging.c, am_fix.c | Write | RX gain control (FM only, AM_fix owns in AM mode) |
| REG_36 | tx_soft_start.c | Write (via API) | PA power level |
| REG_47 | squelch_tail.c | Write (via API) | Audio path AF mute/unmute |
| REG_48 | stock radio.c | Write | AF RX gain + DAC |
| REG_50 | ctcss_lead.c | Write (via API) | TX mic mute |
| REG_63 | smart_squelch.c | Read | Glitch indicator (transients) |
| REG_64 | tx_compressor.c | Read | Mic level amplitude |
| REG_65 | smart_squelch.c, app/spectrum.c VOX | Read | Noise indicator (voice predictor) |
| REG_67 | gain_staging.c, smart_squelch.c, bandscope.c, vfo_split.c | Read | RSSI |
| REG_78 | smart_squelch.c | Write | RSSI squelch open/close threshold |
| REG_7D | tx_compressor.c | Write | Mic gain (bits 4:0) |

Each module touches only its designated registers. No register conflicts exist between modules.

---

## Egzumer Inherited Features (maintained, not VUURWERK-original)

### Stock Radio Functions
- Wide RX (18 MHz - 1300 MHz)
- AM Fix (base version, modified for gain_table sharing)
- DTMF Encode/Decode (still compiled)
- Flashlight
- Channel Scanner with ranges
- Copy Channel to VFO
- Big Frequency Display
- Sensitive Squelch (ENABLE_SQUELCH_MORE_SENSITIVE=1)

### Disabled (compile flags off)
- FM Broadcast Radio (ENABLE_FMRADIO=0)
- VOX Operation (ENABLE_VOX=0)
- Aircopy (ENABLE_AIRCOPY=0)
- Audio Bar (ENABLE_AUDIO_BAR=0)
- NOAA (ENABLE_NOAA=0)

### Egzumer Spectrum Analyzer
- Base spectrum analyzer framework (app/spectrum.c, app/chFrScanner.c)
- VUURWERK modified to add 4 display modes (NORM/PEAK/MTI/VOX)
- Star key cycles modes, mode name shown top-right

---

## Flash Budget History

| Version | Text Size | Free | Delta | Notes |
|---------|-----------|------|-------|-------|
| v1.0.0 | ~57,800 | ~3,600 | baseline | Initial 22 modules, most not wired |
| v1.0.4 | ~58,500 | ~2,900 | +700 | Spectrum modes, scan+watch, bandscope |
| v1.0.9.4 | ~59,200 | ~2,200 | +700 | Cleanup, signal_classifier fix |
| v1.0.9.5 | ~60,150 | ~1,290 | +950 | TX wiring, gain staging integration |
| v1.0.9.6 | 60,500 | 940 | +350 | Gain staging rewrite, RSSI bar, freq detect |
| v1.0.9.7 | 60,452 | 988 | -48 | Dead code cleanup |
| v1.1.0 | 61,040 | 400 | +588 | STE (+112), smart squelch (+476) |
| v1.2.0 | 61,228 | 212 | +188 | VOIC spectrum mode, voice-hop, STE toggle, waterfall removed (-200), dead API removed |
| v1.2.1 | 61,192 | 248 | -36 | Voice-hop fix (stay in SPECTRUM), voice gate, label fix (VOX), cases 0/1 merged |
| v1.2.2 | 61,084 | 356 | -108 | Code optimization pass |
| v1.2.3 | **59,164** | **2,276** | **-2,080** | Feature wiring + snprintf removal + modulo fixes |

**Flash Limit:** 61,440 bytes (60KB)
**Current Headroom:** 2,276 bytes (3.71%)

---

## Version History

| Version | Date | Key Changes |
|---------|------|-------------|
| v1.0.0 | 2026-02-13 | Initial: 22 modules created, TX chain + squelch modules (most integration pending) |
| v1.0.4 | 2026-02-14 | Spectrum modes, scan+watch, bandscope, RX signal chain fully wired |
| v1.0.8 | 2026-02-14 | Intermediate bug fixes |
| v1.0.9 | 2026-02-14 | Menu restructure: 6 visible categories + UNLOCK, clean layout |
| v1.0.9.4 | 2026-02-14 | Major cleanup: dead code removed, signal_classifier fix, gain_staging rewrite |
| v1.0.9.5 | 2026-02-15 | TX chain wired, TX_RAMP_STEPS fix, removed dead modules from Makefile |
| v1.0.9.6 | 2026-02-15 | Gain staging rewrite (FOREGROUND+RX, freq change detect), S-meter compensation, RSSI bar enabled |
| v1.0.9.7 | 2026-02-15 | Dead code cleanup: removed 3 modules from build, deleted 7 orphaned files, archived 2 |
| v1.1.0 | 2026-02-15 | Squelch tail elimination (4-state machine, all STE methods), intelligence-based squelch with voice probability (3-register VAD, EWMA, dynamic threshold) |
| v1.2.0 | 2026-02-16 | Voice-seeking spectrum (VOX mode replaces waterfall), voice-hop navigation (UP/DOWN), STE menu toggle wired, dead smart_squelch API removed |
| v1.2.1 | 2026-02-16 | Bugfix: voice-hop stays in SPECTRUM (was exiting to STILL), voice-gated audio (auto-listen + listen check noise register), mode label VOX, DrawSpectrum optimization |
| v1.2.2 | 2026-02-20 | Code optimization pass, DTMF cleanup |
| v1.2.3 | 2026-02-21 | Feature wiring (Activity Log, Dual-Watch, VFO Split, Signal Classifier), snprintf removal, modulo fixes, documentation pass |

---

## Menu Categories (v1.0.9)

| Category | # Items |
|----------|---------|
| RECEIVE | 7 |
| TONE | 7 |
| TRANSMIT | 13 |
| SCAN | 7 |
| CHANNEL | 5 |
| CONFIG | 15 |
| UNLOCK (hidden) | 9 |

**Total:** 63 menu items across 7 categories (6 visible, 1 hidden)

---

## Removed Features (historical record)

| Feature | Original File | Removed In | Reason |
|---------|---------------|------------|--------|
| ALC with Learning | alc_learning.c/h | v1.0.9.5 | Never produced useful output |
| Pre-Emphasis Optimization | preemph_opt.c/h | v1.0.9.5 | Never produced useful output |
| TX Power Auto-Step-Down | tx_power_mgmt.c/h | v1.0.9.5 | Never produced useful output |
| Auto Repeater Detection | auto_repeater.c/h | v1.0.9.7 | Never integrated, file deleted |
| Frequency Counter (VUURWERK) | freq_counter.c/h | v1.0.9.7 | Redundant with egzumer chFrScanner, file deleted |
| Battery Modeling | battery_model.c/h | v1.0.9.7 | Never implemented, archived |
| Goertzel CTCSS Decoder | goertzel_ctcss.c/h | v1.0.9.7 | BK4819 hardware handles CTCSS, file deleted |
| Priority Scan | priority_scan.c/h | v1.0.9.7 | Never implemented, file deleted |
| Custom 8x8 Icons | vuurwerk_icons.c/h | v1.0.9.7 | Never referenced, file deleted |
| Deviation Calibration | deviation_cal.c/h | v1.0.9.7 | Compiled but never called, removed from build (file still exists) |
| Spectrum Enhancements | spectrum_enh.c/h | v1.0.9.7 | Redundant; spectrum modes live in app/spectrum.c |
| Waterfall Spectrum Mode (WFAL) | app/spectrum.c (case 3) | v1.2.0 | Replaced by VOX mode; freed 200 bytes text + 896 bytes BSS |
| Smart Squelch API (GetVoiceProb, QuickVoiceProb) | smart_squelch.c/h | v1.2.0 | Never called externally; spectrum VOX uses own ComputeVoiceProb |

---

## LTO-Stripped Functions (compiled but zero flash cost)

These functions exist in source but have no reachable call path from `main()`. LTO strips them:

- `ACTIVITY_LOG_FindFrequency()`, `Clear()`, `Get()`, `GetCount()`: log retrieval (Add is wired)
- `RSSI_HISTOGRAM_GetOptimalSquelch()`: squelch value not read externally
- `VFO_SPLIT_GetHitCount()`, `GetLastHitFreq()`, `GetLastHitTime()`, `SwitchToB()`: status queries not wired
- `BANDSCOPE_SetNoiseFloor()`: noise floor setter never called (Render reads the variable directly)

These represent future expansion points at zero current flash cost.

---

## Flash Optimization Lessons

- Cortex-M0 has NO hardware divide, so the `%` operator pulls ~200 byte software division lib
- Use `if (++i >= n) i = 0;` instead of `i = (i + 1) % n;`
- LTO already strips non-static unreachable functions; making them static saves 0 bytes
- Adding MENU items is EXPENSIVE (~296 bytes): MenuList entry + 3 switch cases + EEPROM
- Non-static unused functions cost 0 bytes with LTO; it strips them
- LTO does NOT merge identical code across switch cases; merging manually saves ~40 bytes
- Adding a second call site to a function can force LTO to de-inline: ~300 bytes penalty
- `sprintf(String, "%s", name)` -> pass `name` directly: saves ~8-12 bytes

---

## Notes

### chFrScanner.c (egzumer frequency counter)
Investigated for removal in v1.0.9.7. Referenced in 10+ stock egzumer files. Deeply entangled with stock code. Removing would violate THE ONE LAW. Left in place.

---

## Maintenance Rules

1. **Every version bump** -> update this file FIRST, then code
2. **Adding a feature** -> add to "Active Features" with call site verification
3. **Removing a feature** -> move to "Removed Features" table below with reason
4. **Pre-commit checklist:**
   - Update "Current Version" and "Last Updated"
   - Update Flash Budget table with new `arm-none-eabi-size` output
   - Add entry to Version History
   - Verify all "Active Features" still have valid call sites
5. **Never list aspirational features as active.** If not wired, it's "Planned"

---

**End of FEATURES.md**
*This file is the contract. If it's not listed as "Active", it doesn't ship.*
