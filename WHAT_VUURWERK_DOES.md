# What VUURWERK Actually Does

**v1.2.7: Plain English Guide to Every Feature**

VUURWERK is custom firmware for the Quansheng UV-K5 radio. It keeps everything the stock radio does, then adds 41 features on top, all fitting in the same 60KB of flash memory. Here's what each one does and why it matters.

## Changes since v1.2.6

v1.2.7 fixed the SIDE2 long-press binding that operators found wrong in practice. SIDE2 is now symmetric with SIDE1: short press is one freq-down step, hold is continuous auto-repeat. Backlight toggle is no longer accessible from spectrum view (use EXIT + main-screen gesture + F+5 to re-enter).

v1.2.7 also rebuilt the CSS scanner for reliability: a pre-flight RSSI gate aborts with `NO SIGNAL` on dead air, CTCSS now locks in one confirmation (matching DCS), dwell time dropped to 120 ms, and the soft-timeout tightened to 30 seconds.

See the "v1.2.7 Changes" and "v1.2.6 Changes" subsections at the bottom of this document, plus Sections 6 and 7 of USER_MANUAL.md for the full operator-facing details.

## Changes since v1.2.5

v1.2.6 reorganized the spectrum analyzer side-button keymap (Blacklist moved from SIDE1 to MENU; SIDE1 became freq UP; the v1.2.6 SIDE2 freq DOWN / long-press-backlight binding was later reverted in v1.2.7 to plain freq DOWN with auto-repeat).

## Changes since v1.2.4

Activity Log retired in v1.2.5 (Feature #27 slot intentionally empty). The 128x64 LCD couldn't show enough log context to be useful in HT form factor, and the operator's hands are typically occupied with the radio while a log entry would be most relevant. Numbering jumps from #26 to #28 below.

---

## Transmit (TX) Improvements

### 1. TX Soft Start
**What it does:** When you press PTT, the radio's power amplifier used to slam on instantly. That creates a "click" or "pop" that other operators hear. VUURWERK ramps the power up smoothly over 60 milliseconds using an S-shaped curve, the same shape used in professional broadcast equipment. The result is a clean, click-free key-up.

### 2. CTCSS Lead-In
**What it does:** Many repeaters require a sub-audible tone (CTCSS) to open. The problem is that stock radios start sending your voice at the same instant as the tone. The repeater hasn't decoded the tone yet, so it clips your first word. VUURWERK mutes your microphone for 150ms after PTT, letting the tone reach the repeater first. Then your voice comes through clean.

### 3. TX Audio Compressor
**What it does:** Makes your transmitted audio louder and more consistent. If you speak quietly, it boosts you. If you shout, it holds you back. It measures your mic level 100 times per second, computes the average loudness, and adjusts the microphone gain in real time. The attack is fast (5ms, catches sudden loud sounds) and the release is slow (300ms, smooth fade back up). The result: you sound punchy and clear, like a commercial two-way radio.

---

## Receive (RX) Signal Processing

### 4. FM Gain Staging
**What it does:** The stock radio has fixed receive gain: it doesn't adapt to signal strength. Strong signals overdrive the front end and sound distorted. Weak signals get buried. VUURWERK watches the signal strength and automatically adjusts the radio's internal amplifier chain (LNA, mixer, PGA) to keep the audio clean at any signal level. It reacts fast when a strong signal appears (to prevent overload) and releases slowly (to avoid pumping). When you change frequency, it resets to be ready for whatever's there.

### 5. RSSI Smoothing Filter
**What it does:** The raw signal strength reading from the radio chip bounces around rapidly, making the S-meter jittery and hard to read. VUURWERK applies an exponential moving average: each new reading is blended with the previous ones, producing a smooth, stable S-meter that still responds quickly to real signal changes. Think of it like shock absorbers for your signal meter.

### 6. RSSI Histogram
**What it does:** Quietly builds a statistical picture of the signal environment on your frequency. It sorts hundreds of signal strength readings into bins and finds the "noise floor," the level where most readings cluster. This tells the smart squelch system exactly where the noise ends and real signals begin, without you having to set anything.

### 7. Signal Classifier
**What it does:** Identifies what kind of signal you're receiving by measuring how fast it appears. FM voice signals pop up almost instantly (under 50ms). SSB and AM voice ramp up over 50-200ms. Carriers and CW are slow risers (over 200ms). Random noise never stabilizes. The classifier labels each signal as F (fast/FM), N (normal/SSB), S (slow/carrier), or ~ (noise) and displays the symbol on the status line during RX. This classification also feeds into the gain staging system so it can adjust hold times appropriately for each signal type.

### 8. Signal Quality Meter
**What it does:** Measures how stable a signal is over time. A solid, nearby signal has very little variation in strength. A signal bouncing off buildings or mountains fluctuates constantly. VUURWERK tracks this variance and rates the signal as Excellent, Good, Fair, or Poor, giving you a quick read on propagation conditions without needing test equipment.

### 9. S-Meter Compensation
**What it does:** When the gain staging system adjusts the radio's internal amplifiers, the raw signal strength reading shifts. Without correction, your S-meter would show the wrong value after a gain change. VUURWERK knows exactly how much gain it added or removed and compensates the S-meter reading so it always reflects the true signal strength.

### 10. AM Fix
**What it does:** Inherited from the OneOfEleven project. Prevents the BK4819 chip's AM demodulator from saturating on strong signals, which would cause severe distortion. VUURWERK shares the gain lookup table between this module and FM gain staging to save memory.

---

## Squelch

### 11. Squelch Tail Elimination
**What it does:** When someone stops transmitting on a repeater, there's usually a brief burst of noise ("KSSHHH") before the squelch closes. This is the "squelch tail" and it's annoying. VUURWERK watches the radio chip's CTCSS tone detector. The instant the tone disappears, it mutes the audio, before the noise burst reaches the speaker. After 150ms of silence, it unmutes. This works with all three industry methods (Motorola, Kenwood, and Chinese radios) because they all cause the tone to disappear. You can turn this on or off in the STE menu.

### 12. Intelligence-Based Squelch
**What it does:** The stock squelch uses a single fixed threshold: if the signal is above it, the squelch opens. VUURWERK reads three different measurements from the radio chip simultaneously: signal strength, audio noise level, and a glitch/transient counter. It blends these together to compute a "voice probability" score from 0 to 100. When it's confident there's a real voice signal, it loosens the squelch to let it through. When it detects noise or interference, it tightens the squelch. The result: fewer false opens on noisy bands, and real signals get through more easily. This runs all the time in the background.

### 13. Adaptive Squelch State
**What it does:** A supporting data module that tracks the noise floor and squelch open count. This information is consumed by the intelligence-based squelch to make better decisions about what's noise and what's signal.

---

## Scanning and Monitoring

### 14. Scan+Watch
**What it does:** Scans through frequencies on one VFO while periodically checking the other VFO for activity, like having two radios in one. Every four scan steps, it briefly hops to the watch VFO, listens for 100ms, and if it hears something, it stays there for 2 seconds. If the signal persists, it keeps listening. If it drops off, scanning resumes. This is a feature normally found only on radios costing $300+.

### 15. Intelligent Dual-Watch
**What it does:** Stock dual-watch alternates between two VFOs at a fixed rate. VUURWERK makes it smarter: it tracks activity on each VFO and adjusts the dwell times proportionally. A busier VFO gets shorter dwell time (you'll catch it anyway), while the quieter one gets more listening time (so you don't miss brief transmissions). Default dwell is 500ms, ranging from 200ms to 2000ms. Activity counters decay over time so the system adapts to changing conditions. This runs automatically whenever dual-watch is enabled.

### 16. VFO Split
**What it does:** Allows independent transmit and receive frequencies on separate VFOs. The radio periodically hops to VFO B to check for activity, then returns to VFO A. If it detects a signal on B above -100 dBm, it alerts you. Supports scanning memory channels, frequency ranges, or scan lists on the B side. Enable it from the menu: go to the Scan category, select ScnWch (Scan+Watch), and choose SPLIT. The 5-state hop machine (IDLE, HOP_TO_B, SETTLE, READ_RSSI, RETURN_TO_A) runs every 300ms by default without interrupting your VFO A operation.

### 17. Bandscope
**What it does:** Shows a miniature spectrum display on the main screen: a scrolling timeline of signal strength on your current frequency. New readings scroll in from the right. Peak dots show the strongest signal seen at each position, decaying over time. A dotted line marks the noise floor. Toggle with F+7. It's like a heartbeat monitor for your frequency.

---

## Spectrum Analyzer (4 Modes)

The spectrum analyzer scans a range of frequencies and shows what's active. Press the Star key to cycle through four display modes.

**Side-button keymap (v1.2.7).** Inside the spectrum view, SIDE1 short / hold = step frequency UP (auto-repeats while held), SIDE2 short / hold = step frequency DOWN (auto-repeats while held, symmetric with SIDE1), MENU short = blacklist current peak bin. UP / DOWN keep their mode-aware behaviour: frequency step in NORM / PEAK / MTI, voice-hop in VOX. ToggleBacklight is no longer accessible from spectrum view -- press EXIT, toggle the backlight from the main screen, then F+5 to re-enter spectrum.

Press the Star key to cycle through four display modes:

### 18. NORM: Normal Spectrum
**What it does:** Standard spectrum display. Each frequency bin shows a bar proportional to signal strength. Simple, clean, effective.

### 19. PEAK: Peak Hold
**What it does:** Same as NORM, but adds dots above the bars showing the strongest signal seen at each frequency. The peaks slowly decay over time. Useful for catching brief transmissions you might otherwise miss.

### 20. MTI: Moving Target Indicator
**What it does:** Borrowed from radar technology. Shows only what has *changed* since the last sweep. If a signal is steady (like a carrier or birdie), it disappears. If something new appears or changes strength, it lights up. Perfect for finding intermittent signals in a crowded band.

### 21. VOX: Voice-Seeking Spectrum
**What it does:** The headline feature of VUURWERK v1.2.0. Instead of showing all signals equally, it identifies which ones are likely carrying voice. For each frequency bin, it reads the radio chip's noise indicator alongside the signal strength. Clean audio with a strong signal scores high. Noisy, weak, or interference-heavy bins score low.

The display shows confidence (v1.2.6): bins below the hop threshold of 50 show just a floor dot. Bins with voice probability 50 to 74 show half-height bars. Bins with voice probability 75 or higher show full-height bars. Every visible bar gets a 1-pixel marker tick at the very top of the spectrum display so the hop-eligible bins form a clear row of dots independent of bar height. Press UP or DOWN to hop directly to the next hop-eligible bin (skipping any you have blacklisted with MENU); the radio tunes there and lets you listen, then resumes scanning when the signal drops.

---

## User Interface

### 22. RSSI Bar
**What it does:** A visual signal strength meter on the main display, compensated for gain staging adjustments so it always reads accurately in FM mode.

### 23. Context-Aware Status Line
**What it does:** The bottom line of the display changes based on what the radio is doing. Idle shows the firmware name. RX shows signal info. TX shows power. Scan shows progress. It's always relevant information without cluttering the screen.

### 24. Boot Screen
**What it does:** Shows the VUURWERK name and version when the radio powers on.

### 25. About Screen
**What it does:** Navigate to Menu > CONFIG > About to see firmware info and feature highlights.

### 26. Categorized Menu
**What it does:** The stock radio has 60+ menu items in one flat list. VUURWERK organizes them into 7 categories: Receive, Tone, Transmit, Scan, Channel, Config, and a hidden Unlock category. You scroll through categories first, then items within. Finding settings takes seconds instead of minutes.

---

## v1.2.5+ Additions

### 28. Side-Button Toast Feedback
**What it does:** The 1-second toast overlay that confirms F+key shortcuts now also fires for SIDE1, SIDE2, and long-press MENU presses. Whichever shortcut path you used, the radio answers the same way. Closes the UX-parity gap where F+key actions had visible confirmation but side-button actions executed silently.

### 29. Toast Notification Subsystem
**What it does:** The toast popup engine extracted into its own module. Same 1-second centered overlay, same 10ms tick decrement, now reusable from any caller. Powers the F+key shortcut feedback, the side-button feedback (Feature #28), the F-hold lock toast, and the scan-state toasts.

### 30. Scan Rate Telemetry
**What it does:** While scanning, the status line now shows the live channels-per-second sweep rate. A rolling 100-tick counter measures how fast the scanner is actually advancing through channels and the renderer appends "NNc/s" to the SCANNING text. Lets you see at a glance whether scanning is healthy or whether something has slowed it down.

### 31. F-Key UX Hardening
**What it does:** Three small reliability fixes wrapped around the F-key dispatch path. Long-pressing F (the keypad lock gesture) now toasts "LOCKED" or "UNLOCKED" so you can tell whether the lock actually toggled. F+3 (VFO/MR toggle) toasts "NO CHANNELS" when memory mode is empty instead of the misleading "VFO MODE" stock behavior. F+STAR (CSS scanner) is now gated to FM mode only and toasts "FM ONLY" if you try it in AM or USB, because the BK4819 CTCSS detector only populates in FM and the scan would otherwise loop forever. STAR long-hold (scan toggle) gets its own "SCAN ON" / "SCAN OFF" toast.

### 32. Flashlight Auto-Off Watchdog
**What it does:** A forgotten flashlight in a pocket will drain a UV-K5 pack overnight on every stock fork. VUURWERK ticks a watchdog every 10ms in software (no hardware timer added) and extinguishes the flashlight after 30 minutes of unattended ON or BLINK. SOS mode is preserved indefinitely because that's emergency signaling. On a low-battery pack (battery indicator level 2 or lower) the timeout shortens to 10 minutes so a half-drained pack doesn't get fully drained by a forgotten LED. Mode changes reset the timer so tapping the flashlight key extends the session. Standard watchdog technique, original implementation for the UV-K5 platform.

### 33. CSS Scan Soft-Timeout Watchdog
**What it does:** Companion to Feature #31's F+* gate. If a CSS scan is launched and runs for 60 seconds without finding a tone (the FM-with-no-tones-present trap that even the gate cannot fully prevent), this watchdog flips the scanner state to FAILED and queues an audible double-beep. The stock scanner state machine handles BK4819 teardown via its existing default branch. Closes the unbounded-scan trap that egzumer's binary `ENABLE_NO_CODE_SCAN_TIMEOUT` flag turns into either 16 seconds or forever.

### 34. Audio Palette: VOX-Hop "Found Voice" Two-Tone Chord
**What it does:** In VOX spectrum mode (Feature #21), when the voice-hop search finds a bin with high voice probability, the radio plays a short ascending two-tone chord (800 Hz then 1200 Hz) so you hear that a hop happened before the audio from the new bin even starts. Companion to the existing rejection idiom: on a sweep that finishes without finding voice, the radio plays a soft 500 Hz double-beep instead, so silence never leaves you wondering whether the keypress registered.

### 35. Boot-Time Hardware Health Probe
**What it does:** When the radio powers on, before the welcome screen renders, it runs a two-part health check. First it reads BK4819 register 0x00 after init and looks for the wedged-SPI 0xFFFF signature, which means the transceiver chip is not responding. Then it reads the factory battery-calibration page in EEPROM and looks for an all-0xFF byte pattern, which means the calibration is blank or the I2C bus is wedged. Either fault surfaces as a specific welcome banner: "BK4819 FAULT / RX/TX disabled" or "EEPROM FAULT / calib lost".

### 36. Live Battery Voltage During TX
**What it does:** Stock firmware freezes the battery indicator the instant you press PTT and only updates it when you release. VUURWERK adds a 500ms tick during transmit that advances the existing battery-voltage ring buffer and recomputes the on-screen battery percent and icon. You see voltage sag in real time as you transmit. Closes a long-standing gap in the stock firmware.

### 37. Backlight Fade-Out Tail
**What it does:** When the backlight idle timer expires, stock firmware cuts the backlight to black instantly. VUURWERK ramps it down over 2 seconds via the existing PWM brightness API so the screen tapers off smoothly. Original implementation of a standard backlight-fade UX pattern, adapted to the BK4819 platform's PWM constraints.

### 38. Backlight TX/RX Activity Refresh
**What it does:** Companion to Feature #37. If you have the per-mode "backlight on TX" or "backlight on RX" setting enabled, VUURWERK re-arms the backlight countdown every 500ms while you are actually transmitting or receiving, so the screen never goes dark mid-conversation. The stock firmware's one-shot backlight-on at TX/RX entry would still time out mid-call; this extends that to a hold-for-duration. Operators historically worked around this by setting backlight-always-on at the cost of battery life. VUURWERK now keeps it lit only when needed.

### 39. TX Battery Sag Delta Tracker
**What it does:** Each time you transmit, the radio records the battery voltage at PTT press, watches for the minimum during transmit, and at PTT release computes the sag delta in 10mV units. The About screen shows the most recent delta as "TX sag NNNNmV". Lets you see whether your pack is healthy (small sag) or worn out (large sag) without test equipment. Per-TX battery accounting, original to VUURWERK.

### 40. CSS Scan Status-Bar Glyph
**What it does:** A small "Cs" glyph in the status bar tells you a CSS scan is running, distinct from the existing channel-scan and SCAN+WATCH glyphs. Three concurrent scan types are now visually distinguishable at a glance.

### 41. CSS Scan FOUND Beep
**What it does:** When the CSS scanner locks onto a tone, the radio emits a single 1 kHz beep on the FOUND state edge. Symmetric with Feature #33's failure cue. Both the F+* path (from the main screen) and the menu path (R_CTCS / R_DCS submenus) trigger the same beep. A one-byte BSS latch prevents repeated beeping while the FOUND state persists.

### 42. Quiet Backlight PWM
**What it does:** Stock backlight PWM runs at about 1 kHz, which sits right in the middle of the audio passband and bleeds an audible whine into outbound TX audio whenever the screen is dimmed during transmit. VUURWERK reprograms the PWM prescaler at boot to push the carrier up to about 6.7 kHz, well above the 3 kHz audio passband. The whine drops below noise floor. kamilsss655's NUNU fork demonstrated this technique in 2024 via stock-driver source modification; VUURWERK ports the same fix as a LAW-1-safe one-line hook in main.c so the driver stays byte-identical with the egzumer parent.

---

## v1.2.7 Changes

### SIDE2 keymap symmetry fix
**What it does:** v1.2.6 bound SIDE2 short-press to freq-down with a 640 ms long-press for backlight toggle, on the theory of "clean short/long separation." Operators found this wrong: SIDE1 hold gives continuous freq-up auto-repeat, but holding SIDE2 toggled the backlight instead. v1.2.7 makes SIDE2 symmetric with SIDE1: short press is one freq-down step, hold is continuous freq-down auto-repeat. Backlight toggle is no longer reachable from inside spectrum view; use EXIT, main-screen toggle, F+5 to re-enter. The keymap state-machine code (release-dispatch + hold counter + dual-binding latch) is gone, simplifying the input loop.

### CSS scanner robustness
**What it does:** the CSS scanner was unreliable in v1.2.5 / v1.2.6 -- operators reported it rarely locked onto tones. v1.2.7 rebuilt it with four changes:
- Pre-flight RSSI gate: before launching the BK4819 scan engine, the scanner samples RSSI on the current frequency. Below carrier-present threshold (~-110 dBm) the scan aborts with a `NO SIGNAL` toast and 500 Hz double-beep. Park on an active carrier before scanning.
- CTCSS now locks on first confirmation (was 2). The asymmetry with DCS (which already locked on first confirmation) made CTCSS feel broken; with the pre-flight gate plus the BK4819 detector's internal voting, single-match is reliable.
- Dwell time reduced from 210 ms to 120 ms. The BK4819 detector converges in ~80-120 ms for the worst-case low CTCSS tones; 210 ms was over-conservative.
- Soft-timeout tightened from 60 s to 30 s. Typical successful scans now complete in 3-10 s; 30 s is the new "fruitless" cap.

### Economic re-engineering
**What it does:** to offset the SIDE2 state-machine removal and CSS work cost, v1.2.7 reclaimed bytes from VUURWERK feature modules. Five dead-code / redundant-init / API-tightening passes: TX_COMPRESSOR_GetGainReduction removed (zero callers, dead code), RSSI_HISTOGRAM_Init removed (BSS zero-init covers it), GAIN_STAGING_Reset made static (zero external callers, LTO inlines), bandscope.c parallel memmoves consolidated, SIGNAL_CLASSIFIER_Init removed (BSS zero-init covers it). Plus one CSS-scanner-scoped consolidation: the CDCSS and CTCSS arms in SCANNER_TimeSlice10ms became structurally identical after the 1-confirmation change and were merged. Net flash freed ~200 bytes vs v1.2.6.

### License header enforcement sweep
**What it does:** every VUURWERK-original module now carries the canonical dual-license header with the commercial-licensing contact line. The LICENSE Dual-Licensing Notice section now explicitly enumerates what "outside the complete VUURWERK firmware" means (extraction, porting, repackaging, commercial use). README gained a License section near the top with operator-facing summary. The contributions registry in LICENSE is unchanged (LAW 7). Zero flash impact -- comments only.

---

## v1.2.6 Changes

### Spectrum view keymap reorganization
**What it does:** Inside the spectrum analyzer, SIDE1 now steps frequency UP (with auto-repeat when held), SIDE2 now steps frequency DOWN (release-dispatched, no auto-repeat), long-press SIDE2 (about 640 ms) toggles the backlight, and short-press MENU blacklists the current peak bin. UP / DOWN still voice-hop in VOX and frequency-step in NORM / PEAK / MTI, exactly as before. Operators upgrading from v1.2.5 will need to relearn that SIDE1 and SIDE2 are now the universal frequency-step controls (was: SIDE1=blacklist, SIDE2=backlight). Short-press and long-press are unambiguously separated on SIDE2: the freq-down on release is suppressed when the long-press fires, so you never get a stray step when you wanted to toggle the backlight.

### Voice-hop blacklist respect
**What it does:** With Blacklist now moving to MENU (more accessible than the old SIDE1 binding), the VOX UP / DOWN voice-hop loop also now skips bins you have blacklisted. Previously a blacklisted bin with strong voice probability would still be tuned by the hop.

### Spectrum mode-change flash
**What it does:** When you press STAR to cycle modes (NORM / PEAK / MTI / VOX), a brief `MODE <name>` overlay appears in the middle of the spectrum display for about half a second, in addition to the permanent small label in the top-right corner. Operators who report only ever seeing "NORM" get a clear confirmation that the mode actually changed.

### VOX bar threshold matches hop threshold
**What it does:** In VOX mode, bins with voice probability below 50 now drop to a floor dot (previously 35) and only bins at or above 50 draw any bar. Every visible bar is now a valid hop target. A 1-pixel marker tick at the very top of the spectrum display flags every hop-eligible bin, so the row of dots across the top tells you exactly where UP / DOWN can land.

---

## The Architecture

Every one of these features follows one rule: **the stock radio code runs completely unmodified.** VUURWERK features are thin hooks that run after the stock code does its thing. Each module touches only its assigned hardware registers and never interferes with another module. This means the radio never loses any stock functionality; VUURWERK only adds to it.

---

*VUURWERK (Afrikaans: fireworks). Light in the dark. Signal through the noise.*

*Firmware design and architecture: James Honiball (KC3TFZ)*
*Copyright (c) 2026 James Honiball*
