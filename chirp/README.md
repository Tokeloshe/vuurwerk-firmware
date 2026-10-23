# VUURWERK CHIRP driver

`uvk5_vuurwerk.py` is a community-contributed driver for
[CHIRP](https://chirpmyradio.com). It lets you read and write your VUURWERK
radio's channels, side keys and settings from your computer, with no need to
flash back to stock egzumer first. It loads as a CHIRP Developer-mode module
and talks to a Quansheng UV-K5 running VUURWERK v1.5.1.

The driver recognises VUURWERK (and egzumer) radios directly; no firmware
change is needed.

## What it reads and writes

**Channels** (all 200 memories):

| Field | Notes |
|---|---|
| Frequency and offset | with the offset direction |
| CTCSS and DCS | receive and transmit tones separately |
| Modulation | FM, AM or USB |
| Power, bandwidth, step | LOW / MID / HIGH, WIDE / NARROW |
| Scrambler, busy lockout, reverse | as on the radio |
| Name | as shown by ChName |
| Scan lists and compander | list 1, list 2, OFF / TX / RX / TX/RX |

**Settings**, grouped as CHIRP shows them:

| Group | What is in it |
|---|---|
| Basic | squelch, RxMode, 1 Call, key lock, TX timeout, battery save, scan resume, AM Fix, display, backlight, audio (mic gain, Beep, Roger, STE, RP STE) and the current VFO frequencies |
| Advanced | battery type and S-meter levels |
| Programmable Keys | SIDE1 and SIDE2 short and long press, M Long |
| Scan Lists | list membership and priority channels |
| DTMF Settings and Contacts | stored on the radio but unused, see below |
| FM Radio | broadcast presets (stored, not used by VUURWERK) |
| Unlock Settings | F Lock, Tx 200, Tx 350, Tx 500, 350 En, ScraEn |
| Calibration | an optional, off-by-default calibration upload for advanced use |

## Not covered

- The spectrum analyzer and bandscope have no settings in CHIRP.
- Whether PATROL is running is never stored, so it is never in an image.

## Safe steps for the PATROL settings

> **Download before you edit.** The four PATROL settings (PSens, PSpeed,
> PDwell, PResum) and the saved PATROL range travel inside the image CHIRP
> reads and writes, even though CHIRP shows no field for them. An upload puts
> back whatever the image holds.
>
> 1. **Download from the radio** first, every time.
> 2. **Edit** channels and settings in that fresh image.
> 3. **Upload** that image. Never upload an older saved image, or the PATROL
>    settings and range go back to what they were when it was saved.

See [PATROL](../USER_MANUAL.md#8-patrol) in the manual for what these
settings do on the radio.

## Hidden DTMF items

UPCode, DWCode, D Prel and D Live are not in the v1.5.1 radio menu, because
VUURWERK has no DTMF. CHIRP still shows and stores them; changing them has no
effect on the radio.

## How to load it

1. Open CHIRP.
2. Enable **Developer mode** in CHIRP's settings, then **restart CHIRP**.
3. **File > Load Module**, and select `uvk5_vuurwerk.py`.
4. Connect the radio with a Kenwood-style 2-pin programming cable.
5. **Radio > Download from Radio**, then pick **Quansheng**, then
   **UV-K5 VUURWERK** in the radio list.

Read your radio, edit channels and settings, and write back. For programming
channels on the radio itself see
[Channel Programming](../USER_MANUAL.md#11-channel-programming); for
installing the firmware see [FLASHING.md](../FLASHING.md).

## Credit and license

The driver was contributed by [mjwgeek](https://github.com/mjwgeek), adapted
from the egzumer driver, which is based on Jacek Lipkowski SQ5BPF's original
UV-K5 driver. See [CREDITS.md](../CREDITS.md).

It is GPL: the file carries its own header (GNU General Public License v2 or,
at your option, any later version). It is the contributor's work, shipped here
as-is with credit.
