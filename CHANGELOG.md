# VUURWERK Changelog

## v1.2.7 (May 2026)

SIDE2 keymap symmetry fix, CSS scanner robustness, and economic
re-engineering.

### Fixed (v1.2.6 keymap correction)

- **SIDE2 long-press incorrectly toggled backlight** instead of
  continuing freq-down auto-repeat. The "clean short / long
  separation" model introduced in v1.2.6 was operationally wrong:
  operators expect SIDE2 hold to mirror SIDE1 hold (freq UP and freq
  DOWN should be symmetric). SIDE2 is now press-dispatched with
  auto-repeat, symmetric with SIDE1. ToggleBacklight is no longer
  accessible from spectrum view; operators wanting to toggle the
  backlight while in spectrum should press EXIT first, toggle from
  the main screen, and re-enter via F+5. This is a deliberate UX
  simplification, not a feature loss.

### Fixed (CSS scanner robustness)

- **Pre-flight RSSI gate** added to the CSS scanner. Before launching
  the BK4819 scan engine, `SCANNER_Start(true)` samples RSSI on the
  current frequency; if it reads below carrier-present threshold
  (about -110 dBm), the scan aborts with a `NO SIGNAL` toast and a
  500 Hz double-beep. Today's behaviour on dead air was to run the
  scanner for 60 seconds and surface SCAN FAIL; new behaviour is
  instant feedback that the operator parked on a quiet frequency.
- **CTCSS confirmation logic** changed from two-consecutive-matches
  to single-match, matching the existing DCS behaviour. CTCSS scans
  no longer feel broken next to DCS scans. The BK4819 detector's
  internal voting plus the new pre-flight RSSI gate together provide
  the confidence the second confirmation used to add.
- **Dwell time reduced** from 210 ms to 120 ms (`scan_delay_10ms` in
  misc.c, 21 ticks to 12 ticks). 120 ms is enough for the BK4819
  CTCSS detector to converge on the lowest tone (67 Hz, ~8 cycles)
  with margin. Affects both RF frequency scan and CSS scan.
- **Soft-timeout tightened** from 60 seconds to 30 seconds. With
  faster dwell, single-match CTCSS, and the pre-flight gate, typical
  successful scans complete in 3-10 seconds; 30 seconds is the new
  "this is fruitless" cap.
- **FOUND validation considered but not implemented.** The BK4819
  detector's internal voting plus the pre-flight RSSI gate plus
  sufficient dwell already provide high confidence on FOUND; post-
  validation cost (extra ~500 ms per scan, ~50-100 bytes flash) was
  unfavourable.

### Changed (Economic re-engineering)

- **`TX_COMPRESSOR_GetGainReduction` removed.** Function had zero
  callers across the tree (verified via grep). Dead code.
- **`RSSI_HISTOGRAM_Init` removed.** Function only zeroed the
  histogram global; the C runtime zero-clears BSS at boot, making
  the Init redundant. The main.c call site removed too.
- **`GAIN_STAGING_Reset` made `static`** to `gain_staging.c`. Zero
  external callers; LTO can now fully inline into the single
  caller. Header prototype removed.
- **`bandscope.c` memmove pair consolidated.** Two consecutive
  memmoves on different buffers replaced with one combined loop.
- **`SIGNAL_CLASSIFIER_Init` removed.** Function only set
  `prev_rssi = -127` (the rest of the struct was BSS-zero-default).
  Now relies on BSS zero-init; the worst case is one extra NOISE
  classification on the first signal post-boot before the algorithm
  self-corrects to FAST / NORMAL / SLOW. The main.c call site
  removed too.
- **CSS scanner CDCSS/CTCSS arms merged** in
  `SCANNER_TimeSlice10ms`. Once CTCSS dropped to single-match (see
  Fixed section above), the two arms became structurally identical
  and collapse into a single result-handling block, parameterised
  only by which DCS table to query and which CodeType to assign.

### Changed (License header enforcement sweep)

- Every VUURWERK-original source module now carries the canonical
  dual-license header including the explicit commercial-licensing
  contact line. 51 files normalised in one sweep: 22 had the v1.2.6
  canonical header (missing the contact line) and were updated;
  another 8 had non-canonical or SPDX-only headers and were rewritten
  with the canonical block (preserving any technical descriptive
  content as a separate comment immediately after); `battery_sag.c`
  needed manual layout adjustment.
- **LICENSE Dual-Licensing Notice section clarified** to make the
  complete-firmware-GPL-v3 / extracted-module-commercial boundary
  unmistakable. The notice now explicitly enumerates what "outside
  the complete VUURWERK firmware" means (extraction, porting,
  repackaging, commercial use) and confirms that marker-block
  contributions in upstream-derived files (app/app.c, app/main.c,
  app/spectrum.c, ui/main.c, ui/status.c, ui/welcome.c, main.c,
  misc.c) are protected by the LICENSE contributions list. The
  contributions list above the Dual-Licensing Notice delimiter is
  byte-identical (LAW 7).
- **README License section** added near the top with the same
  boundary expressed in operator-facing language.
- **Zero flash impact** -- all header sweep changes are comments,
  stripped by the preprocessor before codegen.

### Released

- `vuurwerk-v1.2.7.packed.bin`: 60,790 bytes, SHA-256:
  1e1486c27baefce73d817dec45053914a7a128b8f8af92b01c8760bf270740b0
- Shipped to `release/` on the dev repository.

---

## v1.2.6 (May 2026)

Spectrum analyzer keymap reorganization, voice-hop robustness, and
visual polish.

### Fixed

- **Voice-hop now respects the blacklist.** The spectrum VOX mode's
  UP/DOWN voice-hop loop did not check `IsBlacklisted(idx)` before
  landing on a voice-probable bin, so a blacklisted bin with strong
  voice probability would still be tuned. With Blacklist now moving to
  the more discoverable MENU short-press in spectrum view, the
  blacklist is expected to see more use; the missing check is fixed.
- **Spectrum analyzer VOX mode keymap reorganization.** UP/DOWN in VOX
  mode previously consumed the voice-hop path and never reached the
  frequency-step path, leaving operators with no way to manually step
  frequency while in VOX. Resolved by moving frequency stepping to
  SIDE1 (up) and SIDE2 (down) as universal gestures across all spectrum
  modes, so UP/DOWN remains the voice-hop in VOX and the frequency-step
  in NORM/PEAK/MTI as before.

### Changed

- **Spectrum view keymap reorganized.** Operators upgrading from v1.2.5
  will need to relearn Blacklist and Backlight access in spectrum view.

  | Key | Before (v1.2.5) | After (v1.2.6) |
  |---|---|---|
  | SIDE1 short | Blacklist current bin | Frequency UP (one step) |
  | SIDE2 short | Toggle backlight | Frequency DOWN (one step) |
  | SIDE2 long  | (none) | Toggle backlight |
  | MENU short  | (none) | Blacklist current bin |
  | UP / DOWN   | Freq step (NORM/PEAK/MTI), voice-hop (VOX) | Unchanged |

  SIDE1 short is now press-dispatched with auto-repeat: tap for one
  step, hold for continuous fast tuning up.

  SIDE2 short is now release-dispatched (one step per tap, no
  auto-repeat). SIDE2 long fires once at the 640 ms hold threshold and
  suppresses the release-side short action, so short and long are
  unambiguously separated.

  MENU short is press-dispatched (one-shot).
- **Spectrum mode-change feedback.** STAR now flashes a brief
  centered `MODE <name>` label for ~6 renders on mode change, in
  addition to the permanent small label in the top-right corner.
  Closes the "operators only see NORM" issue where the permanent label
  was too small to register at a glance.
- **VOX bar drawing tier matches hop threshold.** Bins with voice
  probability below 50 now drop to a floor dot (previously 35); bins
  at 50 or above draw as bars. Visible bars now equal hop-eligible
  bins, so the visual matches the navigation behaviour.
- **Voice-probable marker tick in VOX mode.** Every bin with
  `voiceProb >= 50` now gets a 1-pixel marker at the top of the
  spectrum display in addition to its bar, so hop targets are visible
  at a glance even among similarly-tall bars.

### Released

- `vuurwerk-v1.2.6.packed.bin`: 61,002 bytes, SHA-256:
  7a14b55039f2a9de6f22faf858f3c64fcc7d2cf372309d42208f47a375ded884
- Shipped to `release/` on the dev repository.

---

## v1.2.5 (May 2026)

Feature release adding diagnostic, reliability, and UX improvements across
the boot path, battery telemetry, backlight behaviour, scanning, and the
voice-seeking spectrum mode.

### Added

- **Side-Button Toast Feedback** (Feature #28). 1-second on-screen toast
  confirms SIDE1 short, SIDE1 long, SIDE2 short, SIDE2 long, and MENU
  long actions, so every shortcut path -- F+keys, side-buttons, long
  presses -- shares the same visible confirmation contract. Closes the
  UX-parity gap inherited from upstream where side-button actions were
  silent.
- **Toast Notification Subsystem** (Feature #29). Extracted to a dedicated
  toast.c/h module. Powers all F+key shortcuts, side_toast, and any
  future shortcut surface. The extraction restored LAW 1 byte-identity
  on app/app.h and saved 204 bytes via LTO re-inlining gains.
- **Scan Rate Telemetry** (Feature #30). Status line now shows live
  channels-per-second readout during scan ("SCANNING NNc/s"). 100-tick
  rolling window. Operators can see at a glance whether squelch is
  robbing scan throughput or the band is just empty.
- **F-Key UX Hardening** (Feature #31). F-hold now shows a LOCKED toast.
  F+3 emits "NO CHANNELS" when the channel list is empty (was a silent
  no-op). F+\* is gated to FM-mode only (was a silent failure in AM/SSB).
  STAR long-hold now shows a SCAN ON / SCAN OFF toast.
- **Flashlight Auto-Off Watchdog** (Feature #32). Auto-extinguishes the
  flashlight after 30 minutes of continuous ON or BLINK, or 10 minutes
  on a low-battery pack. SOS is preserved indefinitely. Prevents the
  belt-clip / pocket bump from running the pack dead overnight.
- **CSS Scan Soft-Timeout Watchdog** (Feature #33). 60-second cap on CSS
  scans with an audible double-beep on timeout. Both the F+\* and menu
  scan paths are covered. Prevents the unbounded-scan trap inherent to
  FM-with-no-tones.
- **Audio Palette VOX-Hop Cues** (Feature #34). Spectrum VOX mode plays
  a short ascending two-tone chord (800 Hz + 1200 Hz) when UP/DOWN
  finds a voice-probable bin, and a soft 500 Hz double-beep when it
  sweeps the range without finding voice. Tied to the
  Intelligence-Based Squelch voice-probability scorer.
- **Boot-Time Hardware Health Probe** (Feature #35). At every power-on,
  the firmware checks BK4819 SPI liveness and the EEPROM I2C
  battery-calibration page. Faults are surfaced as a "BK4819 FAULT /
  RX/TX disabled" or "EEPROM FAULT / calib lost" welcome banner. First
  UV-K5 firmware to boot with a hardware self-test.
- **Live Battery Voltage During TX** (Feature #36). Battery icon and
  voltage display update live during transmit. Stock firmware froze the
  reading on PTT entry. Operators on long key-down (contests, repeater
  nets) now see real-time sag instead of a stale value.
- **Backlight Fade-Out Tail** (Feature #37). 2-second linear taper to
  off at the end of the idle window, instead of cutting to black
  abruptly.
- **Backlight TX/RX Activity Refresh** (Feature #38). Re-arms the
  backlight countdown during TRANSMIT, RECEIVE, INCOMING, and MONITOR
  when the per-mode `BltTRX` bit is set, so the screen never goes dark
  mid-conversation.
- **TX Battery Sag Delta Tracker** (Feature #39). Per-PTT peak voltage
  sag is captured by comparing the clean-idle baseline against the
  during-TX minimum and is displayed on the About screen as
  "TX sag NNNNmV". Trends across sessions help operators tell a
  healthy pack from a tired one.
- **CSS Scan Status-Bar Glyph** (Feature #40). CSS scans now show "Cs"
  in the status bar instead of the generic "S" used for channel scans.
  Makes CSS-scan flavour unambiguous at a glance.
- **CSS Scan FOUND Beep** (Feature #41). Audible 1 kHz single-tone cue
  when a CSS scan locks onto a tone, paired with Feature #33's FAILED
  double-beep for symmetric audible feedback.
- **Quiet Backlight PWM** (Feature #42). PWM carrier raised from 1 kHz
  to ~6.7 kHz at boot, above the radio's 3 kHz audio passband. Removes
  the 1 kHz whine that stock firmware bleeds into outbound audio when
  the screen is dimmed during TX. Mission-aligned with the existing TX
  intelligence (Soft Start, CTCSS Lead-In, Compressor). Credit:
  kamilsss655 / NUNU (2024) bench-validated and fixed this approach on
  another UV-K5 fork; VUURWERK matches that TX-audio quality result
  via an independent mask-then-OR implementation in main.c.

### Changed

- **Intelligent Dual-Watch** rewritten to weight dwell decisions by
  cumulative RX duration rather than per-tick boolean activity. A 30 s
  QSO now weights 30x more in the dwell cascade than a 1 s carrier
  blip.
- **Signal Quality Metrics** variance ring is now reset on channel or
  VFO change. Closes the stale-variance reading that previously persisted
  for ~8 samples across a VFO flip.
- **Squelch Tail Elimination** CTCSS-loss confirmation is now
  confidence-weighted. Stable signals confirm tail-loss at 20 ms; the
  EWMA-flagged fluttery signals confirm at 30 ms. Reduces false squelch
  closures on marginal signals.

### Removed

- **Activity Log** (Feature #27). The 20-entry ring buffer and uptime
  counter are retired. The 128x64 LCD could not show enough log context
  to be operationally useful in HT form factor. The #27 slot is
  intentionally empty going forward.

### Released

- `vuurwerk-v1.2.5.packed.bin`: 60,672 bytes, SHA-256:
  2695c154a9277fd0393c514f5e969650741facbda2b2031c4e91a6520a09ee72
- Shipped to `release/` on the dev repository.

---

## v1.2.4 (April 2026)

Bugfix release addressing three community-reported issues.

### Fixed

- **Spectrum analyzer freeze on entry.** The `scanStepBWRegValues[]` array was missing three entries for the larger scan steps (25 kHz, 50 kHz, 100 kHz), causing an out-of-bounds read on entry with the default scan step. Combined with a no-timeout wait loop in `GetRssi()`, this hung the radio requiring a battery pull. Fixed the array size, added a 50-iteration timeout to `GetRssi()`, and corrected the return type of `GetBWRegValueForScan()` from uint8_t to uint16_t (a latent bug that was silently truncating filter bandwidth register values). Thanks to JoseSoler (https://github.com/JoseSoler) for the detailed root-cause analysis and exact code fix in issue #5.

- **Channel name drift in Dual Watch.** After 4+ hours of Dual Watch operation, the displayed channel name would switch to Channel 10's name on both VFOs, while RX continued correctly on the saved channels. A defensive guard has been added to the display path with a trip counter visible on the About screen. Root cause investigation continues and user reports of trip counts will inform the v1.2.5 fix. Thanks to rieschrispy (https://github.com/rieschrispy) for the detailed reproduction steps in issue #4.

- **About screen now accessible from the CONFIG menu.** Previously About was hidden behind a long-press gesture in the category picker that wasn't discoverable. About is now a regular menu item under CONFIG: scroll to it and press MENU. Thanks to ErikS-web (https://github.com/ErikS-web) for the feedback in issue #1.

---

## v1.2.3 (2026-02-21)

### Fixes
- Fixed: Activity Log now records RF activity (frequency, RSSI, tone, duration) on every squelch open
- Fixed: Intelligent Dual-Watch dwell times now adapt based on VFO activity
- Fixed: VFO Split now accessible via SCANWATCH menu (option 3: SPLIT)
- Fixed: Signal Classifier F/N/S/~ symbol now displayed on status line during RX
- Fixed: FEATURES.md LTO-stripped list corrected (removed 3 false entries for working functions)
- Updated: LICENSE attribution expanded with all VUURWERK-original contributions

### Optimizations
- Removed snprintf from status_line.c (eliminated stdio.h linkage)
- Replaced modulo operators with comparison+reset (avoided software division library)
- Simplified activity_log Init to memset
- Reused SCANWATCH menu for VFO Split (zero new menu item overhead)

### Binary
- Text: 59,164 bytes (2,276 free, 3.71% headroom)
- vuurwerk-v1.2.3.packed.bin

---

## v1.2.2 (2026-02-20)

### Changes
- Code optimization pass
- Text size: 61,084 bytes (356 free)

---

## v1.2.1 (2026-02-16)

### Bugfix Release
- **Voice-hop fix:** `ToggleRX(true)` instead of `SetState(STILL)`, stays in SPECTRUM state
- **Voice gate:** auto-listen skips non-voice bins (voiceProb<50), `UpdateListening()` checks noise register
- **Mode label:** "VOIC" renamed to "VOX " (clearer on 3x5 font)
- **DrawSpectrum:** cases 0/1 merged (saved 40 bytes)
- **Dead code:** removed sprintf("%s",name), idx==5 guard, redundant bin<128 checks
- **VOIC drawing:** 2-tier confidence (was 3-tier), removed h<2 guard

Text size: 61,192 bytes (44 free, -36 from v1.2.0)

---

## v1.2.0 (2026-02-16)

### Voice-Seeking Spectrum
- **VOX mode** (replaces waterfall): voice probability per spectrum bin via REG_0x65 noise indicator
- `ComputeVoiceProb()` in spectrum.c: noise + RSSI scoring, integer-only, max 95
- VOX drawing: bars scaled by confidence (half/<75, full/>=75), floor dots for non-voice
- **Voice-hop:** UP/DOWN jumps to next bin with VP>=50, uses ToggleRX(true)+TuneToPeak()
- **STE menu toggle wired:** squelch_tail.c checks gEeprom.TAIL_TONE_ELIMINATION
- Smart squelch always-on (no separate toggle; flash too tight for MENU_SQIN)
- Dead API removed: GetVoiceProb(), QuickVoiceProb() from smart_squelch.c/h
- **Waterfall removed:** saved 200 bytes text + 896 bytes BSS

Text size: 61,228 bytes (212 free)

---

## v1.1.0 (2026-02-15)

### Squelch Tail Elimination + Intelligence-Based Squelch
- **squelch_tail.c:** 4-state machine (IDLE/MONITORING/TONE_LOST/MUTED)
  - Reads REG_0C (CTCSS status), writes REG_47 (AF mute)
  - 20ms confirmation, 150ms timeout, handles all STE methods
- **smart_squelch.c:** 3-register hardware VAD (REG_67/65/63), EWMA smoothing, dynamic squelch
  - Voice probability 0-100 from noise, glitch, and SNR scoring
  - Dynamic REG_78 adjustment: -6 to +4 steps based on VP

Text size: 61,040 bytes (400 free)

---

## v1.0.9.7 (2026-02-15)

### Dead Code Cleanup
- Removed orphaned files and modules, archived tx_power_mgmt/battery_model
- Text: 60,452 bytes, BSS dropped 896 bytes

---

## v1.0.9.6 (2026-02-15)

### Gain Staging Rewrite + S-Meter
- gain_staging.c rewritten: FOREGROUND+RX guard, frequency change detect, S-meter compensation
- RSSI bar enabled (ENABLE_RSSI_BAR=1)
- Text: 60,500 bytes

---

## v1.0.9.5 (2026-02-15)

### TX Chain Integration
- TX soft start, CTCSS lead, TX compressor fully wired to app.c and functions.c
- TX_RAMP_STEPS fix (was 3, now 6 for proper 60ms S-curve)
- Removed dead modules from Makefile: alc_learning, preemph_opt, tx_power_mgmt
- Text: ~60,150 bytes

---

## v1.0.9.4 (2026-02-14)

### Major Cleanup
- Dead code removed across all modules
- signal_classifier fix: proper rise-time measurement
- gain_staging rewrite began
- Text: ~59,200 bytes

---

## v1.0.9 (2026-02-14)

### Categorized Menu System
- 6 visible categories + hidden UNLOCK
- 63 menu items organized by function
- Number key category selection

---

## v1.0.8 (2026-02-14)

### Intermediate Bug Fixes
- TX compressor, CTCSS lead, tx_soft_start module creation
- VFO split, bandscope modules created

---

## v1.0.4 (2026-02-14)

### Feature Integration
- Spectrum modes (NORM/PEAK/MTI) wired
- Scan+Watch, bandscope, RX signal chain fully integrated
- Signal classifier, RSSI histogram, dual-watch mgmt, status line added
- About screen, activity log

---

## v1.0.0 (2026-02-13)

### Initial Release

**Based on**: Egzumer 2.0 Firmware

- 22 modules created (TX chain, squelch, scanning, spectrum, UI)
- Most integration pending. Modules compiled but not all wired
- DTMF calling removed for space (~12KB saved)
- Text: ~57,800 bytes

### Credits
- **Base firmware**: Egzumer 2.0
- **Original project**: DualTachyon UV-K5 Firmware
- **Firmware design and architecture**: James Honiball (KC3TFZ)
