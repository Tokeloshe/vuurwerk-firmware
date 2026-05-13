# VUURWERK

**Custom firmware that turns a $30 Quansheng UV-K5 into a radio that listens, adapts, and responds like equipment ten times its price.**

VUURWERK is Afrikaans for *fireworks*.

**v1.2.7** | 60,772 bytes | 41 active features | Zero warnings | GPL v3

---

## License

VUURWERK is dual-licensed.

**The complete VUURWERK firmware** is distributed under the GNU General Public License v3, consistent with its upstream Egzumer / DualTachyon parent. You may flash it, fork it, modify it, and redistribute the complete firmware under GPL v3 terms.

**VUURWERK-original algorithms and modules** (listed in the [LICENSE](./LICENSE) contributions registry) are additionally protected by a commercial-license requirement for any use outside the complete VUURWERK firmware. Extracting a VUURWERK-original file, porting a VUURWERK-original algorithm to another platform, or incorporating VUURWERK-original code into a separate project requires a commercial license obtained in writing from James Honiball (KC3TFZ).

Commercial licensing inquiries: jhoniball4@gmail.com

Stock upstream code (Egzumer, DualTachyon, OneOfEleven, fagci) remains under its original license terms as marked in each file.

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

**Toast Notifications** show a 1-second overlay confirming every shortcut action ("PWR: HIGH", "SQL: 5", "SCOPE ON"). Side-button presses (SIDE1/SIDE2 and long-press MENU) get the same 1-second confirmation, so every shortcut path looks and feels identical regardless of which button you pressed.

**Context-Aware Status Line** at the bottom of the screen changes automatically to show what's most useful: signal info during RX, timer and power during TX, scan progress with live channels-per-second readout, step size and mode during idle.

*Scenario: You need to switch from narrowband FM on 2m to wideband AM on airband. F+0 cycles modulation (toast: "MOD: AM"). Two button presses for the critical change. Stock firmware: MENU, scroll, scroll, scroll, select, scroll, MENU, scroll, scroll, scroll, select.*

### It looks after itself

**Boot-Time Health Probe** checks the BK4819 transceiver and the calibration EEPROM the moment the radio powers on. A wedged SPI bus or a blanked-out calibration page surfaces as a clear fault banner on the welcome screen instead of mis-rendered S-meter values and silent misbehaviour. No other UV-K5 firmware boots with a self-test.

**Battery-aware behaviours** keep the radio honest about its own power state. The battery indicator updates live during transmit (stock firmware freezes it on PTT). A per-transmission voltage sag tracker latches the worst dip and shows it in the About screen. The flashlight auto-extinguishes after 30 minutes (10 minutes on a low-battery pack) so a forgotten LED never drains your pack overnight. SOS is preserved indefinitely.

**Backlight behaviour** fades out over 2 seconds at the end of its idle window instead of cutting to black, and refreshes itself any time you're actively transmitting or receiving so the screen never goes dark mid-conversation. The PWM carrier is moved out of the audio passband too, killing the 1 kHz whine that stock firmware bleeds into outbound audio when the screen is dimmed during TX.

**Audible feedback cues** confirm what the radio just did. The VOX-hop spectrum mode plays a short ascending two-tone chord when it finds voice on a new bin and a soft double-beep when it sweeps the whole range without finding any. CSS scan emits a 1 kHz beep when it locks onto a tone and a different cue if it times out without finding one. CSS scans also get a status-bar glyph and a 60-second soft watchdog that prevents the unbounded-scan trap inherent to FM-with-no-tones.

---

## What Makes This Different

Stock firmware is open-loop: you set fixed values and hope they work. VUURWERK is closed-loop: the radio measures what's actually happening (noise floor, signal strength, audio level, signal type) and adjusts itself in real time. It's the difference between driving with your eyes closed holding the wheel straight, and actually watching the road and steering.

**More features in fewer bytes.** VUURWERK v1.2.7 compiles to 60,772 bytes of flash with 668 bytes free (1.09% headroom). Every feature fits in the same 60KB that stock firmware uses for less.

**Original innovations.** VUURWERK introduces concepts that don't exist in any other UV-K5 firmware: intelligence-based squelch with voice probability scoring, voice-seeking spectrum analysis, FM adaptive gain staging, universal squelch tail elimination, real-time TX audio compression, and more. These aren't ports or adaptations of existing features. See [LICENSE](LICENSE) for the full original contributions list and [FEATURES.md](FEATURES.md) for the technical deep dive.

---

## Flash It In 60 Seconds

1. Download [`vuurwerk-v1.2.7.packed.bin`](release/vuurwerk-v1.2.7.packed.bin) from the `release/` folder in this repo
2. Turn off your radio
3. Hold PTT, then turn on the radio (flashlight LED confirms bootloader mode)
4. Connect your programming cable to the radio and your computer
5. Open the flasher in Chrome: https://egzumer.github.io/uvtools/
6. Click "Select firmware file" and choose `vuurwerk-v1.2.7.packed.bin`
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
  RX line: S-level dBm Q F/N/S/~  About: Menu > CONFIG > About
```

See [ACCESS_MAP.md](ACCESS_MAP.md) for the complete feature access guide.

---

### **Hardware Compatibility -- UV-K5 V1 Only**

**VUURWERK only supports the original Quansheng UV-K5 V1 hardware, which uses the DP32G030 processor.**

The UV-K5 has been revised multiple times. The newer V2 (PY32F030 processor) and V3 (PY32F071 processor) boards are **not compatible** with this firmware. Flashing VUURWERK onto V2 or V3 hardware will result in a radio that does not boot. This will not permanently damage the radio, but you will need recovery firmware for your specific hardware version to restore it.

**How to check which version you have:** Remove the battery and look at the circuit board label underneath. V2 and V3 boards are marked with their revision. If there is no marking, you have a V1 -- that is the one VUURWERK supports.

**If you flashed VUURWERK onto incompatible hardware,** use [K5TOOL](https://github.com/qrp73/K5TOOL) to recover your radio. It supports all UV-K5 hardware versions.

---

## Built By

**Firmware design and architecture:** James Honiball (KC3TFZ)
**Implementation:** Built with [Claude Code](https://claude.ai) (Anthropic)
**Based on:** [Egzumer](https://github.com/egzumer/uv-k5-firmware-custom), with credits to [DualTachyon](https://github.com/DualTachyon), [OneOfEleven](https://github.com/OneOfEleven), [fagci](https://github.com/fagci)

See [LICENSE](LICENSE) for full attribution and the complete list of VUURWERK-original contributions.

---

## Documentation

- [USER_MANUAL.md](USER_MANUAL.md) -- Button-by-button operator manual with ASCII LCD mockups for every user-operable feature
- [FEATURES.md](FEATURES.md) -- Full technical documentation with algorithms and register maps
- [WHAT_VUURWERK_DOES.md](WHAT_VUURWERK_DOES.md) -- Plain English guide to every feature
- [ACCESS_MAP.md](ACCESS_MAP.md) -- Complete feature access guide and cheat sheet
- [CHANGELOG.md](CHANGELOG.md) -- Version history
- [DONATE.md](DONATE.md) -- Support the project
- [DEDICATION.md](DEDICATION.md) -- In memory of ZS6NE

---

*Dedicated to George Thomas Honiball, ZS6NE (Silent Key). He is a silent key now, but not in and through me.*

*VUURWERK (Afrikaans: fireworks) -- because your radio should light up.*
