# VUURWERK

**Custom firmware that turns a $30 Quansheng UV-K5 into a radio that listens, adapts, and responds like equipment ten times its price.**

VUURWERK is Afrikaans for *fireworks*.

**v1.2.3** | 59,164 bytes | 27 active features | Zero warnings | GPL v3

---

## See It In Action

When you first power on a VUURWERK radio, the boot screen shows the firmware name and version. Then you see the main display:

```
+-------------------------------+
| [Q===]          [F]  [BATT██] |  Status bar: signal quality, lock/F key, battery
|                               |
| A  146.520.00        FM  H  N |  VFO A: frequency, modulation, power, bandwidth
|                               |
| ▄▂▅▇▃▁▂▄▆▅▃▂▁▃▅▇▆▄▂▁▃▅▄▂▁▃▅ |  Bandscope: live RF activity on your frequency
|                               |
| B  446.000.00        FM  M    |  VFO B: frequency, modulation, power
|                               |
| 12.5kHz N FM                  |  Status line: step, bandwidth, modulation (idle)
+-------------------------------+
```

Stock egzumer shows two frequencies and a battery icon. VUURWERK adds a live bandscope showing who is transmitting nearby, a signal quality meter that tells you propagation conditions at a glance, and a context-aware status line that changes based on what the radio is doing.

When a signal comes in, the bottom line switches to:

```
| RX S7 -65dBm Q:3 F            |  S-level, signal strength, quality, classifier
```

That trailing "F" is the signal classifier telling you this is a fast-rise FM signal. "N" would mean SSB. "S" would mean a carrier. "~" would mean noise. The radio figured that out by measuring how quickly the signal appeared.

---

## What Changes When You Flash This

### You'll hear stations you couldn't before

**FM Gain Staging** measures signal strength in real time and adjusts the front end automatically. Strong signals that crackle on stock firmware come in clean. Weak repeaters that were buried in noise become copyable. It resets when you change channels so you never get stuck with wrong gain.

**Adaptive Squelch** tracks the noise floor on your frequency and adjusts the squelch threshold to match. Works better than a fixed squelch level in changing RF environments.

**Intelligence-Based Squelch** scores every signal for voice probability using three hardware measurements at once. Instead of asking "is there signal?" it asks "is there a human talking?" Opens for voices, stays closed for interference and noise.

*Scenario: You're monitoring a weak repeater while a nearby pager transmitter keeps breaking squelch. Stock firmware opens for both. VUURWERK scores the pager as high-glitch/low-voice-probability and keeps squelch closed. The repeater scores as clean audio and opens immediately.*

### You'll sound better to everyone listening

**TX Compressor** monitors your mic audio in real time and automatically reduces gain when you're loud, increases when you're quiet. Everyone hearing you gets consistent, clean audio instead of whisper-to-shout swings.

**CTCSS Lead-In** mutes the mic for 150ms at the start of every transmission so the CTCSS tone reaches full amplitude before your voice hits the air. No more repeaters cutting off your first word.

**TX Soft Start** ramps transmit power up on an S-curve over 60ms instead of slamming full power instantly. Eliminates the key-up click/pop that other radios hear.

*Scenario: You key up on a repeater. Instead of the usual click-"ello this is..." with the first syllable lost, the repeater hears a clean CTCSS tone, opens smoothly, and your full first word comes through. Other hams notice you sound noticeably cleaner than typical UV-K5 users.*

### You'll find activity faster

**Bandscope** shows a live RF activity display on the main screen (F+7 to toggle). Bars represent signal strength on frequencies around your current position. You can see who's transmitting nearby without scanning.

**Spectrum Analyzer** has 4 modes (F+5): Normal (live signal), Peak (hold max values), MTI (show only new/changing signals), and VOX (automatically jumps to the next frequency with voice activity). Press Star to cycle modes.

**Scan+Watch** runs scan and dual-watch simultaneously on both VFOs. A feature normally found on radios costing $300 or more.

**Calling Frequency Jump** (F+9) auto-tunes to the correct simplex calling frequency for your current band, sets the right step size and modulation, and press again to return. Supports 2m FM/SSB, 70cm FM/SSB, MURS, FRS/GMRS, CB, Marine, Airband.

*Scenario: You're at a ham event and want to find who's talking on 2m. Hit F+5, switch to VOX mode with the Star key, and the analyzer automatically hops to the next frequency with voice activity. No other UV-K5 firmware can do this.*

### You'll spend less time in menus

**Categorized Menu** organizes all settings into 7 categories. Press a number key to jump straight to the category. No more scrolling through 50+ items to find backlight settings.

**F+Key Shortcuts** give you one-press access to the settings you change most: power (F+6), squelch (F+4), bandwidth (via menu), modulation (F+0), spectrum (F+5), bandscope (F+7), calling frequency (F+9).

**Toast Notifications** show a 1-second overlay confirming every shortcut action ("PWR: HIGH", "SQL: 5", "SCOPE ON").

**Context-Aware Status Line** at the bottom of the screen changes automatically to show what's most useful: signal info during RX, timer and power during TX, progress during scan, step size and mode during idle.

*Scenario: You need to switch from narrowband FM on 2m to wideband AM on airband. F+0 cycles modulation (toast: "MOD: AM"). Two button presses for the critical change. Stock firmware: MENU, scroll, scroll, scroll, select, scroll, MENU, scroll, scroll, scroll, select.*

---

## What Makes This Different

Stock firmware is open-loop: you set fixed values and hope they work. VUURWERK is closed-loop: the radio measures what's actually happening (noise floor, signal strength, audio level, signal type) and adjusts itself in real time. It's the difference between driving with your eyes closed holding the wheel straight, and actually watching the road and steering.

**More features in fewer bytes.** VUURWERK v1.2.3 compiles to 59,164 bytes of flash with 2,276 bytes free (3.71% headroom). Every feature fits in the same 60KB that stock firmware uses for less.

**Original innovations.** VUURWERK introduces concepts that don't exist in any other UV-K5 firmware: intelligence-based squelch with voice probability scoring, voice-seeking spectrum analysis, FM adaptive gain staging, universal squelch tail elimination, real-time TX audio compression, and more. These aren't ports or adaptations of existing features. See [LICENSE](LICENSE) for the full original contributions list and [FEATURES.md](FEATURES.md) for the technical deep dive.

---

## Flash It In 60 Seconds

1. Download [`vuurwerk-v1.2.3.packed.bin`](release/vuurwerk-v1.2.3.packed.bin) from the `release/` folder in this repo
2. Turn off your radio
3. Hold PTT, then turn on the radio (flashlight LED confirms bootloader mode)
4. Connect your programming cable to the radio and your computer
5. Open the flasher in Chrome: https://egzumer.github.io/uvtools/
6. Click "Select firmware file" and choose `vuurwerk-v1.2.3.packed.bin`
7. Click "Flash firmware" and wait about 10 seconds
8. Disconnect the cable and power cycle the radio

You'll see the VUURWERK boot screen. You're done.

Chrome is required (Web Serial API doesn't work in Firefox or Safari). Use a standard Kenwood-style 2-pin programming cable (the same one used for Chirp or any other UV-K5 programming).

Your channels, settings, and frequencies are preserved. Only the firmware changes.

---

## Quick Reference

```
SHORTCUTS (F + number key)
F+0  Modulation    FM/AM/USB       F+5  Spectrum      Full analyzer
F+1  Band          Cycle bands     F+6  TX Power      LOW/MID/HIGH
F+2  VFO           A / B           F+7  Bandscope     ON / OFF
F+3  VFO/MR        Freq / Memory   F+8  Reverse       Offset toggle
F+4  Squelch       Level 0-9       F+9  Call Freq     Jump / Restore

ON EVERY PTT                       ALWAYS RUNNING (RX)
  TX Soft Start (60ms S-curve)       FM Gain Staging
  CTCSS Lead-In (150ms mic mute)     RSSI Smoothing + Histogram
  TX Compressor (dynamic gain)       Signal Classifier (F/N/S/~)
                                     Adaptive + Smart Squelch
ON SCREEN                            Squelch Tail Elimination
  Status bar: Q meter, battery       S-Meter Compensation
  Center: Bandscope / Toast
  Bottom: Context status line      MENU: 7 categories, number keys
  RX line: S-level dBm Q F/N/S/~  Long-press MENU: About screen
```

See [ACCESS_MAP.md](ACCESS_MAP.md) for the complete feature access guide.

---

## Built By

**Firmware design and architecture:** James Honiball (KC3TFZ)
**Implementation:** Built with [Claude Code](https://claude.ai) (Anthropic)
**Based on:** [Egzumer](https://github.com/egzumer/uv-k5-firmware-custom), with credits to [DualTachyon](https://github.com/DualTachyon), [OneOfEleven](https://github.com/OneOfEleven), [fagci](https://github.com/fagci)

See [LICENSE](LICENSE) for full attribution and the complete list of VUURWERK-original contributions.

---

## Documentation

- [FEATURES.md](FEATURES.md) -- Full technical documentation with algorithms and register maps
- [WHAT_VUURWERK_DOES.md](WHAT_VUURWERK_DOES.md) -- Plain English guide to every feature
- [ACCESS_MAP.md](ACCESS_MAP.md) -- Complete feature access guide and cheat sheet
- [CHANGELOG.md](CHANGELOG.md) -- Version history
- [DONATE.md](DONATE.md) -- Support the project
- [DEDICATION.md](DEDICATION.md) -- In memory of ZS6NE

---

*Dedicated to George Thomas Honiball, ZS6NE (Silent Key). He is a silent key now, but not in and through me.*

*VUURWERK (Afrikaans: fireworks) -- because your radio should light up.*
