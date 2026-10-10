# VUURWERK

**Custom firmware that turns a $30 Quansheng UV-K5 into a radio that listens,
adapts, and responds like equipment ten times its price.**

VUURWERK is Afrikaans for *fireworks*.

[![Latest release](https://img.shields.io/github/v/release/Tokeloshe/vuurwerk-firmware?label=release&color=blue)](https://github.com/Tokeloshe/vuurwerk-firmware/releases/latest)
[![Release date](https://img.shields.io/github/release-date/Tokeloshe/vuurwerk-firmware?label=released)](https://github.com/Tokeloshe/vuurwerk-firmware/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Tokeloshe/vuurwerk-firmware/total?label=downloads&color=success)](https://github.com/Tokeloshe/vuurwerk-firmware/releases)
[![Latest downloads](https://img.shields.io/github/downloads/Tokeloshe/vuurwerk-firmware/latest/total?label=latest%20downloads)](https://github.com/Tokeloshe/vuurwerk-firmware/releases/latest)
[![Stars](https://img.shields.io/github/stars/Tokeloshe/vuurwerk-firmware?style=flat)](https://github.com/Tokeloshe/vuurwerk-firmware/stargazers)
[![Forks](https://img.shields.io/github/forks/Tokeloshe/vuurwerk-firmware?style=flat)](https://github.com/Tokeloshe/vuurwerk-firmware/network/members)
[![Last commit](https://img.shields.io/github/last-commit/Tokeloshe/vuurwerk-firmware)](https://github.com/Tokeloshe/vuurwerk-firmware/commits/main)
[![Issues](https://img.shields.io/github/issues/Tokeloshe/vuurwerk-firmware)](https://github.com/Tokeloshe/vuurwerk-firmware/issues)
[![License](https://img.shields.io/github/license/Tokeloshe/vuurwerk-firmware)](LICENSE)
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20me%20a%20coffee-Tokeloshe-FFDD00?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/Tokeloshe)

---

## See It In Action

Power on a VUURWERK radio and, after the boot banner, this is your home screen:

```
+--------------------------------+
| [Q|||]           [F]  [BATT##] |  <- status bar: signal quality, F key,
|                                |     battery (live even during TX)
| A  146.520.00       FM   H  N  |  <- VFO A: frequency, modulation,
|                                |     TX power, narrow/wide
| .:|.||:..|,.:|||,.:.|..:||:.,  |  <- bandscope: live RF activity
|                                |     on your frequency (F+7)
| B  446.000.00       FM   M     |  <- VFO B
|                                |
| 12.5kHz N FM                   |  <- context line: step, bandwidth, mode
+--------------------------------+
```

Stock firmware shows two frequencies and a battery icon. VUURWERK adds a live
bandscope showing who is transmitting nearby, a signal-quality meter, and a
context-aware bottom line that changes with what the radio is doing. When a
signal comes in:

```
| RX S7 -65dBm Q:3 F             |  <- S-level, strength, quality, classifier
```

That trailing `F` is the signal classifier telling you this is a fast-rise FM
signal (`N` = SSB, `S` = carrier, `~` = noise). The radio worked that out by
measuring how quickly the signal appeared.

And this is the headline instrument -- the Voice-Seeking Spectrum (F+5, then
STAR until the corner reads `VOX`):

```
+--------------------------------+
| -130/-50              [BATT##] |  <- status bar: dB range, battery
| 146.552.50        FM     VOX   |  <- listening frequency + mode label
|  .    .   .       .    .       |  <- top ticks: every hop-eligible bin
|           #            |       |
|      +--------------+  |       |     full bar  = strong voice (75+)
|  _.__|    LOCKED    |__|_,____ |  <- LOCKED banner flashes on PTT
|      +--------------+          |
| ...:....|....:...^,:....|....: |  <- tick rail + peak arrow (^)
| 145.760        >80.0k  148.960 |  <- span start, step, span end
+--------------------------------+
```

Only bins that score as *human voice* draw bars. UP/DOWN hops between them.
PTT locks on. EXIT carries the frequency home to your VFO. No other UV-K5
firmware does this.

---

## What Changes When You Flash This

### You'll find voices, not just signals

**VOX Voice-Seeking Spectrum** scores every frequency bin for voice
probability instead of raw strength, so carriers and birdies stay flat while
conversations light up. The full hunt loop:

```
  SWEEP ----> voice found ----> LISTEN ----> UP/DOWN  seek next voice
    ^                             |            |
    |   voice gone (2 strikes)    |           PTT --> LOCKED (stay put)
    +-----------------------------+            |         |
                                               |       UP/DOWN releases
                                          EXIT +- LOCKED? -+
                                               |           |
                                          back on your     frequency CARRIES
                                          VFO, as it was   to your VFO
```

- **Seek**: UP walks to the next voice above the cursor, DOWN below. No voice
  that way? The keys fall through to normal band navigation, so they always
  move you. Hold a key and the radio patrols the band, auto-stopping on voice.
- **Lock**: tap PTT while listening -- a `LOCKED` banner flashes and the radio
  parks there through any pause in the conversation, squelch open.
- **Carry-over**: exit while locked (in frequency mode) and the locked
  frequency becomes your VFO frequency. You leave the spectrum already tuned,
  ready to answer.

**Deterministic spectrum exit.** Leaving the spectrum always lands you back on
your own VFO, on your own frequency, hearing normally -- lock or no lock, dual
watch or not.

**Adaptive scan** pre-checks each channel in ~30 ms and only dwells on
channels showing energy, so scanning an empty band runs 2-3x faster with
weak-signal sensitivity unchanged. Tone-coded memory channels get a longer
dwell so the decoder actually has time to confirm the tone.

**Bandscope** (F+7) keeps a live RF activity strip on the main screen, with
peak dots and a noise-floor line, without interrupting audio.

### You'll sound better to everyone listening

**TX Compressor** rides your mic gain in real time -- transparent at normal
speaking level, smoothly catching shouts and close talking.

**CTCSS Lead-In** mutes the mic for the first 150 ms of every transmission so
the repeater decodes your tone before your first syllable. No more clipped
words.

**TX Soft Start** ramps power on an S-curve over 60 ms, killing the key-up
click other radios hear.

### You'll get out of trouble with one gesture

**F+EXIT = reset to simplex.** A VFO that lights the green RX light but stays
silent almost always has a leftover setting: an RX tone waiting for a match, a
repeater offset, reverse, a scrambler. F+EXIT wipes the active VFO back to a
clean simplex state (tones cleared, offset cleared, reverse off, scrambler
off, FM) and shows a `SIMPLEX` toast. Memory channels are never touched.

```
   deaf VFO?    [F] + [EXIT]  -->  +---------+
   green light                     | SIMPLEX |   clean slate:
   but silent                      +---------+   RX = TX, FM, no tones
```

### You'll hear stations you couldn't before

**Smart squelch** scores every signal for voice probability using three
hardware measurements at once -- it opens for voices and stays shut for
pagers, glitches, and noise, and it can ease the threshold for a weak voice
before the squelch even opens.

**FM Gain Staging** adapts the receiver front end to signal strength in real
time, so strong signals come in clean and weak ones are not buried.

**Squelch Tail Elimination** mutes the "KSSHHH" at the end of the other
station's transmission -- for both CTCSS and DCS.

### You'll spend less time in menus

One-press **F-key shortcuts** for everything you change often, each confirmed
by a 1-second toast:

```
F+1  BAND     cycle bands        F+6  POWER     LOW / MID / HIGH
F+2  A/B      swap VFOs          F+7  BANDSCOPE on / off
F+3  VFO/MR   freq <-> memory    F+8  REVERSE   repeater reverse
F+4  SQUELCH  level 0-9          F+9  CALL FREQ jump to calling freq
F+5  SPECTRUM full analyzer      F+0  MODE      FM / AM / USB
                    F+EXIT  RESET TO SIMPLEX
```

A **categorized menu** (7 groups, number keys jump straight in) with a
plain-language hint line that explains cryptic items while you browse.

### It looks after itself

A **boot-time health probe** checks the transceiver chip and calibration
EEPROM at every power-on and reports faults in plain text. The **battery
indicator stays live during TX** and a sag tracker warns you when your pack is
tiring. The **flashlight auto-extinguishes** after 30 minutes so a pocket
press never drains your pack overnight. The **backlight fades out** instead of
cutting to black and stays lit while you are actually talking or listening.

### You can program it from your computer

**CHIRP support** via a community-contributed driver -- read and write
channels, side-keys, and settings without flashing back to stock. See
[chirp/README.md](chirp/) for the one-time setup. Driver contributed by
mjwgeek.

---

## What Makes This Different

Stock firmware is open-loop: you set fixed values and hope they work. VUURWERK
is closed-loop: the radio measures what is actually happening -- noise floor,
signal strength, audio level, signal type -- and adjusts itself in real time.
It's the difference between holding the wheel straight with your eyes closed
and actually watching the road.

**Original innovations.** Intelligence-based squelch with voice-probability
scoring, voice-seeking spectrum analysis with PTT lock and frequency
carry-over, FM adaptive gain staging, universal squelch-tail elimination,
real-time TX audio compression. These aren't ports of existing features. See
[LICENSE](LICENSE) for the original-contributions registry and
[FEATURES.md](FEATURES.md) for the technical deep dive.

---

## Flash It In 60 Seconds

```
 [download] --> [radio off] --> [hold PTT + power on] --> [LED lit?]
                                                              |
 [power cycle] <-- [flash ~10s] <-- [pick .packed.bin] <-- [cable in,
      |                              in Chrome flasher      Chrome open]
   VUURWERK
   boot screen
```

1. Download [`vuurwerk-v1.5.0.packed.bin`](release/vuurwerk-v1.5.0.packed.bin)
   from the `release/` folder
2. Turn the radio off
3. Hold PTT and turn it on -- the flashlight LED confirms bootloader mode
4. Connect your programming cable (standard Kenwood-style 2-pin)
5. Open https://egzumer.github.io/uvtools/ in Chrome
6. "Select firmware file" -> choose the `.packed.bin` -> "Flash firmware"
7. Wait about 10 seconds, disconnect, power cycle

Chrome is required (the Web Serial API doesn't work in Firefox or Safari).
Your channels and settings are preserved -- only the firmware changes. Full
details and troubleshooting: [FLASHING.md](FLASHING.md).

---

## Hardware Compatibility -- UV-K5 V1 Only

**VUURWERK only supports the original Quansheng UV-K5 V1 (DP32G030
processor).** The newer V2 (PY32F030) and V3 (PY32F071) boards are **not
compatible**; flashing them produces a radio that does not boot (recoverable
with the right stock firmware, but avoid the trip).

**How to check:** remove the battery and read the board label. V2 and V3
boards are marked; no marking means V1 -- the one VUURWERK supports.

**If you flashed the wrong hardware**, recover with
[K5TOOL](https://github.com/qrp73/K5TOOL), which supports all UV-K5 versions.

---

## Built By

**Firmware design and architecture:** James Honiball (KC3TFZ)
**Based on:** [Egzumer](https://github.com/egzumer/uv-k5-firmware-custom),
with credits to [DualTachyon](https://github.com/DualTachyon),
[OneOfEleven](https://github.com/OneOfEleven), [fagci](https://github.com/fagci)

See [LICENSE](LICENSE) for full attribution and the complete list of
VUURWERK-original contributions.

---

## Support VUURWERK

VUURWERK is free and built nights and weekends by one operator on a $30
radio. If the spectrum found you a conversation, or F+EXIT rescued a deaf VFO,
you can keep the bench powered and keep going on this:

<p align="center">
  <a href="https://buymeacoffee.com/Tokeloshe">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" height="60">
  </a>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/Tokeloshe">
    <img src="buymeacoffee/qr-code.png" alt="Buy Me a Coffee QR code" width="160">
  </a>
  <br>
  <em>Reading this on a desktop? Scan with your phone.</em>
</p>

Prefer crypto, or want to help without spending a cent? See
[DONATE.md](DONATE.md) for XRP and the free ways to support the project.

---

## Project Pulse

Live numbers, straight from GitHub. Every badge above is a link.

<p align="center">
  <a href="https://star-history.com/#Tokeloshe/vuurwerk-firmware&Date">
    <img src="https://api.star-history.com/svg?repos=Tokeloshe/vuurwerk-firmware&type=Date" alt="Star history" width="600">
  </a>
</p>

| Release | Downloads |
|---|---|
| [v1.5.0](https://github.com/Tokeloshe/vuurwerk-firmware/releases/tag/v1.5.0) -- The Stabilization Release | ![](https://img.shields.io/github/downloads/Tokeloshe/vuurwerk-firmware/v1.5.0/total?label=) |
| [v1.2.7](https://github.com/Tokeloshe/vuurwerk-firmware/releases/tag/v1.2.7) | ![](https://img.shields.io/github/downloads/Tokeloshe/vuurwerk-firmware/v1.2.7/total?label=) |
| [v1.2.4](https://github.com/Tokeloshe/vuurwerk-firmware/releases/tag/v1.2.4) | ![](https://img.shields.io/github/downloads/Tokeloshe/vuurwerk-firmware/v1.2.4/total?label=) |

Repository owners also get views, unique visitors, clones and referring sites
under **Insights -> Traffic**.

---

## Documentation

- [WHATS_NEW.md](WHATS_NEW.md) -- What's new in v1.5.0, with pictures
- [USER_MANUAL.md](USER_MANUAL.md) -- Button-by-button operator manual with
  screen mockups for every feature
- [FEATURES.md](FEATURES.md) -- Full technical documentation
- [WHAT_VUURWERK_DOES.md](WHAT_VUURWERK_DOES.md) -- Plain-English guide to
  every feature
- [ACCESS_MAP.md](ACCESS_MAP.md) -- Feature access cheat sheet
- [FLASHING.md](FLASHING.md) -- Flashing guide with troubleshooting
- [BUILD.md](BUILD.md) -- Building from source
- [chirp/](chirp/) -- CHIRP driver (community-contributed by mjwgeek)
- [CHANGELOG.md](CHANGELOG.md) -- Version history
- [DONATE.md](DONATE.md) -- Support the project
- [DEDICATION.md](DEDICATION.md) -- In memory of ZS6NE

---

## License

VUURWERK is dual-licensed.

**Before you borrow code:** the VUURWERK-original features listed in
[LICENSE](./LICENSE) are licensed as a package with the complete firmware.
Taking an individual feature, algorithm, or module into another project or
product requires written permission (a commercial license) from the author.
Read the notice at the top of [LICENSE](./LICENSE) first.

**The complete VUURWERK firmware** is distributed under the GNU General Public
License v3. The upstream Egzumer / DualTachyon code it builds on is licensed
under the Apache License 2.0, which is compatible with GPL v3; upstream files
keep their Apache-2.0 notices. You may flash it, fork it, modify it, and
redistribute the complete firmware under GPL v3 terms.

**VUURWERK-original algorithms and modules** (listed in the
[LICENSE](./LICENSE) contributions registry) are additionally protected by a
commercial-license requirement for any use outside the complete VUURWERK
firmware. Extracting a VUURWERK-original file, porting a VUURWERK-original
algorithm to another platform, or incorporating VUURWERK-original code into a
separate project requires a commercial license obtained in writing from James
Honiball (KC3TFZ).

Commercial licensing inquiries: jhoniball4@gmail.com

Stock upstream code (Egzumer, DualTachyon, OneOfEleven, fagci) remains under
its original license terms as marked in each file.

---

*Dedicated to George Thomas Honiball, ZS6NE (Silent Key). He is a silent key
now, but not in and through me.*

*VUURWERK (Afrikaans: fireworks) -- because your radio should light up.*
