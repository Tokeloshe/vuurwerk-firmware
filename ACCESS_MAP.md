# VUURWERK v1.2.3 Feature Access Map

Complete guide to accessing every feature. Every mapping verified from source code.

---

## Quick Reference

```
SHORTCUTS (press F then number key)
F+0  Cycle modulation      FM > AM > USB          toast: MOD: FM/AM/USB
F+1  Cycle band            50 > 144 > 430 > ...   toast: BAND
F+2  Toggle VFO            A / B                   toast: VFO: A / VFO: B
F+3  Toggle VFO/MR         Frequency / Memory      toast: VFO MODE / MR MODE
F+4  Cycle squelch         0-9                     toast: SQL: N
F+5  Spectrum analyzer     Launches spectrum app   (no toast)
F+6  Cycle TX power        LOW > MID > HIGH        toast: PWR: LOW/MID/HIGH
F+7  Toggle bandscope      On / Off                toast: SCOPE ON / SCOPE OFF
F+8  Reverse offset        Normal / Reverse        toast: NORMAL / REVERSE
F+9  Calling freq jump     Jump / Restore          toast: CALL FREQ / RESTORED
F+*  CTCSS/DCS scanner     Scans for tones on current channel

ON EVERY PTT
  TX Soft Start        S-curve PA ramp, 60ms
  CTCSS Lead-In        Mic mute 150ms, when tone is set
  TX Compressor        Dynamic mic gain via REG_7D

ALWAYS RUNNING (RX)
  FM Gain Staging      Adaptive front-end gain control
  RSSI Smoothing       Exponential moving average filter
  Adaptive Squelch     Noise floor tracking + voice probability
  Signal Classifier    Rise-time categorization (F/N/S/~)
  S-Meter Compensation Corrects RSSI for gain changes
  Squelch Tail Elim    CTCSS tone monitoring via REG_0C
  RSSI Histogram       Statistical noise floor analysis

ON SCREEN
  Status bar       Battery, signal quality Q+bars, lock icon, F key
  VFO lines        Frequency, channel name, TX power L/M/H, bandwidth N
  Center line      Bandscope (when enabled via F+7)
  Center line      Toast notifications (1 second overlay)
  Bottom line      Context status: idle info / RX signal / TX timer / SCANNING
  Bottom line      Signal classifier symbol (F/N/S/~) during RX

MENU
  Press MENU for category picker (6 visible + 1 hidden)
  Keys 1-6 jump to category
  Long-press MENU at picker for About screen
```

---

## F+Key Shortcuts (from home screen)

All shortcuts verified from `app/main.c:107-232`, function `VUURWERK_FKeyShortcut()`.

### F+0: Cycle Modulation
Cycles through FM, AM, USB modulation modes for the current VFO.
Toast: "MOD: FM", "MOD: AM", or "MOD: USB"

### F+1: Cycle Band
Steps through frequency bands (50 MHz, 144 MHz, 430 MHz, etc.). Only works in VFO (frequency) mode.
Toast: "BAND" (or "FREQ ONLY" if in memory mode)

### F+2: Toggle VFO A/B
Switches the active VFO between A and B.
Toast: "VFO: A" or "VFO: B"

### F+3: Toggle VFO/MR Mode
Switches between VFO (direct frequency entry) and MR (memory channel) mode.
Toast: "VFO MODE" or "MR MODE"

### F+4: Cycle Squelch Level
Steps squelch level from 0 through 9, wrapping back to 0.
Toast: "SQL: 0" through "SQL: 9"

### F+5: Spectrum Analyzer
Launches the full-screen spectrum analyzer. Press Star key inside to cycle display modes: NORM, PEAK, MTI, VOX. Press EXIT to return to main screen.
No toast (full screen transition).

### F+6: Cycle TX Power
Cycles transmit power: LOW, MID, HIGH.
Toast: "PWR: LOW", "PWR: MID", or "PWR: HIGH"

### F+7: Toggle Bandscope
Turns the main-screen bandscope (mini spectrum display) on or off. When on, the center line shows a scrolling RF activity timeline.
Toast: "SCOPE ON" or "SCOPE OFF"

### F+8: Reverse Repeater Offset
Toggles the repeater offset direction for the current VFO. Useful for listening on repeater input frequencies.
Toast: "REVERSE" or "NORMAL"

### F+9: Calling Frequency Jump
First press: tunes to the standard simplex calling frequency for your current band, sets the correct step size and modulation. Second press: restores your previous frequency, step, and modulation.
Toast: "CALL FREQ", "RESTORED", or "NO BAND" (if current frequency doesn't match any known band)

Supported bands (from `app/main.c:87-97`, `band_call_table[]`):

| Band | Range | Calling Frequency | Step | Modulation |
|------|-------|-------------------|------|------------|
| 2m SSB | 144.00-144.30 MHz | 144.200 MHz | 100 Hz | USB |
| 2m FM | 144.00-148.00 MHz | 146.520 MHz | 500 Hz | FM |
| 70cm SSB | 432.00-433.00 MHz | 432.100 MHz | 500 Hz | USB |
| 70cm FM | 420.00-450.00 MHz | 446.000 MHz | 2.5 kHz | FM |
| MURS | 151.00-154.00 MHz | 151.940 MHz | 1.125 kHz | FM |
| FRS/GMRS | 462.00-467.00 MHz | 462.5625 MHz | 1.25 kHz | FM |
| CB | 26.90-27.40 MHz | 27.185 MHz | 1 kHz | FM |
| Marine | 156.00-162.00 MHz | 156.800 MHz | 2.5 kHz | FM |
| Airband | 118.00-136.00 MHz | 121.500 MHz | 2.5 kHz | AM |

Narrower SSB entries are checked before wider FM entries for overlapping bands (2m and 70cm).

### F+STAR: CTCSS/DCS Code Scanner
Scans for CTCSS/DCS codes on the current frequency. Found in `app/main.c` MAIN_Key_STAR function.

---

## Menu Items

Press MENU to open the category picker. Press a number key (1-6) to jump directly to a category, or scroll with UP/DOWN and press MENU to enter. Long-press MENU at the category picker to open the About screen.

All category assignments verified from `ui/vuurwerk_menu.c:43-134`.
Menu labels verified from `ui/menu.c:41-140`.

### Category 1: RECEIVE

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| Sql | MENU_SQL | Squelch level | 0-9 |
| Step | MENU_STEP | Frequency step size | 2.5/5/6.25/10/12.5/25/8.33 kHz |
| W/N | MENU_W_N | Channel bandwidth | Wide (25 kHz) / Narrow (12.5 kHz) |
| Demodu | MENU_AM | Demodulation mode | FM / AM / USB |
| Compnd | MENU_COMPAND | Compander | OFF / TX / RX / TX+RX |
| AM Fix | MENU_AM_FIX | AM demod saturation fix | ON / OFF |
| RxMode | MENU_TDR | Receive mode | Main only / Dual watch / Cross-band |

### Category 2: TONE

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| RxCTCS | MENU_R_CTCS | RX CTCSS tone | OFF / 67.0-254.1 Hz |
| TxCTCS | MENU_T_CTCS | TX CTCSS tone | OFF / 67.0-254.1 Hz |
| RxDCS | MENU_R_DCS | RX DCS code | OFF / D023N-D754I |
| TxDCS | MENU_T_DCS | TX DCS code | OFF / D023N-D754I |
| STE | MENU_STE | Squelch tail elimination | ON / OFF |
| RP STE | MENU_RP_STE | Repeater STE | OFF / 1-10 |
| Scramb | MENU_SCR | Scrambler | OFF / 1-10 |

### Category 3: TX (Transmit)

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| TxPwr | MENU_TXP | TX power level | LOW / MID / HIGH |
| TxODir | MENU_SFT_D | TX offset direction | OFF / + / - |
| TxOffs | MENU_OFFSET | TX offset frequency | 0-99.999 MHz |
| TxTOut | MENU_TOT | TX timeout timer | OFF / 1-15 min |
| Mic | MENU_MIC | Microphone gain | 0-4 |
| BusyCL | MENU_BCL | Busy channel lockout | ON / OFF |
| Roger | MENU_ROGER | Roger beep | OFF / Roger / MDC |
| UPCode | MENU_UPCODE | DTMF up code | DTMF string |
| DWCode | MENU_DWCODE | DTMF down code | DTMF string |
| PTT ID | MENU_PTT_ID | PTT ID mode | OFF / UP / DOWN / UP+DOWN / Apollo |
| D ST | MENU_D_ST | DTMF sidetone | ON / OFF |
| D Prel | MENU_D_PRE | DTMF preload time | 10-200 ms |
| D Live | MENU_D_LIVE_DEC | Live DTMF decoder | ON / OFF |

### Category 4: SCAN

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| SList | MENU_S_LIST | Scan list selection | List 1 / List 2 / All |
| SList1 | MENU_SLIST1 | Scan list 1 channels | Channel selection |
| SList2 | MENU_SLIST2 | Scan list 2 channels | Channel selection |
| ScAdd1 | MENU_S_ADD1 | Add to scan list 1 | ON / OFF |
| ScAdd2 | MENU_S_ADD2 | Add to scan list 2 | ON / OFF |
| ScnRev | MENU_SC_REV | Scan resume mode | Time / Carrier / Search |
| ScnWch | MENU_SCANWATCH | Scan+Watch / VFO Split | OFF / SCAN A WATCH B / SCAN B WATCH A / SPLIT |

The **ScnWch** menu item controls both Scan+Watch and VFO Split:
- **OFF**: Normal operation
- **SCAN A WATCH B**: Scan on VFO A, periodically check VFO B
- **SCAN B WATCH A**: Scan on VFO B, periodically check VFO A
- **SPLIT**: VFO Split mode (hops VFO B checking for activity while listening on VFO A)

### Category 5: CHANNEL

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| ChSave | MENU_MEM_CH | Save channel to memory | Channel number |
| ChDele | MENU_DEL_CH | Delete memory channel | Channel number |
| ChName | MENU_MEM_NAME | Channel name | 10 character string |
| ChDisp | MENU_MDF | Channel display format | Frequency / Channel / Name |
| 1 Call | MENU_1_CALL | 1-Call channel | Channel number |

### Category 6: CONFIG

| LCD Label | Menu ID | What It Controls | Values |
|-----------|---------|------------------|--------|
| BackLt | MENU_ABR | Backlight auto-off | OFF / 5s / 10s / 20s / 1m / 2m / 4m / ON |
| BLMin | MENU_ABR_MIN | Backlight minimum brightness | 0-10 |
| BLMax | MENU_ABR_MAX | Backlight maximum brightness | 1-10 |
| BltTRX | MENU_ABR_ON_TX_RX | Backlight on TX/RX | OFF / TX / RX / TX+RX |
| BatTxt | MENU_BAT_TXT | Battery display format | None / Voltage / Percent |
| POnMsg | MENU_PONMSG | Power-on message | Full / Message / Voltage / None |
| Beep | MENU_BEEP | Key beep | ON / OFF |
| KeyLck | MENU_AUTOLK | Auto key lock | OFF / 15s / 30s / 60s / 2m / 5m |
| F1Shrt | MENU_F1SHRT | F1 short press action | Action selection |
| F1Long | MENU_F1LONG | F1 long press action | Action selection |
| F2Shrt | MENU_F2SHRT | F2 short press action | Action selection |
| F2Long | MENU_F2LONG | F2 long press action | Action selection |
| M Long | MENU_MLONG | M long press action | Action selection |
| BatVol | MENU_VOL | Battery voltage display | Read-only |
| BatSav | MENU_SAVE | Battery save mode | OFF / 1:1 / 1:2 / 1:3 / 1:4 |

### Category 7: UNLOCK (hidden)

Only visible when `gF_LOCK` is true (enabled by holding PTT + upper side button at power-on).

| LCD Label | Menu ID | What It Controls |
|-----------|---------|------------------|
| F Lock | MENU_F_LOCK | Frequency lock |
| 200TX | MENU_200TX | 200 MHz TX enable |
| 350TX | MENU_350TX | 350 MHz TX enable |
| 500TX | MENU_500TX | 500 MHz TX enable |
| 350EN | MENU_350EN | 350 MHz band enable |
| ScnREN | MENU_SCREN | Scrambler enable |
| BatCal | MENU_BATCAL | Battery calibration |
| BatTyp | MENU_BATTYP | Battery type (1600/2200 mAh) |
| Reset | MENU_RESET | Factory reset |

---

## Automatic / Always Running (no user action needed)

These features run in the background. All call sites verified from `app/app.c`.

### During TX (every PTT press)

| Feature | Call Site | What Happens |
|---------|-----------|-------------|
| **TX Soft Start** | app.c:1166 | Ramps PA power on S-curve over 60ms via REG_36 |
| **CTCSS Lead-In** | app.c:1165 | Mutes mic 150ms via REG_50 when CTCSS tone is configured |
| **TX Audio Compressor** | app.c:1167 | Reads mic level (REG_64), adjusts mic gain (REG_7D) every 10ms |

Guard condition: `gCurrentFunction == FUNCTION_TRANSMIT`

### During RX (when receiving a signal)

| Feature | Call Site | What Happens |
|---------|-----------|-------------|
| **RSSI Smoothing Filter** | app.c:1324 | EWMA filter on raw RSSI, alpha=1/8 |
| **Signal Quality Update** | app.c:1325 | Tracks RSSI variance over 8-sample window |
| **RSSI Histogram Update** | app.c:1326 | Bins RSSI readings, finds noise floor every 256 samples |
| **Signal Classifier Update** | app.c:1327 | Measures signal rise time, classifies as F/N/S/~ |
| **Dual-Watch Mgmt Update** | app.c:1328 | Updates per-VFO RSSI averaging for dwell time calculation |
| **Activity Log** | app.c:1335 | Records freq, RSSI, CTCSS, quality on squelch open |
| **Activity Log Uptime** | app.c:1342 | Increments uptime counter every 1 second during RX |

Guard condition: `gCurrentFunction == FUNCTION_RECEIVE || FUNCTION_MONITOR`

### Always Running (any radio state)

| Feature | Call Site | What Happens |
|---------|-----------|-------------|
| **FM Gain Staging** | app.c:1180 | Adaptive gain control via REG_13 (FM mode only) |
| **Bandscope Process** | app.c:1177 | Samples RSSI for scrolling spectrum display |
| **VFO Split Process** | app.c:1183 | 5-state hop machine checking VFO B (when enabled) |
| **Squelch Tail Process** | app.c:1309 | Monitors CTCSS tone loss via REG_0C (when STE enabled) |
| **Smart Squelch Update** | app.c:1312 | 3-register voice probability scoring, adjusts REG_78 |

---

## Visible on Screen (user observes but doesn't trigger)

All display rendering verified from `ui/main.c` and `ui/status.c`.

### Status Bar (top row of LCD)

| Element | Position | Source | What It Shows |
|---------|----------|--------|---------------|
| Signal Quality | Right-center | ui/status.c:152-169 | "Q" letter + 0-3 bars during RX |
| Battery | Far right | ui/status.c:200-208 | Battery icon with 0-4 fill bars |
| Lock Icon | Center | ui/status.c:140-149 | Lock icon when keys locked, "F" when F-key active |

### VFO Display Lines

| Element | Position | Source | What It Shows |
|---------|----------|--------|---------------|
| TX Power | Right of freq line | ui/main.c:670-675 | L, M, or H |
| Bandwidth | Right of freq line | ui/main.c:688-689 | N (narrow) or blank (wide) |

### Center Line (gFrameBuffer[3])

This line is shared by multiple features with this priority:

1. **Toast Notification** (highest priority, 1-second overlay)
   - Source: ui/main.c:855-863
   - Bordered box with centered text (e.g., "PWR: HIGH", "SQL: 5")
   - Disappears after ~1 second (100 timer ticks at 10ms)

2. **Bandscope** (when enabled via F+7)
   - Source: ui/main.c:744-752, renders via BANDSCOPE_Render()
   - 128-pixel wide scrolling RF activity timeline
   - 8 pixels high, new samples scroll in from right
   - Peak dots show strongest signal, noise floor as dotted line

3. **RSSI Bar** (when ENABLE_RSSI_BAR is defined)
   - Source: ui/main.c:170-256
   - Text: "RSSI S-level" + graphical bar

### Bottom Line (gFrameBuffer[6])

| Radio State | What It Shows | Source |
|-------------|---------------|--------|
| Idle | Step size, bandwidth (N/W), modulation mode | ui/main.c:845-849 |
| RX | "RX S[0-9] [dBm] Q:[0-3] [F/N/S/~]" | ui/main.c:835-838 |
| TX | "TX M:SS PWR:L/M/H" (timer + power) | ui/main.c:822-826 |
| Scanning | "SCANNING" | ui/main.c:840-842 |

The **Signal Classifier Symbol** (F/N/S/~) appears at the end of the RX status line. F = fast/FM, N = normal/SSB, S = slow/carrier, ~ = noise.

---

## Triggered by Radio State (automatic but conditional)

| Feature | Trigger | Condition |
|---------|---------|-----------|
| **Activity Log recording** | Squelch opens | When transitioning from no-signal to signal during RX |
| **Intelligent Dual-Watch** | VFO toggle timer | Only when dual-watch is enabled (RxMode menu) |
| **Scan+Watch** | Scan active | Only when ScnWch is set to SCAN A/B WATCH B/A |
| **VFO Split** | Background hop | Only when ScnWch is set to SPLIT |

---

## Long-Press / Hidden Access

| Action | How | Source |
|--------|-----|--------|
| **About Screen** | Long-press MENU at category picker | app/menu.c:1745-1748 |
| **Unlock Category** | Hold PTT + upper side button at power-on | Enables gF_LOCK, reveals 7th menu category |
| **Key Lock** | Long-press F key | app/generic.c, GENERIC_Key_F() |

---

## Spectrum Analyzer Modes

Access: F+5 from main screen. Press Star key to cycle modes.

| Mode | Label | What It Shows |
|------|-------|---------------|
| 0 | NORM | Standard spectrum bars |
| 1 | PEAK | Bars + peak hold dots (decay every 10 sweeps) |
| 2 | MTI | XOR diff, only shows signal changes |
| 3 | VOX | Voice probability per bin, voice-hop navigation (UP/DOWN) |

In VOX mode:
- Bars scaled by voice confidence (half-height < 75%, full-height >= 75%)
- UP/DOWN keys hop to next frequency bin with voice probability >= 50
- Auto-listens and checks noise register for voice presence

---

## Unreachable Features

None. All 27 documented features have verified execution paths from `main()`. Every function listed as "active" in FEATURES.md has at least one reachable call site.

---

*VUURWERK v1.2.3 -- All mappings verified from source code, 2026-02-21*
