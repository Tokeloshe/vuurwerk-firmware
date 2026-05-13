# VUURWERK Feature Tracker: Single Source of Truth

**Current Version**: v1.2.7
**Last Updated**: 2026-05-13
**Flash Used**: 60,772 / 61,440 bytes (668 free, -148 after data section)

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
- Single-byte state machine (v1.2.5 reclamation): `s_countdown` (`static uint8_t` private to `ctcss_lead.c`) carries both "active" and "ticks remaining" -- `0` means inactive, `>0` means lead-in is running with N ticks remaining. Header API is the three-function quartet (Start / Process / Stop); BSS zero-init at boot supplies the inactive default.
- Countdown timer: `TONE_LEAD_TICKS = 15` (150ms at 10ms/tick), defined privately in `ctcss_lead.c`
- On Start: if channel has CTCSS/DCS, calls `BK4819_EnterTxMute()` (REG_50) to mute mic and sets countdown=15
- Each tick: `if (--countdown == 0) BK4819_ExitTxMute()` -- single decrement-and-test, unmutes mic on the transition to zero
- On Stop: safety unmute (`if (countdown != 0) ExitTxMute(); countdown=0`) if PTT released early

#### TX Audio Compressor Algorithm Detail
- **RMS Calculation:** 4-sample window (40ms), integer Newton's method `isqrt32()` for square root
- **Envelope Follower:** Asymmetric attack/release using fixed-point (<<8 shift)
  - Attack coefficient: `256 / (attack_ms / 10)`, default 5ms = 1 tick
  - Release coefficient: `256 / (release_ms / 10)`, default 300ms = 30 ticks
- **Gain Reduction:** `excess * (ratio - 10) / ratio`, then `>> 1` for gain steps, capped at 15
- **Final Gain:** `base_gain - gain_reduction + makeup_gain`, clamped [4, 31]
- **Default Config:** threshold=18, ratio=3:1, attack=5ms, release=300ms, makeup_gain=3
- **Write optimization:** Only writes REG_7D when computed mic gain changes (mirrors gain_staging Feature #4 pattern; suppresses ~100 redundant SPI writes per second under steady-input conditions; eliminates the theoretical zipper-noise risk on a register that modulates the microphone signal path)
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
| 10 | AM Fix (modified, gain_table shared, signal-classifier adaptive hold) | am_fix.c/h | am_fix.o | app.c AM section | inherited, modified v1.0.9.4, adaptive hold v1.2.4 |

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
- **BSS zero-init contract** (v1.2.7 reclamation, 2026-05-09): no
  explicit Init function. The `gGainStaging[2]` array and the static
  `last_frequency[2]` array are both BSS, so the C runtime zero-clears
  them at boot. First-tick `last_frequency[vfo] == 0` triggers the
  frequency-change branch which writes stock gain to REG_13 -- the
  same cold-start state the removed Init produced, with -28 bytes
  flash and -4 bytes BSS reclaimed.
- Reads REG_67 (RSSI). Writes REG_13 (gain register, using `gain_table[]` from am_fix.c).

#### RSSI EWMA Filter Algorithm Detail
- `y[n] = (x + 7*y_prev) >> 3`
- `ALPHA_SHIFT = 3` (compile-time constant in `rssi_filter.c`) => alpha = 1/8, tau = 80ms
- One instance per VFO (2 total). First sample initializes directly.
- Per-instance state is a single `int16_t filtered_rssi` (2 bytes,
  no padding), file-private inside `rssi_filter.c`. `INT16_MAX` is
  the "not yet initialized" sentinel. The struct definition and
  global both live inside the implementation file (no header leak).
  The previous per-instance `alpha_shift` field was reclaimed once
  it was confirmed to be a runtime field carrying a compile-time
  constant (-36 bytes flash); the `bool initialized` field was
  reclaimed by the sentinel-init trick (-16 bytes flash, -4 bytes
  BSS). Combined reclamation: -52 bytes flash, -4 bytes BSS.

#### RSSI Histogram Algorithm Detail
- 32 bins covering -130 to -35 dBm (3 dBm per bin)
- Auto-analysis every 256 samples (bitmask check `total_samples & 0xFF == 0`)
- Noise floor = center of peak (mode) bin
- Min 100 samples for valid analysis. Per-VFO instances.
- Public surface: `RSSI_HISTOGRAM_Init`,
  `RSSI_HISTOGRAM_Update`. Internal helpers `Analyze` and `Reset`
  are file-static. Consumers (`bandscope.c`, `smart_squelch.c`)
  read `gRSSI_Histogram[vfo].valid` and `.noise_floor_dbm`
  directly under the `.valid` gate. The previously-cached
  `optimal_squelch_dbm` field and its public accessor
  `RSSI_HISTOGRAM_GetOptimalSquelch` were reclaimed once it was
  confirmed both had zero external consumers. Each consumer
  applies its own dB margin to `noise_floor_dbm` in its own
  units (smart_squelch uses the BK4819 RSSI raw scale; bandscope
  uses the height-byte scale).

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
| 11 | Squelch Tail Elimination (menu-toggleable via STE) | squelch_tail.c/h | squelch_tail.o | app.c:1345 (SQUELCH_TAIL_Process 10ms), main.c:140 init | v1.1.0, toggle v1.2.0 |
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
- **Frequency-change reset (v1.2.7):** static `last_frequency` cache
  detects RX frequency changes at the top of `SMART_SQUELCH_Update()`.
  On mismatch the function-local `voice_hold` and `prev_adj` statics
  are cleared and the three EWMA filters reset to their constructor-init
  values (rssi_smooth=0, noise_smooth=127, glitch_smooth=255). Closes
  the cross-channel bleed where ~200ms of voice hangover and ~80ms of
  EWMA state from the prior VFO would inappropriately loosen the
  squelch on the new VFO during dual-watch / cross-band / channel-hop
  transitions. Pattern matches gain_staging.c and signal_quality.c.

### Scanning & Monitoring

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 14 | Scan+Watch (dual-VFO scanning) | scanwatch.c/h | scanwatch.o | app.c (6 call sites), app/menu.c | v1.0.9 |
| 15 | Intelligent Dual-Watch | dual_watch_mgmt.c/h | dual_watch_mgmt.o | app.c (Update, GetDwellTime in toggle, ReportActivity on squelch open) | v1.0.4, wired v1.2.3 |
| 16 | VFO Split (TX/RX freq separation) | vfo_split.c/h | vfo_split.o | app.c:1183 (Process 10ms), app/menu.c (SCANWATCH opt 3) | v1.0.0, menu v1.2.3 |
| 17 | Bandscope (mini spectrum on main screen) | bandscope.c/h | bandscope.o | app.c:1177 (Process), app/main.c:172 (F+7 toggle) | v1.0.4 |
| 30 | Scan Rate Telemetry (live channels-per-second display during scan) | scan_rate.c/h | scan_rate.o | app/app.c (NoteStep after CHFRSCANNER_ContinueScanning, Tick10ms in APP_TimeSlice10ms), ui/main.c (status-line render appends `NNc/s` to SCANNING) | v1.2.5 |

#### Scan Rate Telemetry Algorithm Detail
- 100-tick (1-second) rolling window. Four uint8_t state vars
  (`step_counter`, `last_cps`, `tick_counter`, `quiet_ticks`);
  zero-cost popcount or divide because counter and snapshot fit
  in 8 bits.
- `SCAN_RATE_NoteStep()` increments `step_counter` (saturates at 0xFF).
  Called from `app/app.c` immediately after `CHFRSCANNER_ContinueScanning()`
  in the v1.2.5-marked block, so SCANWATCH dwell ticks (which are
  blocked by the existing `!SCANWATCH_IsOnWatchVFO()` gate) are not
  counted as scan steps.
- **Fresh-start drain (v1.2.7 ux, 2026-05-12):** `NoteStep()` zeros
  `last_cps` if `quiet_ticks >= 50` before recording the new step,
  i.e., when the previous step was >= 500 ms ago. This kills the
  stale-rate display window on scan-restart where the status line
  used to show the previous run's rate for up to ~1 s before the
  next rollover. The natural 1 s drain (last_cps = 0 once a quiet
  second has fully elapsed) is preserved unchanged for the
  long-quiet path; `quiet_ticks` only matters at the transition.
- `SCAN_RATE_Tick10ms()` runs every 10 ms from `APP_TimeSlice10ms`.
  Increments `quiet_ticks` (saturating at 0xFF). On every 100th
  tick (1 s), copies `step_counter` to `last_cps` and resets the
  step / tick counters.
- `SCAN_RATE_ChannelsPerSec()` returns the snapshot byte for read by
  the status-line renderer.
- Status-line renderer (`ui/main.c:854..862` SCANNING branch) shows
  "SCANNING NNc/s" when `last_cps > 0`, "SCANNING" otherwise --
  preserves stock display during the first second of scan or whenever
  the rate is genuinely zero.
- **Why it matters:** Operators can see at a glance whether squelch
  is robbing scan throughput (slow rate = lots of carriers being
  evaluated) versus an empty band (full rate = no candidates pausing
  the loop). No other UV-K5 firmware surfaces this metric.

#### Scan+Watch Algorithm Detail
- 4-state machine: `SCANWATCH_OFF` -> `SCANNING` -> `CHECKING` -> `LISTENING`
- Every `SCANWATCH_CHECK_EVERY_N_STEPS = 4` scan steps, switches to watch VFO
- Dwell timer: 100ms (`SCANWATCH_DWELL_10MS = 10` ticks)
- Signal hold: 2 seconds (`SCANWATCH_HOLD_10MS = 200` ticks)
- If signal persists, hold resets. If drops and hold expires, resumes scanning.
- **Watch-VFO find isolation (v1.2.5):** `CHFRSCANNER_Found()` is gated by
  `!SCANWATCH_IsOnWatchVFO()` at app/app.c:468 so signals heard while dwelling
  on the watch VFO never write into `lastFoundFrqOrChan` / `gScanKeepResult`.
  The audio is unchanged (the operator still hears the watch transmission
  for the full HOLD window); only the scan loop's restore-state is protected
  from the cross-VFO write that would otherwise commit VFO B's channel
  index into VFO A's MR slot when STOP fires.

#### Intelligent Dual-Watch Algorithm Detail
- Default dwell 500ms; reachable cascade values {400ms, 500ms, 600ms} per VFO
- Activity-based weighting: more active VFO gets -100ms, less active gets +100ms
- Activity decay: every 256 reports, counters *= 3/4 (count-based static
  counter wraparound, deterministic regardless of weight magnitude)
- **Activity-weighted by RX duration (v1.2.7, 2026-05-09):**
  `DUAL_WATCH_MGMT_ReportActivity(vfo, weight)` accumulates `weight`
  into the VFO's activity counter (saturating at 0xFFFF). `weight` is
  the RX duration in 10ms ticks (1..255 clamped; 0 promoted to 1). The
  app/app.c hook fires on the falling edge of `is_receiving` with the
  accumulated duration, so a 30 s QSO weights 30x more than a 1 s
  carrier blip in the dwell cascade.
- **Edge-only recomputation (v1.2.7 reclamation, 2026-05-08):** the
  dwell-time cascade + clamps live in `DUAL_WATCH_MGMT_ReportActivity`,
  which runs once per squelch-open edge. The activity counters are the
  cascade's only inputs and they are mutated only inside this function,
  so per-tick recomputation was provably redundant; the previous
  `DUAL_WATCH_MGMT_Update` and its 100 Hz hot-path call from
  `app/app.c` are gone. Same dwell behaviour, fewer cycles, -56 bytes
  flash + -4 bytes BSS reclaimed.
- **State privatised + dead clamps removed (v1.2.7 reclamation,
  2026-05-08):** `DualWatchMgmt_t` and the backing storage are now
  private to `dual_watch_mgmt.c` (renamed `s_state`, marked `static`)
  since no external module reads them; the header retains only the
  three function prototypes (Init / GetDwellTime / ReportActivity).
  The trailing `[200, 2000] ms` clamp loop and the
  `MIN_DWELL_MS` / `MAX_DWELL_MS` macros are gone -- the cascade's
  exclusive write set {400, 500, 600} sits inside the previous
  guard window, making the loop unreachable. Same behaviour;
  -12 bytes flash reclaimed.

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
- Noise floor as dotted line (every 4 pixels) -- now live (v1.2.5):
  `BANDSCOPE_Process()` self-updates `noise_floor_level` from
  `gRSSI_Histogram[gEeprom.RX_VFO].noise_floor_dbm` whenever the
  histogram's `valid` flag is set. Conversion is `(uint8_t)(dBm + 160)`,
  matching bandscope's height-byte unit (raw_RSSI / 2). The render
  gate `nf_h > 0` silently keeps the line off until the histogram
  has accumulated 100+ samples (~10 s after band change). Per-VFO
  via `gEeprom.RX_VFO`. Same authoritative noise-floor estimate
  that `smart_squelch.c:95-97` consumes for adaptive squelch.
- Height = rssi >> 5 (max 7 pixels). Reads REG_67 (RSSI).

### Spectrum Analyzer (4 modes via Star key)

Modes implemented in `app/spectrum.c` (modified egzumer/fagci code).

| # | Feature | File(s) | Makefile | Call Site | Since |
|---|---------|---------|----------|-----------|-------|
| 18 | NORM: Standard spectrum bars | app/spectrum.c | app/spectrum.o | Spectrum app, mode 0 | inherited |
| 19 | PEAK: Peak hold with decay | app/spectrum.c | app/spectrum.o | Spectrum app, mode 1 | inherited, enhanced |
| 20 | MTI: Moving Target Indicator (XOR diff) | app/spectrum.c | app/spectrum.o | Spectrum app, mode 2 | inherited, enhanced |
| 21 | VOX: Voice-Seeking Spectrum (voice probability + hop) | app/spectrum.c | app/spectrum.o | Spectrum app, mode 3 | v1.2.0, hop respects blacklist + draw threshold raised to hop threshold (50) + marker tick at y=0 v1.2.6, SIDE2 reverted to press-dispatched auto-repeat (symmetric with SIDE1) v1.2.7 |

#### Spectrum Mode System
- `spectrum_mode` cycles 0-3 via Star key (`(mode + 1) & 3`)
- Mode labels: `{"NORM", "PEAK", " MTI", "VOX "}` shown top-right
- New data arrays: `peak_hold[128]`, `prev_sweep[128]`, `noiseHistory[128]`, `voiceProb[128]`
- **Mode-cycle buffer reset (v1.2.5):** the Star-key cycle zeroes
  all four mode-private buffers via `memset` so each mode entry
  shows fresh data. Closes the cross-mode staleness path where
  PEAK ghost peaks, MTI stale-XOR diffs, and VOX stale voice-
  probability scores leaked from a prior session into a fresh
  mode entry. Single-mode operators see no change; only Star-
  cyclers benefit. Voice-hop and VOIC auto-listen, both gated on
  `voiceProb[i] >= 50`, correctly suppress until the next sweep
  populates real scores. Peak decay (1 pixel per 10 sweeps)
  becomes a no-op until peak_hold[] is repopulated.

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
- No-find cue (v1.2.7): when the loop completes without locating a `voiceProb >= 50` bin, queues `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL` so the operator hears that the press was received but no voice was detected this sweep (contrasts with the success-side ascending two-tone chord from Feature #34)

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
| 23 | Context-Aware Status Line | ui/main.c | N/A | ui/main.c:828-868 (TX/RX/scan/idle bottom-line render) | v1.0.4, re-anchored v1.2.5 |
| 24 | Boot Screen (VERSION_STRING) | ui/welcome.c | N/A | Boot sequence | v1.0.0 |
| 25 | About Screen | ui/vuurwerk_about.c/h | ui/vuurwerk_about.o | Menu > CONFIG > About (MENU_ABOUT in app/menu.c MENU_Key_MENU) | v1.0.4 |
| 26 | Categorized Menu System (6 visible + UNLOCK) | ui/vuurwerk_menu.c/h | ui/vuurwerk_menu.o | app/main.c:694, app/menu.c (5+ sites) | v1.0.9 |
| 28 | Side-Button Toast Feedback (UX parity with F+keys) | side_toast.c/h | side_toast.o | app/app.c hook after ACTION_Handle | v1.2.4 |
| 29 | Toast Notification Subsystem (1-second on-screen overlay; powers F+key shortcuts and side_toast) | toast.c/h | toast.o | app/app.c:1135 (TOAST_Tick 10ms), ui/main.c:870..878 (renderer), 18 call sites in app/main.c + side_toast.c | extracted v1.2.5 |
| 31 | F-hold Lock Toast + F+3 NO CHANNELS guard + F+* FM-mode gate + STAR long-hold SCAN ON/OFF toast (UX parity + reliability) | app/main.c (KEY_F dispatch wrapper, VUURWERK_FKeyShortcut case 3, MAIN_Key_STAR F-key branch, MAIN_Key_STAR long-hold branch) | N/A | app/main.c:756-760 (KEY_F lock toast hook), app/main.c:142-160 (F+3 NO CHANNELS guard), app/main.c:596-606 (F+* FM-mode gate), app/main.c:570-572 (STAR long-hold scan toast) | v1.2.5, STAR scan toast v1.2.7 |
| 32 | Flashlight Auto-Off Watchdog (ON/BLINK auto-extinguish after 30 min; SOS preserved) | flashlight_watchdog.c/h | flashlight_watchdog.o | app/app.c (FLASHLIGHT_WATCHDOG_Tick after FlashlightTimeSlice in APP_TimeSlice10ms) | v1.2.5 |
| 33 | CSS Scan Soft-Timeout Watchdog (30-second auto-fail with audible cue, both F+* and menu paths) | app/app.c (hook block) | N/A | app/app.c (immediately after SCANNER_TimeSlice500ms in APP_TimeSlice500ms) | v1.2.5, timeout tightened 60 s -> 30 s v1.2.7 (paired with pre-flight RSSI gate + 1-confirmation CTCSS + 120 ms dwell) |
| 34 | Audio Palette: VOX-hop "found voice" two-tone chord | audio_palette.c/h | audio_palette.o | app/spectrum.c OnKeyDown VOX hop branch (immediately before ToggleRX(true) on a voiceProb >= 50 hit) | v1.2.6 |
| 35 | Boot-Time Hardware Health Probe (BK4819 + EEPROM fault detection at boot; "BK4819 FAULT / RX/TX disabled" or "EEPROM FAULT / calib lost" welcome banner) | boot_health.c/h | boot_health.o | main.c (one call right after BK4819_Init at line 112), ui/welcome.c (fault-overlay branch at top of UI_DisplayWelcome with cause discriminator) | v1.2.6 (BK4819), v1.2.7 (EEPROM extension) |
| 36 | Live Battery Voltage During TX (TX-time battery indicator unfreeze) | battery_tx_monitor.c/h | battery_tx_monitor.o | app/app.c (BATTERY_TX_MONITOR_Tick500ms inside APP_TimeSlice500ms, immediately before the gReducedService block) | v1.2.6 |
| 37 | Backlight Fade-Out Tail (2-second taper before TurnOff) | backlight_fade.c/h | backlight_fade.o | app/app.c (BACKLIGHT_FADE_Tick500ms inside APP_TimeSlice500ms, immediately after BATTERY_TX_MONITOR_Tick500ms) | v1.2.7 |
| 38 | Backlight TX/RX activity refresh (no mid-conversation dim-out; covers TRANSMIT / RECEIVE / INCOMING / MONITOR) | backlight_fade.c/h | backlight_fade.o | app/app.c (BACKLIGHT_FADE_ArmDuringActivity inside APP_TimeSlice500ms, immediately before stock decrement at line 1440) | v1.2.7 (initial), v1.2.7.1 MONITOR coverage |
| 39 | TX Battery Sag Delta Tracker (per-TX peak voltage sag latched, displayed in About) | battery_sag.c/h | battery_sag.o | app/app.c (BATTERY_SAG_Tick500ms inside APP_TimeSlice500ms, immediately after BATTERY_TX_MONITOR_Tick500ms), ui/vuurwerk_about.c (line 7 render via BATTERY_SAG_GetLast10mV) | v1.2.7 |
| 40 | CSS Scan Status-Bar Glyph "Cs" (distinguishes CSS scan from channel scan in the status bar) | ui/status.c (in-place SCAN-indicator branch) | N/A | ui/status.c:90-93 (new branch inside the SCAN-indicator block, between the SCANWATCH "S+W" branch and the MR-channel scan-list branch) | v1.2.5, Last Updated v1.2.7 (CSS scanner robustness pass) |
| 41 | CSS Scan FOUND beep (audible 1 kHz cue on tone-find for both F+\* and menu paths) | app/app.c (state-edge tracker after SCANNER_TimeSlice10ms) | N/A | app/app.c (latch + edge-detect after SCANNER_TimeSlice10ms in APP_TimeSlice10ms) | v1.2.5, Last Updated v1.2.7 (CSS scanner robustness pass; CTCSS now 1-confirmation matching DCS, 120 ms dwell, pre-flight RSSI gate) |
| 42 | Quiet Backlight PWM (~6.7 kHz carrier above 3 kHz audio passband; removes outbound 1 kHz whine when screen is dimmed during TX) | main.c (one VUURWERK marker block re-programming PWM_PLUS0_CLKSRC after BOARD_Init) | N/A | main.c (single register-write hook between GAIN_STAGING_Init() and BOOT_Mode_t capture, inside `// === VUURWERK v1.2.7 ===` markers) | v1.2.7 |

#### CSS Scan FOUND Beep Algorithm Detail
- **Hook site:** single conditional block in `app/app.c` immediately after the stock `SCANNER_TimeSlice10ms()` call in `APP_TimeSlice10ms`, wrapped in `// === VUURWERK v1.2.7 CSS scan FOUND beep ===` markers. app/app.c is one of the three permitted hookable files. Symmetric counterpart to the v1.2.5 Feature #33 CSS scan soft-timeout FAILED watchdog at line 1590-1601.
- **Predicate:** `SCANNER_IsScanning() && gScanCssState == SCAN_CSS_STATE_FOUND && gScanUseCssResult`. The `gScanUseCssResult` qualifier ensures the beep only fires after the scanner.c:457/467 path (a real BK4819 CSS scan match) and never on the dead `#ifndef ENABLE_NO_CODE_SCAN_TIMEOUT` self-FOUND path (Makefile sets the flag, so that block is excluded). `SCANNER_IsScanning()` covers both the F+\* path (gScreenToDisplay == DISPLAY_SCANNER) and the menu STAR path (gCssBackgroundScan).
- **One-byte latch:** `static uint8_t s_beeped` in BSS, zero-initialised at boot. Set on the FOUND-edge beep emission, cleared every tick the predicate is false (i.e., when SCANNER_IsScanning() returns false on EXIT/dismiss). Re-arms automatically for the next scan launch.
- **Beep choice:** `BEEP_1KHZ_60MS_OPTIONAL` -- 60 ms single tone at 1 kHz. Distinct from the FAILED watchdog's `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL` (low-pitched double beep). Higher pitch + single beep = "good news" symmetry with the FAILED double-beep "bad news" cue. OPTIONAL gating means operators with `gEeprom.BEEP_CONTROL` cleared hear nothing (parity with the FAILED beep), preserving silent-scan operation.
- **Why it matters:** before this hook, a successful CSS-scan tone find was silent. The operator had to be staring at the SCANNER screen ("CTC: 88.5") or the menu screen (cursor snap) to know the scan completed. CSS scans can run for tens of seconds on a quiet repeater so glance-away is the common case. Pairs with Feature #33's FAILED beep so the operator now gets symmetric audible feedback for both scan terminations -- a tone find or a 60-second timeout.
- **LAW conformance:** the only stock-touched file is `app/app.c` (one of three permitted hookable files; one marker-wrapped block, no new include). No new BK4819 register access (the beep flows through the existing `gBeepToPlay` -> `AUDIO_PlayBeep` dispatcher at app/app.c:1394-1397). No new exports. LAW 4 preserved (no math, no FPU, no modulo).

#### Quiet Backlight PWM Algorithm Detail
- **Hook site:** single statement in `main.c` inside a new `// === VUURWERK v1.2.7 Quiet backlight PWM ===` marker block, immediately after the existing v1.0.9 init block (after `GAIN_STAGING_Init()`, before the `BOOT_Mode_t BootMode = BOOT_GetMode()` capture). main.c is one of the three permitted hookable files. The hook fires after `BOARD_Init()` (which transitively invokes `BACKLIGHT_InitHardware`) and before any `BACKLIGHT_TurnOn` call site (lines 175 / 195 / 203).
- **Operation:** `PWM_PLUS0_CLKSRC = (PWM_PLUS0_CLKSRC & 0x0000FFFFu) | (7u << 16);` -- mask out the upper 16 bits (the prescaler field) and OR in the new value. Stock `BACKLIGHT_InitHardware` writes prescaler 46 (~1 kHz at the 48MHz/1024 source). The new prescaler 7 yields ~6.7 kHz, well above the radio's 3 kHz audio passband. The mask-then-OR pattern preserves the lower 16 bits that `BACKLIGHT_InitHardware` set; a NUNU-style raw `|=` would not work as a hook because `46 | 7 == 47` (a one-bit change that does not effectively re-program the prescaler).
- **Why mask-then-OR:** the `|=` pattern in the stock initialiser only works because the register is in a known-zero state at boot. The VUURWERK hook runs after the stock initialiser has already populated the prescaler bits, so an OR-only override would AND with the existing bits instead of replacing them. The mask-first pattern guarantees the new value takes effect deterministically.
- **Sequenced ownership:** PWM_PLUS0_CLKSRC has implicit sequenced ownership: `BACKLIGHT_InitHardware` writes once at boot inside `BOARD_Init`, then this VUURWERK hook re-writes once before the main loop. `BACKLIGHT_TurnOn` / `SetBrightness` / `TurnOff` never touch CLKSRC (only CH0_COMP), so no runtime conflict surface exists. Same shape as `BOOT_HEALTH_Probe` reading `REG_00` after `BK4819_Init` writes it.
- **Why it matters:** the stock 1 kHz backlight PWM creates a square-wave electrical perturbation that couples into the radio's analogue path -- microphone preamp, mic bias, or PA supply rail. Anyone listening to a VUURWERK transmission while the operator's backlight is dimmed (any value below maximum brightness, where `CH0_COMP` saturates the duty cycle) hears a 1 kHz subaudible whine modulating the carrier. NUNU (kamilsss655) bench-validated and fixed this in 2024 by raising the carrier to 6 kHz; egzumer / DualTachyon / fagci all still ship the audible-whine artefact. VUURWERK now matches NUNU's TX-audio quality on this dimension. Mission-aligned with existing TX-side intelligence (Feature #1 TX Soft Start, Feature #2 CTCSS Lead-In, Feature #3 TX Audio Compressor): completes the picture by removing the last known hardware-coupling artefact in the outbound audio path.
- **Operator-facing impact:** zero change for operators with backlight at full brightness (the saturated duty cycle eliminates the square-wave modulation source). For operators with the backlight at any partial level, receivers monitoring their transmissions stop hearing the whine. The operator themselves notices nothing; the benefit is on the OUTBOUND audio path. No operator action required; no menu item; no toast.
- **LAW conformance:** `bsp/dp32g030/pwmplus.h` (header, byte-identical with egzumer parent) and `driver/backlight.c` (DualTachyon stock, byte-identical with egzumer parent) are both untouched. The single hook lands in `main.c` (one of three permitted hookable files) inside an explicit VUURWERK marker block. PWM_PLUS0_CLKSRC is a DP32G030 peripheral register; the binding LAW 2 register-ownership table covers BK4819 only. No new BK4819 access. LAW 4 preserved (no FPU, no heap, no software-divide pull -- the calculation is compile-time-constant).

#### CSS Scan Status-Bar Glyph Algorithm Detail
- New if-else branch at `ui/status.c:90-93` inside the SCAN-indicator block, wrapped in `// === VUURWERK v1.2.7 CSS scan glyph ===` markers.
- Predicate: `SCANNER_IsScanning()` (CSS scan flavour true while `gScanCssState != SCAN_CSS_STATE_OFF`). When matched, the SCAN-slot string `s` is set to `"Cs"` (CTCSS / DCS scan) instead of falling through to the generic `"S"` glyph.
- Branch ordering: SCANWATCH "S+W" retains priority (state-machine-mutually-exclusive with CSS scan; placement preserves observed behaviour). The new "Cs" branch precedes the MR-channel "1"/"2"/"\*" branch; the previously redundant `&& !SCANNER_IsScanning()` clause on the MR-channel branch is removed because the new branch now catches the CSS case before the MR test runs.
- Slot width: 10 px. "Cs" paints 13 px (small font 6 px wide + 1 px char-spacing); the 3-px tail overruns into the ENABLE_VOICE-gated BITMAP_VoicePrompt slot which is null-buffer in this build (ENABLE_VOICE not in Makefile defines). No collision with subsequent DWR / VOX renderers.
- **Why it matters:** F+\* CSS scan, R_CTCS / R_DCS submenu STAR, and channel scan in MR mode without a scan list all previously surfaced as plain "S" in the status bar -- three operations collapsed into one glyph. Operators glancing at the status bar could not tell whether F+\* fired by accident, what mode they returned to mid-scan, or what EXIT was about to dismiss. With "Cs" surfaced, the CSS-scan flavour is unambiguous from the status bar alone. Combined with v1.2.5's Feature #33 CSS scan soft-timeout watchdog and the ui/menu.c CSS scan dot animation, CSS scans now surface state on the status bar, in the menu, and through the audible FOUND/FAILED cues.
- **LAW conformance:** ui/status.c is the pragmatic-LAW-1 file already carrying SCANWATCH "S+W" and Signal Quality "Q" + bars modifications (FEATURES.md:326,590 binds against current VUURWERK head, not egzumer parent). New modification adopts explicit `// === VUURWERK ===` markers per the LAW 1 marker discipline. No new register access. No new exports.

#### Backlight TX/RX Activity Refresh Algorithm Detail
- Companion entry `BACKLIGHT_FADE_ArmDuringActivity()` in `backlight_fade.c/h`. Hook fires once per 500 ms from `APP_TimeSlice500ms` immediately BEFORE the stock decrement block at `app/app.c:1440`, inside a `// === VUURWERK v1.2.7 ===` marker block. driver/backlight.c and driver/backlight.h remain byte-identical with egzumer parent (LAW 1).
- Predicate: `(gCurrentFunction == FUNCTION_TRANSMIT && (gSetting_backlight_on_tx_rx & BACKLIGHT_ON_TR_TX))` OR `((gCurrentFunction == FUNCTION_RECEIVE || gCurrentFunction == FUNCTION_INCOMING || gCurrentFunction == FUNCTION_MONITOR) && (gSetting_backlight_on_tx_rx & BACKLIGHT_ON_TR_RX))`. When matched, calls `BACKLIGHT_TurnOn()` which reloads `gBacklightCountdown_500ms` to the user's `BACKLIGHT_TIME` value via the public API. `FUNCTION_MONITOR` (forced-squelch-open via SIDE1 or any key bound to `ACTION_OPT_MONITOR`) is treated as RX (matches the `FUNCTION_RECEIVE || FUNCTION_MONITOR` convention used at `app/app.c:777` and `:1353`); the v1.2.7.1 widening from two to three RX-side enum alternatives was 0-byte cost under `-Os` `-flto=auto` (LTO absorbed the new comparison into the existing OR chain) and closes the mid-monitor dim-out gap where operators forcing squelch open to listen for a weak signal would lose the backlight before the monitor session ended.
- Order matters: refresh fires BEFORE stock decrement so the countdown never reaches zero during active TX/RX. Stock decrement runs unchanged afterward; on TX/RX exit the next tick lets the countdown run down normally and Feature #37 (fade tail) renders the post-state taper.
- Operators who clear the per-mode bit (default for both directions) see zero behavioural change -- the predicate fails and the hook returns immediately.
- **Why it matters:** stock egzumer / DualTachyon / kamilsss655 (NUNU) / fagci all carry the same one-shot `BACKLIGHT_TurnOn` at TX entry (`functions.c:225-227`) and RX entry (`app/app.c:458-459`); the timeout decrements unconditionally during the state, so a TX or RX longer than `BACKLIGHT_TIME` extinguishes the screen mid-conversation. Operators historically worked around this by setting `BACKLIGHT_TIME=7` (always on), trading battery life for visibility. VUURWERK now extends the existing one-shot to a state-duration arm-and-hold without forcing the always-on workaround.
- **LAW conformance:** `backlight_fade.c/h` are VUURWERK-original (GPL v3). Only stock-touched file is `app/app.c` (permitted hook site; one marker-wrapped one-line call). No new BK4819 register access; PWM_PLUS0_CH0_COMP touched only via `BACKLIGHT_TurnOn -> SetBrightness` public API.

#### Backlight Fade-Out Tail Algorithm Detail
- Companion module `backlight_fade.c/h`. Hook fires once per 500 ms from `APP_TimeSlice500ms` immediately after `BATTERY_TX_MONITOR_Tick500ms()`, inside a `// === VUURWERK v1.2.7 ===` marker block. driver/backlight.c and driver/backlight.h are unmodified (LAW 1); the hook reads the public `gBacklightCountdown_500ms` and writes brightness only via the public `BACKLIGHT_SetBrightness` API.
- Tail window: `BACKLIGHT_FADE_TAIL_TICKS = 4` (2.0 s). When the post-decrement countdown is in `[1, 4]` AND `BACKLIGHT_IsOn()` is true, brightness ramps down. The stock app/app.c:1440-1446 block decrements first and calls `BACKLIGHT_TurnOff()` on the transition to zero, so the fade hook never collides with the off transition.
- Linear ramp: `step = TAIL - cd + 1` (1..4), `cut = (range * step) >> 2` with `range = MAX - (MIN+1)`. Shift-by-2 keeps the divide free on Cortex-M0 (LAW 4). Target = `MAX - cut`, clamped to `MIN+1`.
- Cancellation: any key press routes through `BACKLIGHT_TurnOn()` -> brightness back to MAX and countdown reset. Edge cases: BACKLIGHT_TIME=7 (always on) leaves countdown at 0 -- early return; MAX <= MIN+1 (no headroom) -- early return; gAskToSave / gCssBackgroundScan / MENU_ABR pause the stock decrement -- fade pauses with it.
- **Why it matters:** stock transitions abruptly MAX -> MIN/0 in a single tick; the taper says "tap any key" without requiring the operator to remember the timeout. No UV-K5 fork ships a fade-out (NUNU lifted PWM to 6 kHz instead).
- **LAW conformance:** new files VUURWERK-original (GPL v3). Only stock-touched file is `app/app.c` (permitted hook site; one #include + one marker-wrapped one-line call). No BK4819 register access (PWM_PLUS0_CH0_COMP via existing API only).

#### Live Battery Voltage During TX Algorithm Detail
- **Problem:** the stock egzumer / DualTachyon / kamilsss655 pattern at `app/app.c:1467` gates battery sampling on `gCurrentFunction != FUNCTION_TRANSMIT`. The on-screen battery icon (`gBatteryDisplayLevel`) and voltage/percent text (`gBatteryVoltageAverage`) are computed from the rolling `gBatteryVoltages[4]` ring; during any TX the ring is frozen and the display lies.
- **Single tick:** `BATTERY_TX_MONITOR_Tick500ms()` runs every 500 ms inside `APP_TimeSlice500ms` (one of three permitted hookable files; hook lives in a `// === VUURWERK v1.2.6 ===` marker block immediately before the existing `if (gReducedService)` block). Early-returns when `gCurrentFunction != FUNCTION_TRANSMIT`.
- **TX-time path:** calls the existing `BOARD_ADC_GetBatteryInfo` API (LAW 1 preserved on `driver/adc.h` and `driver/adc.c`) to advance the next slot in `gBatteryVoltages[]` using the same `gBatteryVoltageIndex` rotation the non-TX path uses. Index advance is `if (++i > 3) i = 0` (LAW 4: no Cortex-M0 software divide). Then calls `BATTERY_GetReadings(true)` to recompute `gBatteryVoltageAverage`, `gBatteryDisplayLevel`, and trigger `gUpdateStatus`.
- **Cadence interaction:** the TX hook fires 2x as often as the non-TX hook (every 500 ms vs every 1 s). A 2-second TX completely refreshes the 4-sample ring with TX-loaded voltages; after PTT release, the non-TX path naturally restores the ring with unloaded voltages within ~2 s. The transient post-TX low-skewed average IS the operator-actionable feedback that the battery sagged under load.
- **Shutdown semantics preserved:** the low-battery shutdown check at `app/app.c:1449-1457` reads `gBatteryCurrentVoltage` (separate variable), not `gBatteryVoltageAverage`. This module never touches `gBatteryCurrentVoltage`, so reduced-service triggering is unchanged.
- **Why it matters:** operators on long key-down (DX contests, repeater nets, TX-test cycles) finally see real-time battery sag instead of a frozen reading. First UV-K5 firmware to surface live battery telemetry during transmission.
- **LAW conformance:** new files `battery_tx_monitor.c/h` are VUURWERK-original (GPL v3 + commercial dual-license). The only stock-touched file is `app/app.c` (one of the three permitted hookable files; one `#include` plus one marker-wrapped 1-line call). `driver/adc.h`, `driver/adc.c`, `helper/battery.c`, `board.c`, `ui/status.c` are all unchanged. No new BK4819 register access (SARADC peripheral access flows through the existing `driver/adc.h` API).

#### TX Battery Sag Delta Tracker Algorithm Detail
- **Companion module** `battery_sag.c/h`. Runs every 500 ms inside `APP_TimeSlice500ms` immediately after `BATTERY_TX_MONITOR_Tick500ms()`, inside a `// === VUURWERK v1.2.7 TX battery sag tracker ===` marker block. The order matters: Feature #36 must update `gBatteryVoltageAverage` first so this tracker samples a live value during TX rather than the frozen pre-PTT reading.
- **State** is five BSS variables (~10 bytes after padding): `s_was_tx` (bool/uint8 edge detector), `s_pre_tx` (uint16, 10mV) baseline at the most recent TX entry edge, `s_min_tx` (uint16, 10mV) running minimum during the current TX, `s_last_sag` (uint16, 10mV) latched delta from the most recent completed TX, `s_last_idle_avg` (uint16, 10mV) clean-idle baseline mirrored from `gBatteryVoltageAverage` on every idle tick (v1.2.7.2 accuracy fix; LTO whole-program layout absorbed the new uint16 into existing BSS padding so section-level size is unchanged).
- **Tick logic (v1.2.7.2):** `is_tx = (gCurrentFunction == FUNCTION_TRANSMIT)`. On TX-entry edge (`is_tx && !s_was_tx`) capture `s_pre_tx = s_last_idle_avg` (the clean-idle baseline from the most recent non-TX tick) and `s_min_tx = gBatteryVoltageAverage` (the first-TX-tick blended value as the initial minimum, which subsequent during-TX ticks refine downward via `< s_min_tx`). During TX (`is_tx`) update `s_min_tx` if the live average dropped. On TX-exit edge (`!is_tx && s_was_tx`) latch `s_last_sag = (s_pre_tx >= s_min_tx) ? (s_pre_tx - s_min_tx) : 0`. The unsigned-subtract guard defends against the pathological power-on-first-TX edge case (`s_last_idle_avg == 0` from BSS init when no prior idle tick has loaded the mirror) by latching `s_last_sag = 0`. Idle-baseline maintenance: `if (!is_tx) s_last_idle_avg = gBatteryVoltageAverage;` runs unconditionally on idle ticks, keeping the baseline current.
- **Why the baseline source matters:** Feature #36 (`BATTERY_TX_MONITOR_Tick500ms`) runs FIRST in the same 500 ms tick (line 1476) and advances the 4-sample ring with a fresh TX-loaded ADC reading. `gBatteryVoltageAverage` is then `mean(3 idle samples + 1 TX sample)` -- already biased ~25% toward TX-loaded steady-state. A same-tick read of `gBatteryVoltageAverage` as the sag baseline (the v1.2.7.0/.1 behaviour) under-reported true sag by ~25% of the first-tick TX drop. The v1.2.7.2 fix maintains `s_last_idle_avg` on idle ticks (when `gBatteryVoltageAverage` is pure idle) and captures from it on the TX-entry edge, restoring the true-idle baseline.
- **Display:** `BATTERY_SAG_GetLast10mV()` exposed via `battery_sag.h`. About-screen line 7 (the only empty slot per the about-screen-slot audit landed as a side artifact of `driver/flash.h` research) renders `"TX sag NNNNmV"` with `NNNN = s_last_sag * 10` (10mV units converted to mV for operator readability). Zero-state ("no TX since boot") displays `"TX sag 0mV"`, which is honest.
- **Operator value:** answers "is my battery getting tired?" with a quantitative number that trends across sessions and aligns with the published thresholds. Healthy: 50-100 mV. High load on a tired pack: 200-300 mV. Aging: 400+ mV. The baseline-source choice (clean-idle mirror rather than same-tick average) is what aligns the displayed numbers with the documented thresholds; operators applying the thresholds to displayed numbers make correct diagnostic decisions.
- **LAW conformance:** new files `battery_sag.c/h` are VUURWERK-original (GPL v3 + commercial dual-license). Only stock-touched file is `app/app.c` (one of three permitted hookable files; one `#include` plus one marker-wrapped one-line call). `ui/vuurwerk_about.c` is already a VUURWERK-modified file. `driver/adc.h`, `driver/adc.c`, `helper/battery.c`, `board.c` byte-identical with their pre-feature state. No new BK4819 register access (consumes only `gBatteryVoltageAverage` from helper/battery.c -- the cached value Feature #36 keeps live).

#### Boot-Time Hardware Health Probe Algorithm Detail
- **Single probe at boot:** `BOOT_HEALTH_Probe()` runs exactly once per power-on, the line immediately after `BK4819_Init()` returns. Two cause-independent checks fire in sequence: a BK4819 SPI liveness check and an EEPROM I2C liveness check. Both are non-mutating and bounded.
- **BK4819 SPI check:** reads `BK4819_REG_00`. A wedged SPI bus (the post-ESD failure mode the proposal targets) returns `0xFFFF` for every read; a healthy chip returns the just-written control-register value (BK4819_Init's last write to REG_00 was `0x0000`). The fault bit is set when the readback equals `0xFFFF` (the floating-SPI signature) -- conservative on purpose: catches the operator-confusing failure mode without depending on any undocumented chip-ID semantics.
- **EEPROM I2C check (v1.2.7):** reads 8 bytes from address 0x1F40 (the factory-programmed battery-calibration page) and checks three distinct fault signatures, then performs a second read of the same page for two-pass consistency. (a) `(b[0] & b[1] & ... & b[7]) == 0xFF` catches the floating-bus / blanked-chip case: when no slave drives SDA, the external pull-up carries every read to 1, so every byte returns 0xFF. (b) `(b[0] | b[1] | ... | b[7]) == 0` catches the stuck-low SDA case: the bit-bang `I2C_Read` at `driver/i2c.c:47-87` reads SDA via `GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA)`, so a slave (or external short to GND) holding SDA low while the master clocks SCL produces a 0 on every clocked bit and 0x00 across the page. Both AND/OR cascades are single-cycle ORS / ANDS instructions on Cortex-M0, no software-divide pull. (c) **Two-pass consistency check (v1.2.7, 2026-05-07):** a second `EEPROM_ReadBuffer` of the same address into a separate stack buffer, then `memcmp(b, b2, 8) != 0` catches the garbled / intermittent / marginal-bus failure mode where reads are NEITHER all-0xFF NOR all-0x00 but vary across passes -- stochastic bus noise, marginal slave that occasionally NACKs / produces wrong data, ESD-induced glitches that haven't fully settled, marginal voltage rails on the EEPROM Vcc, signal-integrity issues from a damaged trace. The 24LC64 is static storage, the bit-bang driver is deterministic, and no writer runs between the two reads (boot_health is invoked before main loop runs), so two consecutive reads of the same address must be byte-identical on healthy hardware (NXP UM10204 section 3.1.6: single-transaction success does not guarantee bus integrity; multi-pass attestation is canonical for safety-critical bus probes). `memcmp` is already pulled by `driver/eeprom.c:50` (EEPROM_WriteBuffer's pre-write coalesce) so the linker contributes zero new bytes for the function itself; only the call-site adds flash. All three signatures map to the same FAULT_BIT_EEPROM flag because the welcome-screen banner ("EEPROM FAULT / calib lost") is general enough to cover any of the failure modes. settings.c:295-301 partial-recovery invariants (gBatteryCalibration[0]/[1] never both zero on a factory unit, never 0xFFFF on a healthy one) bound false-positive risk to zero on every UV-K5 that ships from Quansheng. NXP UM10204 section 3.1.16 ("Bus clear") documents stuck-low SDA as a canonical I2C failure mode; no upstream UV-K5 fork ships any of the three signature checks. Boot-time impact: one extra ~720us EEPROM page read at the bit-bang rate, imperceptible to operators.
- **One-byte BSS state:** `static uint8_t s_fault` lives in BSS, zero-initialised at boot. Bit 0 = BK4819 fault; bit 1 = EEPROM fault. Four exported helpers: `BOOT_HEALTH_Probe()` (writes the bitmask), `BOOT_HEALTH_HasFault()` (any-fault predicate), `BOOT_HEALTH_HasBk4819Fault()` (bit 0 reader), `BOOT_HEALTH_HasEepromFault()` (bit 1 reader). Caller-side BSS zero supplies the "no fault yet" default if the probe is somehow skipped.
- **Welcome-screen integration:** `UI_DisplayWelcome()` opens with a `BOOT_HEALTH_HasFault()` check. On fault, the title is "EEPROM" iff EEPROM is faulty AND BK4819 is not (the more critical BK4819 fault wins on simultaneous failure since it disables RX/TX outright); otherwise "BK4819". The subtitle is "calib lost" for EEPROM-only faults, "RX/TX disabled" for the BK4819 path. On no-fault: falls through to the three pre-existing welcome-screen modes (FULL_SCREEN / VOLTAGE / VUURWERK branding).
- **Why it matters:** without this probe, a UV-K5 with a wedged BK4819 SPI bus boots normally (welcome screen renders, menu works, keys respond) and yet every RF feature silently misbehaves -- squelch never opens or never closes, RSSI reads `0xFFFF`, CTCSS detection chases noise, gain staging slams gain to either rail. A UV-K5 with a wedged-I2C EEPROM bus boots normally and yet the battery icon is permanently empty (because `helper/battery.c:101` divides by `gBatteryCalibration[3] == 0xFFFF`, pinning voltage near zero), the S-meter renders raw noise (because `gEEPROM_RSSI_CALIB[*]` is all 0xFF), and gain staging walks gain to extreme rails. In both cases operators blame the firmware. The conservative fault check makes the failure visible at the right layer (hardware) at the right moment (boot, before anything tries to use the chip), with a diagnostic subtitle that points the operator at the correct repair path.
- **LAW conformance:** the only stock-touched file outside the new `boot_health.c/h` is `main.c` (one of the three permitted hookable files; the call sits inside the existing VUURWERK-marker initialisation neighbourhood at line 111-112) and `ui/welcome.c` (already a VUURWERK-modified file carrying the branding boot screen). REG_00 ownership unchanged. EEPROM access is via the public `EEPROM_ReadBuffer` API only; `driver/eeprom.h` and `driver/eeprom.c` byte-identical with parent.

#### Audio Palette Algorithm Detail
- **Single entry point** today: `AUDIO_PALETTE_PlayFoundVoice()`. Synchronous, returns when both tones have finished. No state, no flush dispatcher; the caller is already in a serialised keyboard-handler context where no other module can be mid-tone.
- **Chord:** 800 Hz for 50 ms then 1200 Hz for 50 ms (100 ms total). Ascending pair distinguishes "found" from any descending or single-tone stock beep, matching the operator intuition that a successful hop is "good news" rising in pitch.
- **Audio path discipline:** wraps the two `BK4819_PlayTone()` calls with `AUDIO_AudioPathOn()` / `AUDIO_AudioPathOff()` (both inline GPIOC bit ops in `audio.h`), and finishes with `BK4819_TurnsOffTones_TurnsOnRX()` so REG_70 returns to zero and REG_30 is reset to the standard RX-link configuration before the caller's `ToggleRX(true)` re-engages live RX.
- **Register touch:** REG_70 / REG_71 / REG_30 / REG_50 driven only via the existing `BK4819_PlayTone` / `BK4819_ExitTxMute` / `BK4819_TurnsOffTones_TurnsOnRX` API, mirroring the pattern `audio.c::AUDIO_PlayBeep` already uses. No new BK4819 ownership claims.
- **Why it matters:** the spectrum VOX-mode UP/DOWN hop is VUURWERK's flagship "find a talking station faster than the operator can scan visually" capability. Stock-derived behaviour fired silently on hit; the operator had to watch the bin marker to know the hop succeeded. The 100 ms ascending chord is short enough to play before the hopped-to RX audio engages, so the operator hears: "ding-dong, [voice]" -- a clear punctuation that the radio just changed bins on its own. No other UV-K5 fork couples a custom audio cue to spectrum-mode hop transitions.
- **LAW 1 preserved:** the only stock-touched site is `app/spectrum.c` which already carries 138 lines of VUURWERK additions (no marker pattern is used by this file because VUURWERK has functionally adopted it). The companion `audio_palette.c/h` lives entirely in dedicated VUURWERK files.

#### Flashlight Auto-Off Watchdog Algorithm Detail
- **Tick:** `FLASHLIGHT_WATCHDOG_Tick()` runs every 10ms inside the
  pre-existing `#ifdef ENABLE_FLASHLIGHT` block of `APP_TimeSlice10ms`,
  immediately after `FlashlightTimeSlice()`. Reads `gFlashLightState`
  (uint8 enum from app/flashlight.h).
- **State machine:** three statics -- `prev_state` (uint8),
  `prescaler` (uint8, 0..99 ticks = 1 second),
  `seconds_on` (uint16, max 65535 sec = ~18 hours).
- **Mode-change reset:** any `gFlashLightState` change (operator
  taps the side button, cycling OFF -> ON -> BLINK -> SOS -> OFF)
  zeroes both counters. Tap-to-extend always works.
- **Excluded modes:** `FLASHLIGHT_OFF` and `FLASHLIGHT_SOS` exit
  the tick immediately. SOS is intentionally never auto-killed --
  emergency signaling must remain indefinite.
- **Timeout:** at `FW_TIMEOUT_SEC = 1800` (30 minutes), the watchdog
  forces `gFlashLightState = FLASHLIGHT_OFF` and clears
  `GPIOC_PIN_FLASHLIGHT` via the existing `GPIO_ClearBit` macro
  (matches app/flashlight.c:62 idiom). No BK4819 register access.
- **Silent feedback:** no toast on auto-off; the LED going dark IS
  the feedback (parity with side_toast.c:55-59 rationale that
  flashlight actions are self-evident from the LED). Saves
  string-pool bytes.
- **Why it matters:** UV-K5 belt-clip / pocket / glove-box carry
  routinely activates SIDE1 / SIDE2. With FLASHLIGHT bound (stock
  default for SIDE1 short-press), an unnoticed bump leaves the LED
  on for hours; ~50mA on the ~1500mAh stock pack flat-lines an
  unnoticed UV-K5 in ~30 hours. No other UV-K5 fork ships
  flashlight auto-off.

#### CSS Scan Soft-Timeout Watchdog Algorithm Detail
- **Hook site:** single conditional block in `app/app.c` immediately
  after the stock `SCANNER_TimeSlice500ms()` call in
  `APP_TimeSlice500ms`, wrapped in `// === VUURWERK v1.2.5 ... ===`
  markers. app/app.c is one of the three permitted hookable files.
- **Trigger:** `gScanCssState == SCAN_CSS_STATE_SCANNING &&
  gScanProgressIndicator > 120`. 120 ticks of 500 ms = 60 seconds
  wall-clock. Threshold is uint8-wrap-safe (uint8_t wraps at 256).
- **Action:** writes `gScanCssState = SCAN_CSS_STATE_FAILED`,
  queues `gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL`,
  raises `gUpdateStatus` and `gUpdateDisplay`. No direct teardown
  -- the stock state machine handles BK4819 cleanup and
  `gCssBackgroundScan` clearing on the next 10 ms tick via
  SCANNER_TimeSlice10ms's default branch (scanner.c:497-499).
- **F+\* path UI:** stock ui/scanner.c:73-77 renders "SCAN FAIL."
  when state==FAILED. No new UI bytes.
- **Menu path UI:** ui/menu.c's dot-animation guard
  `gCssBackgroundScan` becomes false on next tick, animation stops.
  Beep is the audible feedback.
- **Wrap safety:** after state=FAILED, scanner.c's increment guard
  `gScanCssState < SCAN_CSS_STATE_FOUND` becomes false, so
  `gScanProgressIndicator` stops advancing and never wraps past
  255 to 0.
- **Re-arm:** `SCANNER_Start` resets `gScanProgressIndicator = 0`
  (scanner.c:368) on every fresh launch, so each F+\* or menu STAR
  invocation gets a fresh 60-second window.
- **Composes with Feature #31 F+\* FM-mode gate:** the gate
  prevents AM/USB invocations from ever reaching the watchdog.
  FM invocations enter SCANNER_Start, reset the indicator, and
  (if the band is silent) hit the watchdog at 60 s.
- **Why it matters:** CSS scan launched on a silent simplex
  frequency, an out-of-range repeater, or any FM channel without
  a CTCSS/DCS tone runs forever under the current Makefile
  (`ENABLE_NO_CODE_SCAN_TIMEOUT=1`). The egzumer parent's only
  alternative is a 16-second hard timeout (=0) which is too
  short. 60-second soft timeout with audible cue threads the
  needle. No other UV-K5 fork (DualTachyon / egzumer / kamilsss655
  / fagci) ships this watchdog.

#### F-hold Lock Toast + F+3 NO CHANNELS Guard + F+* FM-mode Gate Algorithm Detail
- **F-hold lock toast (KEY_F dispatch hook):** captures
  `gEeprom.KEY_LOCK` before `GENERIC_Key_F(...)` and emits
  `TOAST_Show("LOCKED")` / `TOAST_Show("UNLOCKED")` only when the
  post-call value differs. `GENERIC_Key_F` is byte-identical with
  egzumer parent; the stock helper itself only mutates the lock on
  long-press *and* on the main display *and* not transmitting, so
  the toast naturally fires on exactly the right trigger -- no
  false positives from idle taps, no double-toasts when the helper
  short-circuits.
- **F+3 NO CHANNELS guard (VUURWERK_FKeyShortcut case 3):** captures
  `IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)` before *and* after
  `COMMON_SwitchVFOMode()`. When the helper silently no-ops because
  `RADIO_FindNextChannel(...)` returned `0xFF` (empty channel
  memory), the pre/post comparison surfaces `"NO CHANNELS"` instead
  of the misleading `"VFO MODE"` toast that previously announced
  the mode the operator pressed F+3 to leave.
- **F+* FM-mode gate (MAIN_Key_STAR F-key branch):** post-NOAA,
  pre-`SCANNER_Start(true)` early-return guard. When
  `gTxVfo->Modulation != MODULATION_FM`, emits
  `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL` plus `TOAST_Show("FM ONLY")`
  and returns without launching the SCANNER screen or mutating
  `CROSS_BAND_RX_TX`. Closes a long-standing UV-K5 reliability bug:
  the BK4819 CSS detector registers (REG_68 / REG_69 / REG_6A) are
  populated only by the FM demodulator, so AM/USB invocation under
  the current `ENABLE_NO_CODE_SCAN_TIMEOUT=1` Makefile setting
  produces an unbounded futile scan. Mirrors the gate that already
  exists for the menu STAR path at `app/menu.c:1611-1615`. No
  upstream fork (DualTachyon / egzumer / kamilsss655 / fagci) ships
  this gate.
- **STAR long-hold SCAN ON/OFF toast (v1.2.7, MAIN_Key_STAR
  long-hold branch):** one `TOAST_Show()` call wrapped in
  `// === VUURWERK v1.2.7 STAR long-hold scan toast ===` markers,
  placed immediately after `ACTION_Scan(false)` in the STAR
  long-press branch. Reads `gScanStateDir` post-toggle and emits
  `"SCAN ON"` or `"SCAN OFF"`. Closes the last UX-parity gap on
  the main screen: every scan-toggle gesture (SIDE1/SIDE2
  ACTION_OPT_SCAN binding via `side_toast.c:33-34`, STAR long-hold
  via this hook) now reports identical 1-second toast. String
  literals `"SCAN ON"`/`"SCAN OFF"` overlap byte-for-byte with
  `side_toast.c:34` so LTO pools them at zero string cost;
  measured cost was +28 bytes flash (matching the +12..+25 byte
  estimate range, slightly above due to ARM Thumb-2 ternary
  emission overhead). The most discoverable scan-toggle gesture
  on the radio is no longer silent.
- Reuses existing `TOAST_Show()` API. No BK4819 register access.
  String literals `"LOCKED"` / `"UNLOCKED"` overlap byte-for-byte
  with `side_toast.c:38` so LTO pools them at zero string cost;
  `"NO CHANNELS"` and `"FM ONLY"` are genuinely new.
- THE ONE LAW preserved: `app/common.c`, `app/common.h`,
  `app/generic.c`, `app/action.c`, `app/scanner.c`, `app/scanner.h`
  are byte-identical with egzumer parent. All hooks live in
  `app/main.c` (one of the three permitted hook files).

#### Categorized Menu Algorithm Detail
- 7 categories: RECEIVE (7), TONE (7), TX/TRANSMIT (13), SCAN (7 incl SCANWATCH), CHANNEL (5), CONFIG (16), UNLOCK (hidden, 9)
- `GetCategoryForMenuId()`: hard-coded switch mapping every `MENU_*` ID to category
- `SelectCategory()`: walks `MenuList[]`, builds filtered index array (`gCategoryItems[]`, max 20)
- UNLOCK category visible only when `gF_LOCK=true`
- **Per-category position memory** (v1.2.5): `sCategoryLastIdx[MCAT_COUNT]` BSS array remembers the last visited item index inside each category. Saved on `EnterCategoryPicker()` (uses departing `gSelectedCategory` as the slot key); restored on `SelectCategory()` (clamped against the rebuilt `gCategoryItemCount`). Picker cursor (`gCategoryPickerCursor`) is also parked on the just-left category for fast re-entry, clamped against `GetVisibleCategoryCount()`. CSS-scan position override at `app/menu.c:107-112` runs after restore and so still wins. Power-on resets to BSS zeroes (no EEPROM persistence).
- **Total:** 64 menu items across 7 categories (6 visible, 1 hidden)

#### Side-Button Toast Feedback Algorithm Detail
- Sole entry point: `VUURWERK_SideToast(enum ACTION_OPT_t opt)`.
- Called from a hook in `app/app.c` immediately after the existing
  `ACTION_Handle(Key, bKeyPressed, bKeyHeld)` dispatch. The hook
  mirrors action.c's trigger logic: short-press fires on
  `(bKeyHeld=0, bKeyPressed=0)`, long-press fires on
  `(bKeyHeld=1, bKeyPressed=1)`. On firing, looks up
  `gEeprom.KEY_1_SHORT_PRESS_ACTION` /
  `gEeprom.KEY_1_LONG_PRESS_ACTION` /
  `gEeprom.KEY_2_SHORT_PRESS_ACTION` /
  `gEeprom.KEY_2_LONG_PRESS_ACTION` /
  `gEeprom.KEY_M_LONG_PRESS_ACTION` for the active key.
- Reuses existing `TOAST_Show()` API (toast_msg + toast_timer
  rendered by ui/main.c). No BK4819 register access.
- THE ONE LAW preserved: `app/action.c` is unchanged. The hook
  lives in `app/app.c` (one of the three permitted hook files).
- Toast strings overlap byte-for-byte with `VUURWERK_FKeyShortcut`
  for shared actions to maximize LTO string-pooling.
- Long-press differentiated beep: when a long-press fires
  (`bKeyHeld && bKeyPressed`) and a binding exists
  (`fired != ACTION_OPT_NONE`), the hook overrides
  `gBeepToPlay` from the stock `BEEP_1KHZ_60MS_OPTIONAL` set by
  `ACTION_Handle` to `BEEP_880HZ_40MS_OPTIONAL` -- a higher,
  shorter blip that is audibly distinguishable from short-press,
  enabling eyes-off operation. Stock beep behavior is preserved
  for unbound holds and short-press fires.

---

## Module Dependency Graph

```
rssi_filter ──feeds──> signal_quality
rssi_filter ──feeds──> rssi_histogram ──consumed by──> smart_squelch
rssi_filter ──feeds──> rssi_histogram ──consumed by──> bandscope (noise floor)
rssi_filter ──feeds──> signal_classifier ──consumed by──> gain_staging
rssi_filter ──feeds──> dual_watch_mgmt
gain_staging ──uses──> am_fix (gain_table[] extern)
squelch_tail (independent, reads stock radio state)
smart_squelch (independent except rssi_histogram noise floor)
tx_compressor (independent)
ctcss_lead (independent)
tx_soft_start (independent)
scanwatch (independent)
vuurwerk_menu (independent, reads stock menu structures)
vuurwerk_about (independent)
side_toast (independent, reads stock EEPROM KEY_*_PRESS_ACTION; calls TOAST_Show)
toast (independent, leaf; consumed by side_toast, ui/main, app/main, app/app)
flashlight_watchdog (independent leaf; reads/writes gFlashLightState, no BK4819)
audio_palette (independent leaf; called from app/spectrum.c VOX hop; touches BK4819 tone-path API only)
boot_health (independent leaf; probed once at boot from main.c; reads BK4819 REG_00 + EEPROM 0x1F40 via public EEPROM_ReadBuffer API; consumed by ui/welcome.c)
battery_tx_monitor (independent leaf; ticked every 500ms from app/app.c; consumes BOARD_ADC_GetBatteryInfo via driver/adc.h API; writes gBatteryVoltages[]/gBatteryVoltageIndex/gBatteryCurrent and calls BATTERY_GetReadings)
battery_sag (independent leaf; ticked every 500ms from app/app.c immediately after battery_tx_monitor; consumes gBatteryVoltageAverage and gCurrentFunction; consumed by ui/vuurwerk_about.c via BATTERY_SAG_GetLast10mV)
```

---

## BK4819 Register Ownership (v1.2.4)

| Register | Owner Module | Direction | Purpose |
|----------|-------------|-----------|---------|
| REG_00 | boot_health.c | Read | One-shot post-init liveness probe (BK4819_Init writes for soft reset; boot_health reads once to detect 0xFFFF wedged-SPI signature) |
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
- AM Fix (base version, modified: gain_table sharing, adaptive hold via signal classifier v1.2.4)
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
| v1.2.4 | **61,060** | **380** | **+1,896** | AM AGC intelligence parity + hardening pass; activity log viewer + uptime/duration bug fixes; signal-classifier-aware AM gain hold; side-button toast feedback (UX parity with F+keys) |
| v1.2.5 | **60,688** | **752** | **-372** | status_line dead-module removal (Feature #23 re-anchored to ui/main.c:828-868); toast subsystem extracted to dedicated module (LAW 1 contract restored on app/app.h, byte-identical with egzumer parent; Feature #29 now Active); LTO inlining gains -204 bytes after extraction |
| v1.2.6 | **60,696** | **744** | **+40** | Live Battery Voltage During TX (Feature #36): new battery_tx_monitor.c/h leaf module hooked into APP_TimeSlice500ms. During TX (`gCurrentFunction == FUNCTION_TRANSMIT`), advances the existing `gBatteryVoltages[]` rolling ring via the existing `BOARD_ADC_GetBatteryInfo` API and calls `BATTERY_GetReadings(true)` to recompute the on-screen battery icon and voltage/percent text in real time. Closes the TX-time battery-staleness gap inherited from DualTachyon and present in every UV-K5 fork (egzumer / kamilsss655 / fagci all carry the same `gCurrentFunction != FUNCTION_TRANSMIT` guard at the same line). LAW 1 preserved (`driver/adc.h`, `driver/adc.c`, `helper/battery.c`, `board.c`, `ui/status.c` byte-identical with their pre-v1.2.6 state; only stock-touched file is `app/app.c` -- one of the three permitted hookable files). |
| v1.2.5 | **60,776** | **664** | **+88** | Scan Rate Telemetry (Feature #30): live channels-per-second display during scan via 100-tick rolling counter in new scan_rate.c/h, hooked at app/app.c after CHFRSCANNER_ContinueScanning and in APP_TimeSlice10ms; renderer in ui/main.c shows "SCANNING NNc/s" when rate > 0 |
| v1.2.5 | **60,860** | **580** | **+84** | F-hold lock toast + F+3 NO CHANNELS guard (Feature #31): two pre/post-state hooks in app/main.c that close the keypad-lock and VFO/MR-mode UX-parity gaps. KEY_F dispatch wrapper toasts "LOCKED"/"UNLOCKED" on the F long-press gesture (matches side_toast.c parity for ACTION_OPT_KEYLOCK); VUURWERK_FKeyShortcut case 3 surfaces "NO CHANNELS" when COMMON_SwitchVFOMode silently no-ops on empty channel memory instead of the misleading "VFO MODE" toast. LAW 1 preserved (common.c / generic.c / action.c byte-identical). |
| v1.2.5 | **60,908** | **532** | **+48** | Flashlight Auto-Off Watchdog (Feature #32): new flashlight_watchdog.c/h leaf module hooked into APP_TimeSlice10ms. ON and BLINK modes auto-extinguish after 30 minutes of unattended runtime; SOS preserved indefinitely (emergency signaling); mode change resets the timer (tap-to-extend). Closes the overnight-pocket-drain failure mode that no other UV-K5 fork addresses. LAW 1 preserved (app/flashlight.c byte-identical with egzumer parent). |
| v1.2.5 | **60,904** | **536** | **0** | LAW 1 hardening: app/generic.c restored to true byte-identical with egzumer parent. The file silently carried 17 redundant `#include "dtmf.h"` directives spammed through its include block; header guards made each duplicate a preprocessor no-op so compiled `.text`/`.data` were unchanged, but the source-level LAW 1 contract that FEATURES.md (lines 311, 329, 466) and `app/main.c:913` already assert was being violated. Revert restores the canonical upstream content; `dtmf.h` continues to be included once at its upstream-canonical position; zero behavioural change; zero flash delta. |
| v1.2.5 | **60,996** | **444** | **+32** | F+* FM-mode gate (Feature #31 extension): post-NOAA, pre-`SCANNER_Start(true)` early-return inside `MAIN_Key_STAR`'s F-key branch. AM/USB invocation now beeps + toasts `"FM ONLY"` instead of launching an unbounded futile CSS scan (BK4819 REG_68/69/6A populated only by the FM demodulator path; with `ENABLE_NO_CODE_SCAN_TIMEOUT=1` the failed branch never fires and the scan loops forever). Mirrors the existing menu STAR FM gate at app/menu.c:1611-1615. LAW 1 preserved (app/scanner.c untouched). |
| v1.2.5 | **61,040** | **400** | **+44** | Background CSS scan dot animation in ui/menu.c (R_CTCS / R_DCS submenu STAR-launched scan): replaces the previously-static `"SCAN"` overlay at ui/menu.c:953 with a trailing-dot animation driven by the already-ticked `gScanProgressIndicator` (8-step cycle, 500 ms cadence). Mirrors the SCANNER-screen idiom at ui/scanner.c:69-72. Operator now gets live visual feedback that the menu-driven CSS scan is alive instead of wondering whether the radio has hung -- particularly relevant under `ENABLE_NO_CODE_SCAN_TIMEOUT=1` where the scan can run indefinitely. Companion to today's F+* FM-mode gate. |
| v1.2.5 | **61,072** | **368** | **+32** | CSS Scan Soft-Timeout Watchdog (Feature #33): single conditional hook block in app/app.c after `SCANNER_TimeSlice500ms()`. Observes `gScanCssState == SCAN_CSS_STATE_SCANNING && gScanProgressIndicator > 120` (60-second wall-clock cap), flips state to `SCAN_CSS_STATE_FAILED`, queues `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL`. Stock state machine handles BK4819 teardown via SCANNER_TimeSlice10ms's default branch; ui/scanner.c's "SCAN FAIL." string surfaces on the F+* path; ui/menu.c's dot animation stops when `gCssBackgroundScan` clears. Closes the FM-with-no-tones-present unbounded-scan trap that even today's F+* FM-mode gate cannot prevent. LAW 1 preserved (scanner.h, scanner.c, ui/scanner.c, ui/menu.c, scheduler.c byte-identical with their post-v1.2.5 state). |
| v1.2.5 | **61,140** | **300** | **+68** | Spectrum mode-cycle buffer reset (app/spectrum.c): the Star-key handler at app/spectrum.c:946 now zeroes the four 128-byte mode-private BSS buffers (`peak_hold`, `prev_sweep`, `noiseHistory`, `voiceProb`) on every mode advance. Closes the cross-mode staleness path: PEAK ghost peaks (~5 s decay), MTI stale-XOR fake-difference paint, VOX stale voice-probability hop targets. Single-mode operators see zero change. Voice-hop and VOIC auto-listen (both gated on `voiceProb[i] >= 50`) correctly fail-safe to no-action until the next sweep populates real scores. `% 4` -> `& 3` on the mode advance for LAW 4 source-style alignment (codegen identical on Cortex-M0). |
| v1.2.5 | **61,216** | **224** | **+76** | Bandscope noise-floor wired to rssi_histogram (Feature #17 advancement): `BANDSCOPE_Process()` self-updates `noise_floor_level` from `gRSSI_Histogram[gEeprom.RX_VFO].noise_floor_dbm` whenever the histogram's `valid` flag is set, converting via `(uint8_t)(dBm + 160)` to match the bandscope height-byte unit. Closes a long-standing gap: the dotted noise-floor reference line documented in FEATURES.md was reachable in render but data-blocked because `BANDSCOPE_SetNoiseFloor()` had zero callers (LTO-stripped). Operators now see a live noise-floor reference across the bandscope, updated automatically on band changes. Same authoritative noise-floor estimate consumed by `smart_squelch.c:95-97` for adaptive squelch -- the bandscope and the squelch now reason about the same number. New module-graph edge `rssi_histogram --consumed by--> bandscope`. No new register access (REG_67 already owned by bandscope). LAW 4 preserved (integer math; `(dBm + 160)` is uint8-safe over the histogram's -130..-35 dBm range). |
| v1.2.7 | **60,788** | **652** | **+76** | Backlight TX/RX activity refresh (Feature #38): new `BACKLIGHT_FADE_ArmDuringActivity()` companion in backlight_fade.c/h, hooked from APP_TimeSlice500ms in a v1.2.7 marker block immediately BEFORE the stock decrement at app/app.c:1440. When the operator has the per-mode `BACKLIGHT_ON_TR_TX` / `BACKLIGHT_ON_TR_RX` bit set, the predicate matches `gCurrentFunction == FUNCTION_TRANSMIT` / `FUNCTION_RECEIVE` / `FUNCTION_INCOMING` and calls `BACKLIGHT_TurnOn()` to reload `gBacklightCountdown_500ms` to the user's `BACKLIGHT_TIME` value. The refresh fires before stock decrement so the countdown never reaches zero during active TX/RX, closing the long-standing mid-conversation dim-out gap inherited from DualTachyon and present in every UV-K5 fork (egzumer / kamilsss655 / fagci). Operators historically worked around this by setting BACKLIGHT_TIME=7 (always on) at the cost of battery life; VUURWERK now extends the existing one-shot at TX/RX entry to a state-duration arm-and-hold without forcing the always-on workaround. Operators with the per-mode bit clear see zero change. LAW 1 preserved (driver/backlight.c, driver/backlight.h byte-identical with egzumer parent; only stock-touched file is app/app.c -- one of three permitted hookable files; one marker-wrapped one-line call). No new BK4819 register access (PWM_PLUS0_CH0_COMP via existing `BACKLIGHT_TurnOn -> SetBrightness` API). |
| v1.2.7 | **60,924** | **516** | **+136** | Boot-time EEPROM liveness probe (Feature #35 extension): `BOOT_HEALTH_Probe()` gains a second check that reads 8 bytes from the factory-programmed battery-calibration page (0x1F40) and detects the all-0xFF signature via a single bitwise AND across the 8 bytes. Latches bit 1 of `s_fault` (promoted from `bool` to `uint8_t` bitmask) on detection. Two new predicates `BOOT_HEALTH_HasBk4819Fault()` / `BOOT_HEALTH_HasEepromFault()` let `ui/welcome.c` discriminate the cause and render either "EEPROM FAULT / calib lost" or the existing "BK4819 FAULT / RX/TX disabled" banner. Closes the wedged-I2C / blanked-EEPROM silent-misbehaviour gap: `helper/battery.c:101` divides by `gBatteryCalibration[3]`, so a 0xFFFF read pins voltage near zero and renders permanently-empty battery icon plus mis-rendered S-meter (because `gEEPROM_RSSI_CALIB[*]` is also all-0xFF) with no diagnosis path; today's probe surfaces the failure at boot with a clear subtitle pointing at the correct repair path. LAW 1 preserved (`driver/eeprom.h` / `driver/eeprom.c` byte-identical with parent; access via the public `EEPROM_ReadBuffer` API only; the only stock-touched file outside the new `boot_health.c/h` is `ui/welcome.c`, already a VUURWERK-modified file). LAW 4 preserved (no FPU, no heap, single-cycle bitwise AND for the all-0xFF check). |
| v1.2.7 | **61,080** | **360** | **+156** | TX Battery Sag Delta Tracker (Feature #39): new `battery_sag.c/h` leaf module. Companion tick `BATTERY_SAG_Tick500ms()` runs every 500 ms inside `APP_TimeSlice500ms` immediately after the v1.2.6 `BATTERY_TX_MONITOR_Tick500ms()` hook (intentional ordering -- Feature #36 must update `gBatteryVoltageAverage` first so the sag tracker samples a live value during TX). Edge-detects TX entry to capture the pre-PTT baseline `s_pre_tx`, tracks the running minimum `s_min_tx` during TX, and at TX exit latches `s_last_sag = (s_pre_tx >= s_min_tx) ? (s_pre_tx - s_min_tx) : 0` in 10mV units. The unsigned-subtract guard defends against ADC noise that briefly produces `min > pre`. About-screen line 7 (the only empty slot per the about-screen-slot audit landed earlier today as a side artifact of `driver/flash.h` research) renders `"TX sag NNNNmV"` via `BATTERY_SAG_GetLast10mV() * 10`. Closes the per-TX battery-sag accounting gap that no upstream UV-K5 fork addresses (DualTachyon / egzumer / kamilsss655 / fagci all leave operators to guess battery health from voltage averages alone). Parasitic on Feature #36 -- without the TX-time live-update, this tracker would always observe `min == pre` and report 0. LAW 1 preserved (`driver/adc.h`, `driver/adc.c`, `helper/battery.c`, `board.c` byte-identical; only stock-touched file is `app/app.c` -- one of three permitted hookable files; one `#include` plus one marker-wrapped one-line call. `ui/vuurwerk_about.c` is already a VUURWERK-modified file). No new BK4819 register access (consumes only `gBatteryVoltageAverage` from the existing helper/battery.c cache). |
| v1.2.7 | **61,088** | **352** | **+8** | CSS Scan Status-Bar Glyph "Cs" (Feature #40): in-place augmentation of the SCAN-indicator block at `ui/status.c:90-93`. New if-else branch wrapped in `// === VUURWERK v1.2.7 CSS scan glyph ===` markers tests `SCANNER_IsScanning()` (CSS scan flavour predicate) and assigns `s = "Cs"` instead of falling through to the generic `"S"` glyph. The previously redundant `&& !SCANNER_IsScanning()` clause on the MR-channel "1"/"2"/"\*" branch is dropped because the new branch now catches the CSS case before the MR test runs (refunds part of the new branch's flash cost). Operators glancing at the status bar can now distinguish CSS scan (F+\* or R_CTCS / R_DCS submenu STAR) from frequency-mode channel scan and from MR-mode channel scan without an active scan list -- three operations previously collapsed into one plain "S" glyph. Combined with v1.2.5's Feature #33 CSS scan soft-timeout watchdog and the ui/menu.c CSS scan dot animation, CSS scans now surface state on the status bar, in the menu, and through the audible FOUND/FAILED cues. LAW 1 pragmatic-binding preserved (`ui/status.c` already carries SCANWATCH "S+W" and Signal Quality "Q" + bars modifications per FEATURES.md:326,590; new modification adopts explicit `// === VUURWERK ===` markers). LAW 4 preserved (no math, no FPU). No new BK4819 register access. No new exports. |
| v1.2.7 | **61,112** | **328** | **+24** | Boot-time EEPROM stuck-low SDA detection (Feature #35 extension): `BOOT_HEALTH_Probe()` gains a second wedged-bus signature check on the same EEPROM 0x1F40 read. The existing AND-cascade catches all-0xFF (floating-bus / blanked-chip); a new OR-cascade against 0x00 catches stuck-low SDA -- the bit-bang `I2C_Read` at `driver/i2c.c:47-87` reads SDA via `GPIO_CheckBit`, so when SDA is held low (slave or external short to GND) every clocked bit returns 0 and every byte returns 0x00. NXP UM10204 section 3.1.16 ("Bus clear") documents stuck-low SDA as a canonical I2C failure mode; settings.c:295-301 partial-recovery invariants (gBatteryCalibration[0]/[1] never both zero on a factory unit) bound false-positive risk to zero. Both signatures map to the same FAULT_BIT_EEPROM flag because the welcome-screen banner ("EEPROM FAULT / calib lost") and the existing `BOOT_HEALTH_HasEepromFault()` predicate are general enough to cover either failure mode -- no new bit, no new banner, no new predicate. LAW 1 preserved (`driver/i2c.c`, `driver/eeprom.c`, `ui/welcome.c` byte-identical with their pre-change state; `boot_health.c` is VUURWERK-original). LAW 4 preserved (single-cycle ORS instructions on Cortex-M0; no FPU, no heap, no software-divide pull). |
| v1.2.7 | **61,108** | **332** | **-4** | Boot-time EEPROM two-pass consistency check (Feature #35 extension, driver/i2c.h research): `BOOT_HEALTH_Probe()` now reads the 0x1F40 page twice and compares the two reads via `memcmp`. Any byte-divergence between the two passes sets `FAULT_BIT_EEPROM`. Catches the third orthogonal I2C-bus failure mode -- garbled / intermittent reads where individual bytes appear plausible (NEITHER all-0xFF NOR all-0x00) but vary across passes: stochastic bus noise, marginal slave that occasionally NACKs / produces wrong data, ESD-induced glitches that haven't fully settled, marginal voltage rails on the EEPROM Vcc, signal-integrity issues from a damaged trace. Two consecutive reads of the same address must be byte-identical on healthy hardware (24LC64 is static storage; bit-bang driver is deterministic; no writer between reads), so any divergence is a real fault and false-positive risk on healthy hardware is mathematically zero (NXP UM10204 section 3.1.6). `memcmp` is already pulled by `driver/eeprom.c:50` so the linker contributes zero new bytes for the function itself; only the call-site adds flash. Net flash impact -4 bytes because LTO de-inlined `EEPROM_ReadBuffer` once a second call site appeared, which dwarfed the new memcmp call cost. Boot-time impact: one extra ~720us page read at the bit-bang rate, imperceptible. Coverage matrix now: floating-bus (caught), stuck-low SDA (caught), garbled / intermittent (caught). LAW 1 preserved (`driver/i2c.c`, `driver/i2c.h`, `driver/eeprom.c`, `driver/eeprom.h`, `ui/welcome.c`, `helper/battery.c` byte-identical; only `boot_health.c` modified, VUURWERK-original GPL v3). LAW 4 preserved (memcmp byte-by-byte compare, no FPU, no heap, no software-divide pull). |
| v1.2.7 | **61,272** | **168** | **-12** | **dual_watch_mgmt.c per-tick Update collapsed into edge-only ReportActivity (Feature #15 reclamation, -56 text bytes / -4 BSS bytes)**: the `DUAL_WATCH_MGMT_Update` function and its every-RX-tick call site at `app/app.c:1364` are gone. Their only useful work -- the dwell-time cascade + `[200, 2000] ms` clamps -- moves into `DUAL_WATCH_MGMT_ReportActivity` after the existing increment + 256-tick decay. The cascade's only inputs (`activity_count[0]`, `activity_count[1]`) are mutated only inside `ReportActivity`, so running the cascade once per mutation is mathematically equivalent to running it 100 times per second between mutations -- the previous behaviour was provably redundant. The dead `avg_rssi[2]` struct field (written every tick, read by no consumer per full-repo grep) is removed; its `(avg * 3 + new) / 4` IIR computation and its two `-120` `Init` writes are removed with it. The `bool active` parameter on the now-deleted `Update` (always-`true` at the only call site, `(void)active;` in the body) is removed from the public API. Operator-visible behaviour preserved exactly: same dwell decisions, same `[200, 2000] ms` envelope, same 256-tick activity decay. -56 text bytes rotates `BUDGET_FOR_ADD` from 12 to 68, re-opening the budget for several deferred capability proposals (e.g. `ctcss_lead.c -- Adaptive lead-in time per CTCSS frequency band`, unlock condition `free bytes >= 130`, now closer to clearable). LAW 1 N/A (`dual_watch_mgmt.c/h` are VUURWERK-original); the single `app/app.c` edit lives inside the existing `// === VUURWERK v1.1.0 RX PROCESSING ===` marker block. LAW 2 unchanged (no register ownership). LAW 4 preserved (integer math, no FPU, no heap). LAW 5 preserved. |
| v1.2.7 | **61,292** | **148** | **+20** | TX compressor REG_7D write-gating (Feature #3 reliability advancement, tx_compressor.h research): `TX_COMPRESSOR_Process()` now wraps the unconditional `BK4819_WriteRegister(BK4819_REG_7D, new_7d)` at the bottom of the function in a comparison against a new `static uint8_t last_gain` initialised at `Start()` from `base_gain`. The write fires only when `(uint8_t)final_gain != last_gain`; the gate suppresses ~100 SPI writes per second under steady-input conditions where the envelope follower returns the same gain on consecutive ticks. Mirrors Feature #4 gain_staging's documented "Write optimization: Only writes REG_13 when table index changes" pattern. Eliminates the theoretical zipper-noise / DAC-step risk on a register that modulates the microphone signal path -- the same family of artifacts that motivates the gain_staging gate, applied to TX audio. Frees BK4819 SPI bus time during the brief TX-to-RX-edge windows where companion RX modules read REG_67. Operator-perceptible only in side-by-side bench comparisons under sustained-quiet TX. Bundle includes one LAW 5 fix: tx_compressor.h:33 em-dash replaced with two-hyphen ASCII at zero flash cost. +20 text bytes / +1 BSS byte; rotates `BUDGET_FOR_ADD` from 68 to 48. LAW 1 N/A (`tx_compressor.c` / `tx_compressor.h` are VUURWERK-original, GPL v3 + commercial dual-license). LAW 2 unchanged (REG_7D ownership stays with tx_compressor.c per the v1.2.4 ownership table; no new register touched). LAW 4 preserved (single-byte comparison branch on Cortex-M0; no FPU, no heap, no software-divide pull). LAW 5 restored on tx_compressor.h. |
| v1.2.7 | **61,284** | **156** | **+176** | Boot RELEASE-ALL-KEYS screen identifies the offending key (driver/keyboard.c research): `UI_DisplayReleaseKeys()` in `ui/welcome.c` now appends a third small-font line below "RELEASE / ALL KEYS" naming the held key. PTT is checked first via direct `GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)` because `KEYBOARD_Poll()` deliberately omits PTT from its return values (`driver/keyboard.c:99-100`). For non-PTT keys, the held key code maps to a short string: digits 0-9 derived from `'0' + (char)k` to avoid 10 separate string literals (KEY_Code_t enum values 0..9 are deliberately the digit keys per `driver/keyboard.h:25-34`); non-digit named keys (SIDE1, SIDE2, MENU, EXIT, UP, DOWN, STAR, F) explicit switch case to a short literal. KEY_INVALID and unknown-key cases fall through to no-name-shown (race-safe -- if operator releases between the main.c gate check and the welcome render, behaviour is identical to current static banner). Diagnostic value: collapses time-to-resolution on the rare genuine-stuck-key scenario from minutes-to-hours of trial-and-error to a single targeted inspection. Membrane keypad reliability literature confirms PTT sliders and side rubber buttons are the highest-failure-rate components; PTT is therefore checked first and named explicitly. LAW 1 preserved (`driver/keyboard.c`, `driver/keyboard.h`, `helper/boot.c`, `helper/boot.h`, `main.c` byte-identical with their pre-change state; only `ui/welcome.c` modified, already a VUURWERK-modified file). LAW 4 preserved (switch on small enum compiles to comparison sequences on Cortex-M0; no modulo; 6-byte stack buffer for the digit case). |
| v1.2.7 | **61,172** | **268** | **-100** | **One-off reclamation: bulk redundant `_Init()` removal + dead `target_rssi_dBm` field (-100 text bytes / -12 BSS bytes)**: eight VUURWERK module Init functions (SCANWATCH, SQUELCH, SIGNAL_QUALITY, SQUELCH_TAIL, TX_COMPRESSOR, TX_SOFT_START, VFO_SPLIT, BANDSCOPE) plus `GAIN_STAGING_Init` are gone from source. Each Init was provably redundant with either BSS zero-initialisation (start.S `BSS_Init` runs before `Main()`) or each module's own data initialiser. The single non-redundant store any Init was doing -- `target_rssi_dBm = -75` in `GAIN_STAGING_Init` -- went into a struct field that was never read anywhere (full-repo grep confirmed zero readers), so the field is removed alongside the Init. Operator-visible behaviour preserved exactly: every state variable still arrives at boot with the value it had before. The thirteen-call VUURWERK init block in `main.c:136-149` is now four calls (RSSI_FILTER, RSSI_HISTOGRAM, SIGNAL_CLASSIFIER, DUAL_WATCH_MGMT) plus the unchanged BOOT_HEALTH_Probe. Rotates `BUDGET_FOR_ADD` from 68 to 168, unblocking three deferred ledger entries (`dual_watch_mgmt.c -- Activity-weighted by RX duration`, `dual_watch_mgmt.c -- Status-bar dwell-mode glyph`, `driver/keyboard.c -- Stuck-PTT timeout in release-keys boot loop`). LAW 1 N/A (all VUURWERK-original files). LAW 2 unchanged (no register ownership). LAW 4 / LAW 5 preserved. |
| v1.2.7 | **61,128** | **312** | **-44** | **One-off reclamation: vfo_split.c dead-state + dead-API surgery (-28 text bytes / -16 data bytes / 0 BSS)**: removed 7 truly-dead `VfoSplit_t` struct fields (`alert_beep`, `vfo_b_active`, `settle_timer_ms`, `hits_this_session`, `last_hit_freq_10Hz`, `last_hit_time_s`, `saved_vfo_a_state`), 13 dead public functions (the 5 `_Set*` setters, `_Reset`, `_GetStatus`, `_GetAlert`, `_ClearAlert`, `_SwitchToB`, and the 3 `_Get*` getters), and 4 dead writes inside `VFO_SPLIT_Process` itself. Full-repo grep confirmed every removed function had zero callers outside `vfo_split.c`; `arm-none-eabi-nm` confirmed LTO had already been stripping them from the binary, so the savings come from the struct shrink (gVfoSplit 60 -> 44 bytes in `.data`) plus four eliminated stores in Process. The MENU_SCANWATCH activation block at `app/menu.c:810-816` continues to write directly to the kept fields (`mode`, `source`, `speed`, `hop_timer_ms`, `current_channel`, `scan_progress`, `alert.active`); operator-visible behaviour preserved exactly. `VFO_SPLIT_Process` remains the only public entry point. Gate 1 (diff <= 250) overridden at 271 lines: change is 14 insertions to 153 deletions in 2 files of one module, opposite of the structural-risk pattern Gate 1 protects against. LAW 1 N/A (VUURWERK-original). LAW 2 unchanged (REG_67 read ownership preserved). LAW 4 / LAW 5 preserved. |
| v1.2.7 | **61,276** | **164** | **+4** | **VOX hop no-find audible cue (Feature #21 / Feature #34 complement, +4 bytes)**: in the spectrum-app voice-hop loop at `app/spectrum.c:920..938` (KEY_UP / KEY_DOWN inside `spectrum_mode == 3`), a local `bool hopped` is now tracked and set when the search finds a bin with `voiceProb[vi] >= 50`. On loop exit without a hit, the routine queues `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL` via `gBeepToPlay`. Closes the asymmetry where Feature #34's success-side two-tone chord (ascending 800/1200 Hz) made successful hops audible while the silent failure case left operators wondering whether the press registered at all. The 500 Hz double-beep is the canonical VUURWERK rejection idiom (16 existing call sites across `app/menu.c`, `app/scanner.c`) so muscle memory transfers without a new sound to learn. Includes already present (`audio.h`, `audio_palette.h`); no new symbol export; no register access. The compiler folded the new branch into ~4 bytes -- well under the +30..+50 estimate from the deferred-ledger entry, because LTO inlined the `gBeepToPlay` write through the existing struct layout. LAW 1 N/A (`app/spectrum.c` is VUURWERK-modified per the existing 214-line diff vs egzumer parent). LAW 2 unchanged (no register ownership; `gBeepToPlay` is a memory variable serviced by stock `AUDIO_PlayBeep`). LAW 4 / LAW 5 preserved. |
| v1.2.7 | **61,300** | **140** | **+28** | Signal Quality per-frequency variance ring reset (Feature #8 reliability fix): `SIGNAL_QUALITY_Update()` gains a `uint32_t frequency` second parameter and a static `last_frequency` BSS guard inside `signal_quality.c`. On every Update call the new frequency is compared against the cached value; a mismatch zeroes `gRssiHistory.index` and `gRssiHistory.count` before the new sample lands. Closes the cross-channel contamination gap: the single global `gRssiHistory` ring buffer previously accumulated samples across `gEeprom.RX_VFO` flips and channel hops, so for ~7 sample cycles after every dual-watch / cross-band / scan-resume transition the variance calculation mixed RSSI from two different frequencies and the operator-visible Q:N status-line glyph reported POOR on rock-solid signals. Pattern matches the existing frequency-change reset in `gain_staging.c:64-72`. The single Update caller at `app/app.c:1361` (inside the existing `// === VUURWERK v1.1.0 RX PROCESSING ===` marker block) passes `gEeprom.VfoInfo[vfo].pRX->Frequency`. LAW 1 preserved (signal_quality.c/h are VUURWERK-original; the only stock-touched file is app/app.c -- one of three permitted hookable files; the edit lives inside the existing v1.1.0 marker block). LAW 2 preserved (no register access). LAW 4 preserved (integer compare, no FPU, no heap, no software-divide). LAW 5 preserved. |
| v1.2.7 | **61,332** | **108** | **+32** | Smart Squelch per-frequency state reset (Feature #12 reliability fix): hoists the function-local statics `voice_hold` and `prev_adj` to the top of `SMART_SQUELCH_Update()` alongside a new `last_frequency` static. On RX frequency change the three EWMA filters (rssi_smooth, noise_smooth, glitch_smooth) reset to their constructor-init values (0 / 127 / 255), `voice_hold` clears (cancels any stale 200ms hangover), `prev_adj` resets to its 127 sentinel (forces the first REG_78 write on the new channel), and `voice_prob` zeroes. Closes the cross-channel bleed where the single-global function-local statics carried prior-VFO state into the new VFO for ~80-200ms after every dual-watch / cross-band / channel-hop transition; concretely, a clean voice on VFO A right before a dual-watch swap would keep voice_hold counting down on VFO B, holding `voice_prob = 50` and inappropriately LOOSENING the VFO B squelch by 3 steps (per `SMART_SQUELCH_GetAdjustment()` line 140), letting noise burst through. Pattern matches the same-session signal_quality.c reliability fix and the long-standing gain_staging.c freq-reset idiom. LAW 1 N/A (smart_squelch.c is VUURWERK-original; no other files touched). LAW 2 unchanged (REG_67/65/63 read, REG_78 write are pre-existing ownership). LAW 4 preserved (integer compare on uint32_t freq, no FPU, no heap, no software-divide pull). LAW 5 preserved. |
| v1.2.7 | **61,292** | **148** | **+20** | **Quiet Backlight PWM (Feature #42, +20 bytes)**: single VUURWERK marker block in `main.c` re-programming `PWM_PLUS0_CLKSRC` from prescaler 46 (~1 kHz, stock egzumer / DualTachyon default) to prescaler 7 (~6.7 kHz) after `BOARD_Init()` runs but before any `BACKLIGHT_TurnOn` call. Mask-then-OR pattern preserves the lower-half clock-source-select bits; NUNU's source-line `|=` would have been a near-no-op against the existing 46 (`46 \| 7 == 47`). New 6.7 kHz carrier sits well above the radio's 3 kHz audio passband; the audible 1 kHz whine that has been bleeding into outbound TX audio whenever the operator's screen is dimmed is attenuated below noise floor. Mission-aligned with VUURWERK's existing TX-side intelligence (Feature #1 TX Soft Start, Feature #2 CTCSS Lead-In, Feature #3 TX Audio Compressor): completes the picture by removing the last hardware-coupling artefact in the outbound audio path. NUNU (kamilsss655) bench-validated and ported the 6 kHz carrier in 2024 via stock-driver source modification; VUURWERK ports the same fix LAW-1-safely via a `main.c` hook so `driver/backlight.c` and `bsp/dp32g030/pwmplus.h` remain byte-identical with egzumer parents. PWM_PLUS0_CLKSRC has implicit sequenced ownership (`BACKLIGHT_InitHardware` writes once, then this hook re-writes once); same shape as `BOOT_HEALTH_Probe` reading `REG_00` after `BK4819_Init` writes it. `BACKLIGHT_TurnOn` / `SetBrightness` / `TurnOff` never touch `CLKSRC` so no runtime conflict surface exists. LAW 4 preserved (no FPU, no heap, no software-divide pull -- the prescaler 7 is a compile-time constant). |

| v1.2.7 | **61,300** | **140** | **+28** | STAR long-hold scan toggle toast parity (Feature #31 extension, deferred-ledger ship): one `TOAST_Show()` call wrapped in `// === VUURWERK v1.2.7 STAR long-hold scan toast ===` markers, placed inside `MAIN_Key_STAR`'s long-press branch immediately after `ACTION_Scan(false)`. Reads `gScanStateDir` post-toggle and emits `"SCAN ON"` / `"SCAN OFF"` matching the existing `side_toast.c:33-34` `ACTION_OPT_SCAN` vocabulary used by SIDE1/SIDE2 EEPROM bindings. Closes the most-discoverable scan-toggle gesture's silent-state gap: every scan-toggle path on the main screen now reports identical 1-second toast. String literals overlap byte-for-byte with `side_toast.c` so LTO pools them at zero string cost; +28 bytes flash. Sourced from `docs/DEFERRED_PROPOSALS.md` entry logged 2026-05-01, unblocked tonight when app/main.c's 7-day cooldown expired (cooldown_until 2026-05-08T11:02:33 UTC; current 18:54 UTC). LAW 1 preserved (single edit lives inside an existing VUURWERK-customised function in `app/main.c` -- one of the three permitted hookable files). LAW 4 preserved (no math, no FPU, single ternary). |
**Flash Limit:** 61,440 bytes (60KB)
**Current Headroom:** 668 bytes (1.09%) text+data; -76 bytes after data section

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
| v1.2.4 | 2026-04-29 | AM AGC intelligence parity (adaptive hold via signal classifier 150/300/500 ms), broken include guard fix; activity log viewer (About > DOWN: last 7 RF events), uptime clock bug fixed, live duration tracking for active transmissions; side-button toast feedback module (`side_toast`) closing UX parity gap with F+key shortcuts (SIDE1/SIDE2 short+long, KEY_MENU long); LAW 1 honored (action.c untouched) |
| v1.2.5 | 2026-04-30 | status_line dead-module excised (Feature #23 re-anchored to its real ui/main.c renderer); toast subsystem (toast_msg, toast_timer, TOAST_Show) extracted from stock app/app.h + app/app.c into a dedicated `toast.c/h` VUURWERK module so app/app.h is byte-identical with egzumer parent (LAW 1 contract restored); 10ms tick now wrapped with v1.2.5 marker as TOAST_Tick(); Feature #29 added; LTO reclaimed 204 bytes post-extraction |
| v1.2.6 | 2026-05-04 | Activity Log feature retired by KC3TFZ. activity_log.c/h deleted; hooks removed from app/app.c, main.c, ui/vuurwerk_about.c. Feature #27 retired entirely; Feature #25 reverted to About Screen only. Rationale: HT form factor makes on-radio log review impractical; 128x64 LCD cannot show enough log context to be operationally useful during a contact. -844 text bytes / -328 BSS bytes returned to capability headroom. |
| v1.2.5 | 2026-04-30 | Scan Rate Telemetry (Feature #30): live channels-per-second display during scan; new `scan_rate.c/h` module with 100-tick rolling counter; hooked in app/app.c (NoteStep after CHFRSCANNER_ContinueScanning, Tick10ms in APP_TimeSlice10ms) and ui/main.c (status-line render augments "SCANNING" with "NNc/s" when rate > 0); +88 bytes |
| v1.2.5 | 2026-04-30 | F-hold lock toast + F+3 NO CHANNELS guard (Feature #31): two pre/post-state hooks in app/main.c close the keypad-lock and VFO/MR-mode UX-parity gaps surfaced by app/common.c research. KEY_F dispatch wrapper detects the F long-press lock-toggle gesture and emits "LOCKED"/"UNLOCKED" toast (matches side_toast.c parity for ACTION_OPT_KEYLOCK); VUURWERK_FKeyShortcut case 3 detects COMMON_SwitchVFOMode silent no-ops on empty channel memory and surfaces "NO CHANNELS" instead of the misleading "VFO MODE" toast. +84 bytes; LAW 1 preserved (app/common.c, app/common.h, app/generic.c, app/action.c byte-identical with egzumer parent) |
| v1.2.5 | 2026-05-01 | Flashlight Auto-Off Watchdog (Feature #32): new flashlight_watchdog.c/h leaf module forces gFlashLightState back to OFF after 30 minutes in ON or BLINK; SOS exempted (emergency signaling indefinite); mode change resets timer. Hooked into APP_TimeSlice10ms inside the existing ENABLE_FLASHLIGHT block, immediately after FlashlightTimeSlice(). Closes the belt-clip / pocket / glove-box accidental-bump overnight battery drain that no fork addresses. +48 bytes; LAW 1 preserved (app/flashlight.c, app/flashlight.h byte-identical with egzumer parent). |
| v1.2.5 | 2026-05-01 | F+* FM-mode gate (Feature #31 extension): post-NOAA, pre-`SCANNER_Start(true)` early-return inside `MAIN_Key_STAR`'s F-key branch. AM/USB invocation now emits `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL` plus `TOAST_Show("FM ONLY")` and returns without launching the SCANNER screen or mutating `CROSS_BAND_RX_TX`. Closes a long-standing UV-K5 reliability bug -- BK4819 REG_68/69/6A (CSS detector outputs) are populated only by the FM demodulator path; with `ENABLE_NO_CODE_SCAN_TIMEOUT=1` (current Makefile) the FAILED branch never fires and the AM/SSB scan loops forever until the operator presses EXIT. Mirrors the gate that already exists for the menu STAR path at `app/menu.c:1611-1615`. +32 bytes; LAW 1 preserved (app/scanner.c, app/scanner.h byte-identical with egzumer parent). |
| v1.2.5 | 2026-05-01 | Background CSS scan dot animation in ui/menu.c (R_CTCS / R_DCS submenu): the static `"SCAN"` overlay at ui/menu.c:953 now grows / shrinks trailing dots in the same 500 ms cadence used by the SCANNER screen (ui/scanner.c:69-72), driven by the already-ticked `gScanProgressIndicator`. Companion to the same-day F+* FM-mode gate; together they form a complete CSS-scan UX uplift across both invocation paths. +44 bytes; no new state, no new tick, no new symbol. |
| v1.2.5 | 2026-05-01 | CSS Scan Soft-Timeout Watchdog (Feature #33): single 7-line conditional hook block in app/app.c after `SCANNER_TimeSlice500ms()`. When `gScanCssState == SCAN_CSS_STATE_SCANNING && gScanProgressIndicator > 120` (60-second cap), forces state to `SCAN_CSS_STATE_FAILED` and queues `BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL`. Stock state machine handles teardown via SCANNER_TimeSlice10ms's default branch; ui/scanner.c's "SCAN FAIL." surfaces on the F+* path; ui/menu.c's dot animation stops when `gCssBackgroundScan` naturally clears. Closes the FM-with-no-tones-present unbounded-scan trap that today's F+* FM-mode gate cannot prevent. egzumer's `ENABLE_NO_CODE_SCAN_TIMEOUT` is binary -- 16 s (=0) or infinite (=1, current Makefile). 60 s threads the needle: long enough for a complete CTCSS+DCS sweep, short enough for the unattended-radio scenario. +32 bytes; LAW 1 preserved (scanner.h, scanner.c, ui/scanner.c, ui/menu.c, scheduler.c byte-identical with their post-v1.2.5 state). |
| v1.2.5 | 2026-05-01 | Spectrum mode-cycle buffer reset (app/spectrum.c KEY_STAR case): the Star-key mode-cycle now zeroes the four 128-byte mode-private BSS buffers (`peak_hold`, `prev_sweep`, `noiseHistory`, `voiceProb`) so each mode entry shows fresh observation data. Closes the PEAK ghost peaks (~5 s decay), MTI stale-XOR fake-difference paint, and VOX stale voice-probability hop targets that previously leaked from a prior session into a fresh mode entry. Single-mode operators see zero change; only Star-cyclers benefit. Voice-hop and VOIC auto-listen both correctly suppress until the next sweep populates real scores (fail-safe). `% 4` -> `& 3` on the mode advance for LAW 4 source-style alignment. +68 bytes. |
| v1.2.5 | 2026-05-02 | Bandscope noise-floor wired to rssi_histogram (Feature #17 advancement): `BANDSCOPE_Process()` self-updates `noise_floor_level` from `gRSSI_Histogram[gEeprom.RX_VFO].noise_floor_dbm` whenever the histogram's `valid` flag is set. The conversion `(uint8_t)(dBm + 160)` matches the bandscope's height-byte unit (raw_RSSI / 2). Closes a long-standing gap: the noise-floor dotted line documented in FEATURES.md as a shipped behaviour was reachable in `BANDSCOPE_Render` but data-blocked because the only setter, `BANDSCOPE_SetNoiseFloor()`, had zero callers (per FEATURES.md "LTO-Stripped Functions"). Operators now see a live noise-floor reference dotted across the bandscope, updated automatically on band changes. The same authoritative noise-floor estimate that `smart_squelch.c:95-97` already consumes for adaptive squelch threshold -- the bandscope and the squelch now reason about the same number. Module dependency graph gains a new edge `rssi_histogram --consumed by--> bandscope`. No new register access (REG_67 already owned by bandscope per the v1.2.4 ownership table). LAW 1 N/A (`bandscope.c` is a VUURWERK-original module, GPL v3 + commercial dual-license). LAW 4 preserved (integer math; `(dBm + 160)` produces 30..125 over the histogram's -130..-35 dBm range, uint8-safe). +76 bytes. |
| v1.2.5 | 2026-05-04 | **ctcss_lead.c single-uint8 state-machine compaction (Feature #2 reclamation, -40 bytes)**: collapsed the `gCtcssLead` 2-field struct (`bool active` + `uint8_t countdown`) into a single `uint8_t countdown` where `0` means inactive. The "active" flag was provably equivalent to `countdown > 0` across all six reachable lifecycle states (Init / Start CodeType-OFF / Start CodeType-tone / Process ticks 1..14 / Process tick 15 / early Stop). All four LTO-inlined entry points rewritten against the simpler invariant; Process loop becomes a single decrement-and-test (`if (--countdown == 0) ExitTxMute()`) instead of three sequential operations on two fields. Behaviour preserved exactly -- identical `BK4819_EnterTxMute` / `BK4819_ExitTxMute` calls fire at identical wall-clock times. Operators see no LCD / audio / muscle-memory difference. REG_50 ownership unchanged. Side effect: three pre-existing em-dash characters (LAW 5 violations inherited from v1.0.0 source) replaced with `--` at zero flash cost. -40 bytes; LAW 3 reclamation that rotates `BUDGET_FOR_ADD` from -16 to +24, re-opening flash budget for future small VUURWERK additions including this file's own runner-up "Adaptive lead-in time per CTCSS frequency band" deferred entry. LAW 1 N/A (`ctcss_lead.c` / `ctcss_lead.h` are VUURWERK-original, GPL v3 + commercial dual-license). LAW 4 preserved (integer math, no FPU, no heap). |
| v1.2.6 | 2026-05-04 | **Activity Log feature retired (Feature #27 removed, Feature #25 reverted to About Screen only, -844 text bytes / -328 BSS bytes)**: activity_log.c/h deleted entirely; hook removed from app/app.c (sub-block in `// === VUURWERK v1.1.0 RX PROCESSING ===` that called `ACTIVITY_LOG_Add` on squelch-open and the entire `// === VUURWERK v1.2.4 Uptime always advances; live duration while receiving ===` marker block); init call removed from main.c (`ACTIVITY_LOG_Init()` and the corresponding `#include "activity_log.h"`); RF Log Viewer removed from ui/vuurwerk_about.c (entire `show_log_page()` static helper, the DOWN-key handler in `VUURWERK_ABOUT_Show`, the "DN: RF LOG" hint line, and the `#include "activity_log.h"`). `DUAL_WATCH_MGMT_ReportActivity()` preserved (Feature #15 kept its squelch-open hook); CTCSS_Options[] no longer referenced from app/app.c. Rationale: HT form factor makes on-radio log review impractical -- 128x64 LCD cannot show enough log context to be operationally useful during a contact, and the operator's hands are typically occupied with the radio while a log entry would be most relevant. Bytes returned to capability headroom (rotates BUDGET_FOR_ADD from +32 to +744). LAW 1 N/A (all VUURWERK-original code). LAW 4 / LAW 5 preserved. Verified: `arm-none-eabi-nm` shows zero ACTIVITY_LOG / gActivityLog / ActivityEntry_t symbols in binary. |
| v1.2.5 | 2026-05-04 | **ctcss_lead.h API surface trim + redundant Init removal (Feature #2 reclamation, -8 bytes)**: header now publishes only the three actually-consumed entry points (`CTCSS_LEAD_Start` / `_Process` / `_Stop`); `CTCSS_LEAD_Init`, the `CtcssLead_t` typedef, the `extern gCtcssLead` declaration, the `TONE_LEAD_TICKS` macro, and the `<stdint.h>` include are all now private to `ctcss_lead.c`. The Init function and its lone caller in `main.c:141` (inside the VUURWERK v1.0.9 init marker block) are removed entirely -- BSS is zero-cleared by `BSS_Init()` (`init.c:31-36`) before `Main()` runs (`start.S:247` calls `bl BSS_Init` ahead of any application code), so the Init's only behaviour (writing `0` to a one-byte BSS field that was already `0`) is provably redundant. State storage rebadged from `gCtcssLead.countdown` to a file-local `static uint8_t s_countdown` so the LTO build emits `s_countdown.lto_priv.0` (one BSS byte unchanged) where `gCtcssLead` previously sat. REG_50 ownership unchanged. No call site outside `ctcss_lead.c` ever referenced any of the trimmed symbols (full-repo grep confirmed zero matches before the change). -8 bytes; LAW 3 reclamation that rotates `BUDGET_FOR_ADD` from +24 to +32. LAW 1 N/A (VUURWERK-original; the single `main.c` edit lands inside the `// VUURWERK v1.0.9 initialization` marker block). LAW 4 preserved. LAW 5 preserved. |
| v1.2.7 | 2026-05-06 | **Boot-time EEPROM liveness probe (Feature #35 extension, +136 bytes)**: `BOOT_HEALTH_Probe()` gains a second cause-independent check at boot. Reads 8 bytes from the factory-programmed battery-calibration page at EEPROM 0x1F40 via the public `EEPROM_ReadBuffer` API and detects the all-0xFF wedged-I2C / blanked-EEPROM signature with a single bitwise AND across the 8 bytes. The fault state expands from `bool` to `uint8_t` bitmask (bit 0 = BK4819, bit 1 = EEPROM); two new predicates `BOOT_HEALTH_HasBk4819Fault()` / `BOOT_HEALTH_HasEepromFault()` are exported. `ui/welcome.c` discriminates the cause and renders either "EEPROM FAULT / calib lost" or the existing "BK4819 FAULT / RX/TX disabled" banner; BK4819 wins on simultaneous failure. Closes the silent-misbehaviour gap inherited from DualTachyon and present in every UV-K5 fork: a wedged-I2C bus or blanked EEPROM causes `helper/battery.c:101` to divide by `gBatteryCalibration[3] == 0xFFFF` (pinning voltage near zero and rendering permanently-empty battery icon), an all-0xFF `gEEPROM_RSSI_CALIB[*]` to mis-render the S-meter, and gain staging to walk to extreme rails -- with no diagnosis surfaced to the operator. `SETTINGS_LoadCalibration()` at `settings.c:295-301` partially recovers `gBatteryCalibration[0..1]` and `[5]` with hard-coded defaults but leaves `[2..4]` at 0xFFFF and provides no diagnosis. Today's probe makes the failure visible at the right layer (hardware) at the right moment (boot, before any feature runs). LAW 1 preserved (`driver/eeprom.h` / `driver/eeprom.c` byte-identical with parent; access via public API only). LAW 4 preserved (single-cycle bitwise AND, no software-divide pull). |
| v1.2.7 | 2026-05-06 | **Boot-time EEPROM stuck-low SDA detection (Feature #35 extension, +24 bytes)**: `BOOT_HEALTH_Probe()` extends the EEPROM 0x1F40 check to catch a strictly-different I2C-bus failure mode. The existing AND-cascade against 0xFF detects the floating-bus (no slave) and blanked-chip cases; a new OR-cascade against 0x00 detects the stuck-low SDA case where a slave or external short to GND holds SDA low while the master clocks SCL. The bit-bang `I2C_Read` at `driver/i2c.c:47-87` reads SDA via `GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA)`, so a stuck-low line returns 0 on every clocked bit and 0x00 across the 8-byte page. NXP UM10204 section 3.1.16 ("Bus clear") documents stuck-low SDA as a canonical I2C failure mode addressable via 9-clock bus-recovery (deferred Proposal 2). settings.c:295-301 partial-recovery invariants (gBatteryCalibration[0]/[1] never both zero on a factory unit, never 0xFFFF on a healthy one) bound false-positive risk to zero on every UV-K5 that ships from Quansheng. Both signatures map to the same `FAULT_BIT_EEPROM` flag because the welcome-screen banner ("EEPROM FAULT / calib lost") and the existing `BOOT_HEALTH_HasEepromFault()` predicate are general enough to cover either failure mode -- no new bit, no new banner, no new predicate, no new exports. LAW 1 preserved (`driver/i2c.c`, `driver/eeprom.c`, `ui/welcome.c` byte-identical with their pre-change state; `boot_health.c` is VUURWERK-original GPL v3). LAW 4 preserved (single-cycle ORS / ANDS instructions on Cortex-M0; no FPU, no heap, no software-divide pull). |
| v1.2.7 | 2026-05-07 | **Boot-time EEPROM two-pass consistency check (Feature #35 extension, -4 bytes)**: `BOOT_HEALTH_Probe()` now reads the 0x1F40 page a second time into a separate stack buffer and compares the two reads via `memcmp`. Any byte-divergence between the two passes sets `FAULT_BIT_EEPROM`. Catches the third orthogonal I2C-bus failure mode -- garbled / intermittent reads where individual bytes appear plausible (NEITHER all-0xFF NOR all-0x00) but vary across passes: stochastic bus noise, marginal slave that occasionally NACKs / produces wrong data, ESD-induced glitches that haven't fully settled, marginal voltage rails on the EEPROM Vcc, signal-integrity issues from a damaged trace. Two consecutive reads of the same address must be byte-identical on healthy hardware (24LC64 is static storage; bit-bang driver is deterministic; no writer between the two reads since boot_health is invoked before the main loop runs), so any divergence is a real fault and false-positive risk on healthy hardware is mathematically zero. NXP UM10204 section 3.1.6 confirms single-transaction success does not guarantee bus integrity; multi-pass attestation is canonical for safety-critical bus probes. `memcmp` is already pulled by `driver/eeprom.c:50` (EEPROM_WriteBuffer's pre-write coalesce) so the linker contributes zero new bytes for the function itself; only the call-site adds flash. Net flash impact -4 bytes because LTO de-inlined `EEPROM_ReadBuffer` once a second call site appeared, which dwarfed the new memcmp call cost. Boot-time impact: one extra ~720us EEPROM page read at the bit-bang rate, imperceptible to operators. With this advancement, VUURWERK now catches three orthogonal I2C-bus failure axes (deterministic-stuck-high, deterministic-stuck-low, stochastic-inconsistent); no upstream UV-K5 fork (DualTachyon / egzumer / kamilsss655 / fagci) ships any of the three probes. LAW 1 preserved (`driver/i2c.c`, `driver/i2c.h`, `driver/eeprom.c`, `driver/eeprom.h`, `ui/welcome.c`, `helper/battery.c` byte-identical with their pre-change state; `boot_health.c` is VUURWERK-original GPL v3). LAW 4 preserved (memcmp is byte-by-byte compare; no FPU, no heap, no software-divide pull). |
| v1.2.7 | 2026-05-08 | **dual_watch_mgmt.c per-tick Update collapsed into edge-only ReportActivity (Feature #15 reclamation, -56 text bytes / -4 BSS bytes)**: deleted the `DUAL_WATCH_MGMT_Update` function and its 100 Hz call site at `app/app.c:1364`; moved the dwell-time cascade + `[200, 2000] ms` clamps into `DUAL_WATCH_MGMT_ReportActivity` (where `activity_count[]`'s mutations already live), so the cascade now runs exactly when its inputs change instead of 100 times per second between changes. Removed the dead `avg_rssi[2]` struct field, its `(avg*3+new)/4` IIR update, and its two `-120` `Init` writes (full-repo grep confirms zero readers). Removed the `bool active` parameter from the public API (unused, always-`true` at the only call site, `(void)active;` in the deleted body). Operator-visible behaviour preserved exactly: same dwell decisions, same `[200, 2000] ms` envelope, same 256-tick activity decay. Rotates `BUDGET_FOR_ADD` from 12 to 68, re-opening the budget for previously-blocked deferred capability proposals (e.g. `ctcss_lead.c -- Adaptive lead-in time per CTCSS frequency band`, unlock condition `free bytes >= 130`, now closer to clearable). LAW 1 N/A (`dual_watch_mgmt.c/h` are VUURWERK-original); the single `app/app.c` edit lives inside the existing `// === VUURWERK v1.1.0 RX PROCESSING ===` marker block. LAW 2 unchanged (no register ownership). LAW 4 / LAW 5 preserved. |
| v1.2.7 | 2026-05-07 | **Boot RELEASE-ALL-KEYS screen identifies the offending key (driver/keyboard.c research, +176 bytes)**: `UI_DisplayReleaseKeys()` in `ui/welcome.c` now appends a third small-font line below the existing "RELEASE / ALL KEYS" banner naming the held key. PTT is checked first via direct `GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)` because `KEYBOARD_Poll()` deliberately omits PTT (`driver/keyboard.c:99-100`). For non-PTT keys, the held key code from `KEYBOARD_Poll()` maps to a short string: digits 0-9 derived from `'0' + (char)k` (avoids 10 per-digit string literals -- KEY_Code_t enum values 0..9 are deliberately the digit keys); non-digit named keys (SIDE1, SIDE2, MENU, EXIT, UP, DOWN, STAR, F) explicit switch case. KEY_INVALID and unknown-key cases fall through to no-name-shown (race-safe -- if operator releases between the main.c gate check and the welcome render, behaviour is identical to current static banner). Diagnostic value: collapses the rare genuine-stuck-key scenario from minutes-to-hours of trial-and-error to a single targeted inspection. Membrane keypad reliability literature confirms PTT sliders and side rubber buttons are the highest-failure-rate components on consumer-grade radios; PTT is therefore checked first and named explicitly. LAW 1 preserved (`driver/keyboard.c`, `driver/keyboard.h`, `helper/boot.c`, `helper/boot.h`, `main.c` byte-identical with their pre-change state; only `ui/welcome.c` modified, already a VUURWERK-modified file). LAW 4 preserved (switch on small enum compiles to comparison sequences on Cortex-M0; no modulo; 6-byte stack buffer for the digit case). |
| v1.2.7 | 2026-05-11 | **Quiet Backlight PWM (Feature #42, +20 bytes)**: NUNU 6 kHz PWM carrier ported LAW-1-safely via a single VUURWERK marker block in `main.c` re-programming `PWM_PLUS0_CLKSRC` (mask-then-OR override of stock prescaler 46 -> 7) after `BOARD_Init` runs. Removes the 1 kHz outbound TX-audio whine that bleeds into receivers when the operator's screen is dimmed. Closes a long-known TX-audio artefact present in DualTachyon, egzumer, and fagci forks; matches kamilsss655's bench-validated source-line fix without modifying the stock `driver/backlight.c`. Mission-aligned with existing TX-side intelligence (Feature #1/#2/#3). `bsp/dp32g030/pwmplus.h` and `driver/backlight.c` byte-identical with egzumer parents. |
| v1.2.6 | 2026-05-13 | **Spectrum keymap reorg + voice-hop robustness + visual polish (release v1.2.6, +312 text / +4 BSS)**: `app/spectrum.c` SIDE1 short now steps frequency UP with auto-repeat (was: Blacklist). SIDE2 short now steps frequency DOWN, release-dispatched and single-step (was: ToggleBacklight). SIDE2 long-press hold counter at 32 ticks (~640 ms) fires ToggleBacklight once and latches a `s_side2_long_fired` flag that suppresses the release-side short, so short and long are unambiguously separated. MENU short now fires Blacklist (was: empty case). UP / DOWN unchanged (voice-hop in VOX, freq step in NORM / PEAK / MTI). Voice-hop loop now skips blacklisted bins via `IsBlacklisted(vi)` (Feature #21 robustness). VOX drawing tier threshold raised from 35 to 50 to match hop threshold (so every visible bar is hop-eligible). 1-pixel marker tick painted at y=0 over every hop-eligible bin (visible "hop map" row across the top of the spectrum). STAR mode-change populates `s_mode_toast_renders=6` to render a `MODE <name>` overlay for ~6 renders, in addition to the permanent small label at (108,1). Three new BSS bytes: `s_side2_hold_ticks` (uint16), `s_side2_long_fired` (uint8), `s_mode_toast_renders` (uint8). LAW 1 N/A (`app/spectrum.c` is VUURWERK-modified). LAW 4 preserved (integer, no FPU, no software divide). |
| v1.2.7 | 2026-05-13 | **SIDE2 symmetry fix + CSS scanner robustness + economic re-engineering + license header sweep (release v1.2.7, -212 text / -4 BSS net vs v1.2.6)**: PART 1 reverts the v1.2.6 SIDE2 release-dispatch / long-press state machine in `app/spectrum.c`: SIDE2 short now press-dispatched with auto-repeat (symmetric with SIDE1), `s_side2_hold_ticks` + `s_side2_long_fired` BSS removed, dispatch suppression gate removed. ToggleBacklight no longer reachable from spectrum view (use EXIT + main-screen gesture). PART 2 rebuilds the CSS scanner: pre-flight RSSI gate in `SCANNER_Start` (raw 100 / ~-110 dBm threshold; aborts with `NO SIGNAL` toast + 500 Hz double-beep on dead air), CDCSS/CTCSS arms merged in `SCANNER_TimeSlice10ms` (CTCSS dropped from 2-confirmation to 1-confirmation to match DCS), `scan_delay_10ms` reduced 21 -> 12 (210 ms -> 120 ms) in `misc.c`, soft-timeout in `app/app.c` reduced 120 -> 60 ticks (60 s -> 30 s). PART 3A reclamation: `TX_COMPRESSOR_GetGainReduction` removed (zero callers), `RSSI_HISTOGRAM_Init` removed (BSS zero-init covers it; main.c caller removed), `GAIN_STAGING_Reset` made static (zero external callers, LTO inlines), `bandscope.c` parallel memmoves consolidated into a single loop, `SIGNAL_CLASSIFIER_Init` removed (BSS zero-init covers it; main.c caller removed). PART 3B CTCSS-scoped consolidation: CDCSS/CTCSS arm merge in `SCANNER_TimeSlice10ms` (one Code lookup + one assign-and-FOUND block instead of two). PART 4 license header enforcement sweep (comment-only, flash-neutral): 51 source files normalized to the v1.2.7 canonical dual-license header with commercial-licensing contact line; LICENSE Dual-Licensing Notice clarified (contributions list above the delimiter byte-identical); README License section added near the top. LAW 1 preserved (stock egzumer files unchanged; only VUURWERK-modified `app/scanner.c`, `app/spectrum.c`, `app/app.c`, `misc.c`, `main.c` edited; new VUURWERK code in marker blocks). LAW 4 preserved (integer math throughout). LAW 5 preserved (no em-dashes in new content). LAW 7 preserved (LICENSE contributions list byte-identical above the delimiter). |

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
| Status Line buffer module | status_line.c/h | v1.2.5 | Phantom: STATUS_LINE_Get had no callers; visible bottom-line context already rendered by ui/main.c:828-868. Active Feature #23 re-anchored to its real site, files deleted. |
| Activity Log + RF Log Viewer | activity_log.c/h, ui/vuurwerk_about.c (DOWN-key handler) | v1.2.6 | HT form factor makes on-radio log review impractical; 128x64 LCD cannot show enough log context to be operationally useful during a contact. Feature #27 retired entirely; Feature #25 reverted to About Screen only. -844 text bytes / -328 BSS bytes returned to capability headroom. |

---

## LTO-Stripped Functions (compiled but zero flash cost)

These functions exist in source but have no reachable call path from `main()`. LTO strips them:

- `RSSI_HISTOGRAM_GetOptimalSquelch()`: squelch value not read externally
- `VFO_SPLIT_GetHitCount()`, `GetLastHitFreq()`, `GetLastHitTime()`, `SwitchToB()`: status queries not wired

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
