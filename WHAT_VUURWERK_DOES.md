# What VUURWERK Actually Does

**v1.2.3: Plain English Guide to Every Feature**

VUURWERK is custom firmware for the Quansheng UV-K5 radio. It keeps everything the stock radio does, then adds 27 features on top, all fitting in the same 60KB of flash memory. Here's what each one does and why it matters.

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

The spectrum analyzer scans a range of frequencies and shows what's active. Press the Star key to cycle through four display modes:

### 18. NORM: Normal Spectrum
**What it does:** Standard spectrum display. Each frequency bin shows a bar proportional to signal strength. Simple, clean, effective.

### 19. PEAK: Peak Hold
**What it does:** Same as NORM, but adds dots above the bars showing the strongest signal seen at each frequency. The peaks slowly decay over time. Useful for catching brief transmissions you might otherwise miss.

### 20. MTI: Moving Target Indicator
**What it does:** Borrowed from radar technology. Shows only what has *changed* since the last sweep. If a signal is steady (like a carrier or birdie), it disappears. If something new appears or changes strength, it lights up. Perfect for finding intermittent signals in a crowded band.

### 21. VOX: Voice-Seeking Spectrum
**What it does:** The headline feature of VUURWERK v1.2.0. Instead of showing all signals equally, it identifies which ones are likely carrying voice. For each frequency bin, it reads the radio chip's noise indicator alongside the signal strength. Clean audio with a strong signal scores high. Noisy, weak, or interference-heavy bins score low.

The display shows confidence: bins with no voice get just a floor dot. Low-confidence bins get half-height bars. High-confidence bins get full-height bars. Press UP or DOWN to hop directly to the next bin with likely voice activity; the radio tunes there and lets you listen, then resumes scanning when the signal drops.

---

## User Interface

### 22. RSSI Bar
**What it does:** A visual signal strength meter on the main display, compensated for gain staging adjustments so it always reads accurately in FM mode.

### 23. Context-Aware Status Line
**What it does:** The bottom line of the display changes based on what the radio is doing. Idle shows the firmware name. RX shows signal info. TX shows power. Scan shows progress. It's always relevant information without cluttering the screen.

### 24. Boot Screen
**What it does:** Shows the VUURWERK name and version when the radio powers on.

### 25. About Screen
**What it does:** Long-press MENU at the category picker to see firmware info and feature highlights.

### 26. Categorized Menu
**What it does:** The stock radio has 60+ menu items in one flat list. VUURWERK organizes them into 7 categories: Receive, Tone, Transmit, Scan, Channel, Config, and a hidden Unlock category. You scroll through categories first, then items within. Finding settings takes seconds instead of minutes.

### 27. Activity Log
**What it does:** Records RF activity every time the squelch opens. Each entry captures the frequency, signal strength (RSSI in dBm), CTCSS tone (if present), signal quality rating, and duration. The 20-entry ring buffer acts as a logbook of what the radio has heard during the current session. If the same frequency is heard again within 10 seconds, the existing entry is updated with the better signal reading rather than creating a duplicate. Radio uptime is tracked in a separate counter that increments once per second during reception.

---

## The Architecture

Every one of these features follows one rule: **the stock radio code runs completely unmodified.** VUURWERK features are thin hooks that run after the stock code does its thing. Each module touches only its assigned hardware registers and never interferes with another module. This means the radio never loses any stock functionality; VUURWERK only adds to it.

---

*VUURWERK (Afrikaans: fireworks). Light in the dark. Signal through the noise.*

*Firmware design and architecture: James Honiball (KC3TFZ)*
*Copyright (c) 2026 James Honiball*
