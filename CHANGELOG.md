# VUURWERK Changelog

## v1.2.3 (2026-02-21): Current Release

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
