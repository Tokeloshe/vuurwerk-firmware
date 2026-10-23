# Flashing VUURWERK v1.5.1

Flashing takes about a minute and does not touch your channels or settings:
only the firmware changes. This guide covers the browser flasher, a
command-line alternative, the fault banners you might see, and recovery.

## What you need

| Item | Notes |
|---|---|
| Quansheng UV-K5 V1 | The original board; see the hardware check below |
| Kenwood-style 2-pin programming cable | The same cable CHIRP uses |
| Google Chrome | The browser flasher needs Web Serial; Firefox and Safari do not have it |
| The firmware file | [`release/vuurwerk-v1.5.1.packed.bin`](release/vuurwerk-v1.5.1.packed.bin) |

## The flashing flow

```
 +----------------+     +----------------+     +---------------------+
 | 1. DOWNLOAD    |     | 2. RADIO OFF   |     | 3. BOOTLOADER MODE  |
 | vuurwerk-v1.5.1| --> | rotary switch  | --> | hold PTT, then turn |
 | .packed.bin    |     | fully off      |     | the radio ON        |
 +----------------+     +----------------+     +----------+----------+
                                                          |
                                              flashlight LED lights up
                                              = bootloader is waiting
                                                          |
 +----------------+     +----------------+     +----------v----------+
 | 6. FLASH       |     | 5. PICK FILE   |     | 4. CONNECT          |
 | click "Flash   | <-- | "Select        | <-- | cable into radio    |
 | firmware",     |     | firmware file" |     | and computer; open  |
 | wait ~10 s     |     | -> .packed.bin |     | the flasher in      |
 +-------+--------+     +----------------+     | Chrome              |
         |                                     +---------------------+
         v
 +----------------+     +----------------+
 | 7. POWER CYCLE |     | 8. VERIFY      |
 | unplug, off,   | --> | boot banner:   |
 | wait 5 s, on   |     | VUURWERK v1.5.1|
 +----------------+     +----------------+
```

## Step by step

1. **Download** `vuurwerk-v1.5.1.packed.bin` from the `release/` folder.
2. **Turn the radio off.**
3. **Hold PTT** and turn the radio on while still holding it. The flashlight
   LED turns on: that is the bootloader saying it is ready.
4. **Connect** the programming cable to the radio and your computer.
5. **Open** https://egzumer.github.io/uvtools/ in Chrome.
6. **Click "Select firmware file"** and choose the `.packed.bin` you
   downloaded.
7. **Click "Flash firmware".** The transfer takes about 10 seconds.
8. **Disconnect** the cable, then power the radio off and on.

For about a second you see the boot banner:

```
+----------------------------+
|                            |
|         VUURWERK           |
|                            |
|                            |
|          v1.5.1            |
|                            |
|        Precision RF        |
|                            |
+----------------------------+
```

| Row | What is there |
|---|---|
| status bar | blank |
| rows 0-1 | VUURWERK in the big font |
| row 3 | v1.5.1 |
| row 5 | Precision RF |

If Menu > CONFIG > POnMsg is set to VOLTAGE you see the battery voltage and
percentage instead; NONE shows nothing. Either way, you are done.

## Command-line alternative

If you prefer a terminal or cannot use Chrome,
[k5prog](https://github.com/sq5bpf/k5prog) works with the same bootloader
mode (steps 2 to 4 above). Mind the two file formats:

| File | Format | Tool |
|---|---|---|
| `vuurwerk-v1.5.1.packed.bin` | Quansheng updater format, what `release/` ships | Browser flasher |
| `vuurwerk-v1.5.1.bin` | Raw flash image, what a source build produces ([BUILD.md](BUILD.md)) | k5prog |

```bash
# back up the current firmware first (recommended)
k5prog -r backup.bin

# flash the RAW image from a source build
k5prog -F -YYY -b vuurwerk-v1.5.1.bin
```

If you only have the packed release file, use the browser flasher.

## Hardware check

VUURWERK runs only on the original **UV-K5 V1**. The V2 and V3 revisions
use a different processor and will not boot this firmware. To check, remove
the battery and read the board label under it: V2 and V3 boards are marked
with their revision, and **no marking means V1**.

Flashing the wrong hardware does not damage it, but you will need recovery
firmware for your board: [K5TOOL](https://github.com/qrp73/K5TOOL) supports
all UV-K5 versions.

## Fault banners

At power-on the radio checks that the transceiver answers and that the
battery calibration is present. If either check fails, a fault banner
replaces the normal boot screen, even with POnMsg set to NONE.

```
+----------------------------+
|                            |
|          BK4819            |
|                            |
|                            |
|          FAULT             |
|                            |
|                            |
|       RX/TX disabled       |
+----------------------------+
```

```
+----------------------------+
|                            |
|          EEPROM            |
|                            |
|                            |
|          FAULT             |
|                            |
|                            |
|         calib lost         |
+----------------------------+
```

| Banner | Meaning | What to do |
|---|---|---|
| `BK4819` / `FAULT` / `RX/TX disabled` | The transceiver did not answer at power-on | Power off and on. If the radio stays on a `BK4819` / `FAULT` screen without the bottom line, the transceiver is still silent and the radio waits for it: this is hardware, not the flash |
| `EEPROM` / `FAULT` / `calib lost` | The battery calibration data is blank or unreadable | The radio boots on; the battery readings may be wrong. Restore a saved image with CHIRP if you have one |

A fault banner is never caused by a bad flash: a bad flash does not boot at
all, and the bootloader lets you flash again.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| The flashlight LED does not light in step 3 | The radio was not fully off, or PTT was pressed after the switch | Switch off, hold PTT first, then switch on, slowly |
| Chrome shows no serial port | Charge-only cable, marginal clone cable, or another program holds the port | Try another cable or USB port; close CHIRP or any terminal. On Linux add yourself to the `dialout` group |
| Flashing fails partway | Cable disturbed or port dropped | Power off, enter bootloader mode again, flash again. The bootloader is never overwritten, so you can always retry |
| A fault banner shows after the flash | A hardware condition, not the firmware | See Fault banners above |
| One VFO seems deaf after the flash | A leftover tone, offset, reverse or scrambler setting on that VFO | In frequency mode press F+EXIT on that VFO. Toast `SIMPLEX` |
| The radio shows `Long press #` / `to unlock` | The keypad is locked | Hold F (the # key) until `UNLOCKED` shows |

## Reverting to stock

Flash your `backup.bin` (if you made one with k5prog), or download the stock
Quansheng firmware or an egzumer release and flash it with the same steps.
The browser flasher accepts those packed images too.

## Notes

- Do not disconnect the cable while the progress bar is moving.
- Your channels, settings and calibration are preserved across the flash.
- Release checksums are listed in [CHANGELOG.md](CHANGELOG.md#release-checksums)
  if you want to verify your download.
- The getting-started chapter of [USER_MANUAL.md](USER_MANUAL.md#1-getting-started)
  covers the first power-on, and [README.md](README.md) has the overview.
