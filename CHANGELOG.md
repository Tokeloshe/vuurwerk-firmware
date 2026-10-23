# VUURWERK Changelog

What changed in each release, in the words you would use on the radio.
Checksums for every release file are in [Release checksums](#release-checksums)
at the end. The illustrated tour of the current release is
[WHATS_NEW.md](WHATS_NEW.md); how to put it on the radio is
[FLASHING.md](FLASHING.md).

## Versions

- [v1.5.1 (October 2026)](#v151-october-2026-patrol-and-a-stability-pass): PATROL, a hands-free voice hunt, and the largest fix pass VUURWERK has had.
- [v1.5.0 (July 2026)](#v150-july-2026): leaving the spectrum no longer flips the VFO or leaves the radio deaf.
- [v1.4.5 (July 2026)](#v145-july-2026): F+EXIT works; it also resets the modulation to FM.
- [v1.4.4 (July 2026)](#v144-july-2026): F+EXIT, the reset-to-simplex key.
- [v1.4.3 (July 2026)](#v143-july-2026): you always leave the spectrum on the VFO you entered from.
- [v1.4.2 (July 2026)](#v142-july-2026): the VOX lock carry-over returns, done safely.
- [v1.4.1 (July 2026)](#v141-july-2026): the carry-over is removed while it is reworked.
- [v1.4.0 (July 2026)](#v140-july-2026): deaf-after-spectrum fixed; MTI mode removed.
- [v1.3.8 and v1.3.7 (July 2026)](#v138-and-v137-july-2026): two attempts at the deaf-radio fix.
- [v1.3.6 (July 2026)](#v136-july-2026): VOX Voice Lock with PTT.
- [v1.3.5 (July 2026)](#v135-july-2026): UP and DOWN seek the next voice.
- [v1.3.4 (July 2026)](#v134-july-2026): the bottom RX readout returns.
- [v1.3.3 (July 2026)](#v133-july-2026): scanner deafness on tone channels fixed; voice seeking improved.
- [v1.3.2 (June 2026)](#v132-june-2026): boot banner fixed.
- [v1.3.1 (June 2026)](#v131-june-2026): menu hints, Split indicator, battery sag warning.
- [v1.3.0 (June 2026)](#v130-june-2026): a sixteen-item bug sweep.
- [v1.2.8 (May 2026)](#v128-may-2026): scanner deafness fixed; CHIRP driver added.
- [v1.2.7 (May 2026)](#v127-may-2026): side-key symmetry and a faster tone scan.
- [v1.2.6 (May 2026)](#v126-may-2026): spectrum keys reorganised.
- [v1.2.5 (May 2026)](#v125-may-2026): toasts, boot self-test, flashlight watchdog.
- [v1.2.4 (April 2026)](#v124-april-2026): three community-reported fixes.
- [Early releases](#early-releases): v1.2.3 and older.

## v1.5.1 (October 2026): PATROL and a stability pass

PATROL is a hands-free version of the VOX voice hunt. Set a range with two
key presses, press 0, and the spectrum walks that range on its own, stops on
voices, listens, and moves on. Alongside it, this release fixes more bugs
than any VUURWERK release before it, several of them on the transmit side.

### New

- **PATROL.** In the spectrum's VOX mode, press 0 to start it; the corner
  label changes from `VOX` to `PTRL`. Press 0 again or EXIT to stop.
- **Range keys.** In VOX mode, 2 stores the left edge of the window on
  screen as the range start (`SET LO`) and 8 stores the right edge as the
  stop (`SET HI`). The step and modulation in force are saved with the range.
  With no range stored, PATROL sweeps the window on screen.
- **Four PATROL settings** under Menu > SCAN, all with the hint
  `PATROL ONLY`: PSens (LOW / NORM / HIGH), PSpeed (SLOW / NORM), PDwell
  (SHORT / NORM / LONG, 0.5 / 1 / 2 s) and PResum (SHORT / NORM / LONG).
  They change PATROL only; the hand-held VOX hunt is unchanged.
- **PATROL safeguards.** An automatic stop never lasts more than 60 s; that
  frequency is then skipped for 2 minutes of sweeping. MENU while PATROL is
  camped skips that frequency until PATROL stops. At 5 % battery or less
  PATROL stops and the spectrum closes so the `LOW BATTERY` warning can show.
- **PATROL keys.** While PATROL runs only 0, MENU, PTT and EXIT act. The
  BackLt timer runs during PATROL; any key restarts it, and on a dark screen
  the first key only wakes the light.
- **F+9 calling-frequency jump** lands on a clean simplex frequency: offset
  direction off, tones off, reverse off, scrambler off, saved. Toast
  `CALL FREQ`; `NO BAND` outside the table; `FREQ ONLY` on a memory channel.

### Changed

- The main-screen frequency reads `146.52000` in the standard font.
- The compander glyph sits at the right of the second row, on memory
  channels too.
- The main screen repaints twice a second while transmitting, receiving or
  monitoring, so the TX timer, the RX line and the bandscope stay live and
  toasts clear on time.
- The RX line reads like `RX S9 -73dBm Q3`, so readings of -100 dBm and
  weaker fit.
- The spectrum corner label shows `PTRL` while PATROL runs and `NORM`, `PEAK`
  or `VOX` otherwise, clear of the modulation at every frequency.
- Menu hints are shorter and stay on the items whose names are cryptic. D ST
  reads `1750/APOLLO SIDETONE`; 1 Call has no hint.
- Four DTMF items that did nothing (UPCode, DWCode, D Prel, D Live) are
  hidden.

### Fixed

- Leaving the spectrum with PTT held no longer transmits; release and press
  again.
- The radio could freeze on a nearly flat battery, with the carrier up if it
  happened mid-over. Fixed.
- A crash can no longer hold the transmitter on; the radio restarts instead.
- Pressing PTT with a key still held transmits on the frequency, power and
  reverse shown, and releasing the key mid-over keeps the carrier up.
- Holding EXIT while transmitting no longer drops the carrier.
- A 1750 Hz tap in the repeater tail window, or a TX timeout, no longer
  leaves the receiver deaf.
- A TX timeout landing just after unkey no longer replays the roger beep
  and tail, and a very short PTT blip no longer leaves the transmitter
  half-armed while receiving.
- AM and USB keep their own mic level for the whole over.
- A corrupt stored power setting transmits on LOW instead of an
  uncalibrated level.
- Leaving the spectrum always restores audio, squelch, monitor, the green
  LED and the status row, however long you held EXIT.
- A long press that opens the spectrum does nothing inside it, and a held
  EXIT acts once (it never backs out of entry and quits in one go).
- A side key or M Long set to SPECTRUM is refused while the keypad is
  locked (double beep and the unlock prompt).
- `LOCKED` shows the moment you press PTT; UP or DOWN clears it; no stale
  banner returns at the next session.
- Steps under 2.5 kHz open centred on the VFO frequency.
- PEAK dots sit on their bars; key 6 changes the listen bandwidth at once;
  AM and USB survive the found-voice chord; the chord respects Beep OFF; the
  flashlight auto-off keeps counting inside the spectrum.
- The VOX lock carry-over takes the modulation you were listening in.
- The menu tone scan (RxCTCS or RxDCS, then STAR) no longer wipes the VFO's
  offset, tone and power; the found tone lands on the right item and beeps.
- The menu does not time out while its tone scan runs; the scan is refused
  in plain CROSS BAND; a menu timeout in Scramb drops the scrambler you
  were trying.
- The F+STAR screen shows the frequency you are on and says `NO SIGNAL`
  when there was nothing to scan.
- STE (mute squelch tail) works on CTCSS channels as it does on DCS.
- The F+7 bandscope draws its dotted noise-floor line once the noise
  estimate has settled.
- Scan+Watch shows `S+W` in the status bar; an MR scan on an empty list
  keeps its `EMPTY LIST` toast.
- The screen updates before the key click; refused F shortcuts double-beep;
  a STAR tap no longer swallows the next key; F+4 keeps unsaved memory
  edits; About closes when a signal arrives.
- Viewing a blank memory slot is safe; BatCal refuses typed digits; PTT
  works on the category screen; long menu hints are not clipped; a side-key
  action not in this build shows NONE instead of freezing.
- VFO Split no longer walks your own VFO with VFO B selected, hops only
  while VFO A is selected, returns to VFO A when disarmed mid-hop, stands
  aside for scans and monitor, alerts once per channel each time it goes
  busy, checks VFO B's real frequency on the first hop, and VFO B is heard
  correctly on every band.
- Dual watch listens at the right gain on both VFOs; moving from FM to AM or
  USB restores the stock gain.
- A transmit sag no longer starts the low-battery shutdown; flashlight
  blink, SOS and auto-off keep working on a low battery.
- The `BK4819 FAULT` banner stays on screen with the backlight on while
  the fault persists, and shows even with POnMsg set to NONE.
- CHIRP: a corrupted upload packet is rejected safely.
- The TX timer no longer shows garbage with TxTOut at 15 min; the radio no
  longer drops into power save the instant you leave the spectrum; the
  spectrum draws correctly above 1 GHz; the PATROL settings are live from
  power-on; the adaptive squelch no longer loses its noise floor during a
  long receive.

### Removed

- The gain editor under the STILL meter; MENU does nothing there now.
- The big-digit main-screen font.
- The second press of F+9 that returned you to the previous frequency.
- The class letter on the RX line.
- Four DTMF menu items that did nothing (hidden, see Changed).

## v1.5.0 (July 2026)

Two field reports fixed. Holding EXIT a beat too long when leaving the
spectrum could flip you to the other VFO; and a spectrum exit could leave the
radio mistuned and silent until something else retuned it.

### Fixed

- Leaving the spectrum with EXIT held no longer swaps VFO A and B.
- The radio is no longer mistuned or silent after a spectrum session.

## v1.4.5 (July 2026)

F+EXIT did not work in v1.4.4: the chord swapped VFOs instead of resetting
the one you were on.

### Fixed

- F+EXIT now performs the simplex reset, and it also sets the modulation
  back to FM.
- With cross band on, the watch frequency is heard again after a spectrum
  exit.
- PTT in a wide span that shows `VOX` no longer drops into the STILL view.
- The VOX lock carry-over is skipped when reverse is on, and the carried
  frequency is rounded to the VFO step.

## v1.4.4 (July 2026)

### New

- **F+EXIT** resets the active VFO to true simplex: RX and TX tones off,
  offset and direction cleared, reverse off, scrambler off. Toast `SIMPLEX`.
  On a memory channel you get `FREQ ONLY` and nothing is touched.

## v1.4.3 (July 2026)

### Fixed

- Every spectrum exit, locked or not, returns you on the VFO you entered
  from. Dual watch gets a fresh dwell afterwards, so it cannot flip the
  instant the spectrum closes.

## v1.4.2 (July 2026)

### Changed

- The VOX lock carry-over is back, written straight into the active VFO on
  exit. Frequency mode and same band only; it never touches a memory
  channel.

The v1.3.7 and v1.3.8 builds were mislabelled and ran an older version;
v1.4.2 is the first clean build of this carry-over.

## v1.4.1 (July 2026)

### Removed

- The VOX lock carry-over, while it is reworked. PTT still locks a voice
  and UP/DOWN still seek; the frequency just no longer follows you out of
  the spectrum.

If a radio flashed from an earlier v1.3.x build still seems deaf, power-cycle
it, then re-enter the frequency or reset the VFO.

## v1.4.0 (July 2026)

### Fixed

- Locking a voice in the spectrum and exiting could leave the radio deaf
  with the green light on and switch it to the other VFO with dual watch on.
  Fixed.
- PTT in VOX mode only ever locks a voice (or does nothing if none is heard
  yet) instead of dropping into the STILL view.

### Removed

- The MTI spectrum mode. STAR now cycles NORM, PEAK and VOX. The About
  screen's feature list is back.

## v1.3.8 and v1.3.7 (July 2026)

Two attempts at the deaf-after-spectrum fix. Both were mislabelled builds
and ran older code; v1.4.0 contains the fix that works.

## v1.3.6 (July 2026)

### New

- **VOX Voice Lock.** In VOX mode, tap PTT while listening to a station and
  the radio stops hopping, shows `LOCKED` and holds that frequency with the
  squelch open through any pause. UP or DOWN releases the lock.
- **Carry home.** EXIT while locked, in frequency mode and on the same band,
  makes the locked frequency your VFO frequency.

### Changed

- The mode toast reads `VOX` instead of `MODE VOX`.

## v1.3.5 (July 2026)

### Changed

- In VOX mode, UP seeks the next voice above and DOWN the next voice below,
  without wrapping. When there is no voice that way the key moves the window
  instead, so UP and DOWN always move you. SIDE1 and SIDE2 move without
  seeking.

### Fixed

- Entering VOX mode no longer shows false voice bars for one sweep.
- A seek can no longer land on a blacklisted bin.

### Removed

- The descending "nothing found" chord.

## v1.3.4 (July 2026)

### Changed

- The RX readout (`RX S7 -83dBm Q3`) returns to the bottom row, and it now
  agrees with the centre RSSI bar.

## v1.3.3 (July 2026)

### Fixed

- The scanner went deaf on tone-coded memory channels: it moved on before
  the tone decoder could confirm. Reported by Scott Savage, KC7WDG.
- A bug that could silently change saved settings was fixed.
- Squelch tail elimination muted real DCS receptions.
- Chained voice seeks could lock silently; the first bin of a span was never
  scanned; the blacklist leaked between sessions.
- Dual watch yields to manual tuning; VFO Split, Scan+Watch, F+8 reverse and
  the side-key toasts had edge-case fixes.

### Changed

- Scanning is 2 to 3 times faster on empty bands with no loss of
  sensitivity.
- The middle and upper Mic steps drive about 2 dB harder.
- VOX mode seeks voice, not the loudest signal, and holds through natural
  speech gaps.

## v1.3.2 (June 2026)

### Fixed

- POnMsg set to FULL booted to a black screen. FULL and MESSAGE now show the
  VUURWERK banner, VOLTAGE shows the battery voltage and percentage, and
  NONE is blank.

## v1.3.1 (June 2026)

### New

- A hint line on the menu's bottom row explains cryptic items in plain
  words while they are highlighted.
- `SP` in the status bar while VFO Split is armed.
- Split activity raises an `ACT 146.52000` style toast with a short beep.
- A `SAG 0.25V` style toast after unkey when the pack sagged 0.20 V or more.

### Changed

- Weak voices open the squelch a little sooner; off during scans and Split.
- Squelch tail elimination covers DCS channels too.
- Dual watch reacts about four times faster.
- The spectrum sweeps about 10 % faster.

## v1.3.0 (June 2026)

A sixteen-item bug sweep.

### Fixed

- A short deaf dip after every FM channel change or VFO flip.
- Dual watch deaf for a second or two after traffic on the other VFO.
- AM audio chopped on channels carrying a stray CTCSS code with STE on.
- F+9 hit the wrong VFO under dual watch or cross band.
- Squelch stayed easier to open after a voice transmission ended.
- The tone scanner's `NO SIGNAL` abort leaked the previous save prompt.
- VFO Split never actually walked the memory list; power save and Split
  fought each other.
- Scan+Watch was one-shot; the ScnWch setting now arms on every scan.
- Leaving the spectrum left the receiver tuned below the displayed
  frequency.
- AM gain dived once after boot.
- A memory scan on an empty list showed `SCANNING`; it now says
  `EMPTY LIST`.
- The TX line showed the wrong power letter under cross band.

### Removed

- The bold small font; bold text now uses the regular small font.

## v1.2.8 (May 2026)

### Fixed

- The receiver went deaf during a scan while a parked frequency received
  normally. Reported by Scott Savage, KC7WDG.
- Scan+Watch went deaf on returning to the scan VFO.
- F+3 showed a false `NO CHANNELS` on every successful toggle.

### New

- A CHIRP driver for reading and writing channels, side keys and settings
  without flashing back to stock. Contributed by mjwgeek. See
  [chirp/README.md](chirp/README.md).

## v1.2.7 (May 2026)

### Fixed

- SIDE2 held in the spectrum now steps down, mirroring SIDE1 up.
- The tone scanner checks for a carrier first and says `NO SIGNAL` on dead
  air instead of running for a minute.
- CTCSS scans confirm as quickly as DCS scans and give up sooner with
  `SCAN FAIL.`

## v1.2.6 (May 2026)

### Changed

- Spectrum keys: SIDE1 and SIDE2 step the window up and down; MENU
  blacklists the current peak; STAR shows the new mode name briefly.
- VOX bars are drawn only for bins that qualify as voice, with a dot above
  each.

### Fixed

- A voice seek no longer lands on a blacklisted bin.

## v1.2.5 (May 2026)

### New

- Toasts confirm side-key, M Long and F shortcut actions; `LOCKED`,
  `NO CHANNELS`, `FM ONLY`, `SCAN ON` and `SCAN OFF`.
- `SCANNING 12c/s` style scan-rate readout.
- The flashlight turns itself off after 30 minutes, 10 on a low battery;
  SOS never.
- The tone scan gives up with a double beep instead of running forever, and
  beeps once when it finds a tone; `Cs` in the status bar while it runs.
- A boot self-test shows `BK4819 FAULT` or `EEPROM FAULT` when the hardware
  does not answer.
- The battery reading stays live while transmitting; the About screen shows
  the last TX sag.
- The backlight fades out over about 2 s and re-arms on TX or RX per BltTRX.
- A quieter backlight: no whine in transmitted audio when the screen is
  dimmed.
- Ascending chord when a voice seek lands.

### Changed

- Dual watch weights its dwell by how long each VFO has been busy.

### Removed

- The activity log.

## v1.2.4 (April 2026)

### Fixed

- The spectrum froze on entry at the larger scan steps.
- Channel names drifted after hours of dual watch.

### Changed

- About moved into the CONFIG menu.

## Early releases

- **v1.2.3 (February 2026):** activity log, adaptive dual watch, VFO Split
  under ScnWch, class letter on the status line.
- **v1.2.2 (February 2026):** size optimisation pass.
- **v1.2.1 (February 2026):** voice seek stays in the sweep view; auto-listen
  skips non-voice bins; mode label `VOX`.
- **v1.2.0 (February 2026):** VOX spectrum mode added, waterfall removed, STE
  menu toggle works.
- **v1.1.0 (February 2026):** squelch tail elimination and smart squelch
  added.
- **v1.0.9 (February 2026):** categorised menu with number-key category
  selection.
- **v1.0.4 to v1.0.8 (February 2026):** TX compressor, CTCSS lead-in, soft
  start, VFO Split, bandscope, Scan+Watch, signal quality, About screen.
- **v1.0.0 (February 2026):** first build on egzumer 2.0, itself based on
  DualTachyon's firmware; DTMF calling removed.

## Release checksums

Verify a download with `sha256sum` (Linux, macOS) or
`certutil -hashfile <file> SHA256` (Windows) and compare with this table.

| Version | File | SHA-256 |
|---|---|---|
| v1.5.1 | .packed.bin | 40705cfa43abd85eb40a80ae1a7b5af94d27319fbb2ebce91bfbb5605598c47f |
| v1.5.1 | .bin | 49910f23eccd23f70d3c2282ae5b2f3f9bfd62c02e2ce5bff13c2950fa63063b |
| v1.5.0 | .packed.bin | 14b54b2e84623d4009f619d42e6a3f82230b66d77d7e513b0636f3dfb1ddee15 |
| v1.4.5 | .packed.bin | 1a95658a40e3090db42690c9a61774eeb3d5fe9de3944ca8297ef0815dd937b7 |
| v1.4.4 | .packed.bin | 1c137921e333bad4e7e93e1c18c2ef97b24b80c282e6e4bf75f1b866432f1d56 |
| v1.4.3 | .packed.bin | 4771eec942d04384c4086be78890fc6d2a5eeac51e35cade931393044743bd2a |
| v1.4.2 | .packed.bin | 75a286be3f3a41beed735ac4209f6ceeb3562098d604bc2a1dab5fa7130fb193 |
| v1.4.1 | .packed.bin | 335c660e0a1a0ff06d6fed358b612db1761b7ecbe2c85bf982d19371f6aa58f7 |
| v1.4.0 | .packed.bin | e26a154932c84d755cf2432f887b755d8790b24087539cd4c7019e435ffc0041 |
| v1.3.8 | .packed.bin | 110727aa2e840557e9e707e0f5c5d9c113fc3968167a87c9b88e9ef74b6906a3 |
| v1.3.7 | .packed.bin | c1cf7295ddaffa29b9e5099e3397cd8d3e99f950223a946f2a9db639446df1a6 |
| v1.3.6 | .packed.bin | 8ed9ddb25d2ef0e188ca27348f5204f8f37f228e1674efac5ac4bd6569e2cf4e |
| v1.3.6 | .bin | e6152f3993c2f679835197971f0dc88d6de24690b37db64ae9f133bd6f7281cf |
| v1.3.5 | .packed.bin | 49a37aaa02d1dfa68d175f907e99c5e5d08816496b82ffef13f83641419f2a1e |
| v1.3.5 | .bin | e445bd1f4ce98ad1988829a5325171fab8f31c45bd3dbdd6332cd91fc601eb88 |
| v1.3.4 | .packed.bin | 6d4f249eec864d5db758bd3093f510e72ba8563ca0bd803286802e8a1a42d0ad |
| v1.3.4 | .bin | a9f6f2730ad62269eb4f4a44a2bd781b5a326a8a2260fb483be41cebf276db2f |
| v1.3.3 | .packed.bin | f6f2468b2ec90200b8e6d96a92006529f827583c69cccb750b2a6a0a864d54da |
| v1.3.3 | .bin | 43e8a69a37831876088ff2ceb55661ef3f4bfca1e015a97b529510a715c7b60e |
| v1.3.2 | .packed.bin | ef36bfefdff83bb10601d7f1ae0e07739f7cae9d537cd249c2dfcaf601bc2b8c |
| v1.3.2 | .bin | 92f8433c8df1d68328ed7051e8efd4b0b5bdacf7ef40be6712f653583a846cf1 |
| v1.3.1 | .packed.bin | 78a90257269c6df95d26f7575a31aa15615b26a1ac829b357a557e52d35e60c5 |
| v1.3.1 | .bin | feb5746a8b903eb1e675441eb579abeab92a9e5f30027d19eaef67f079c3f9a2 |
| v1.3.0 | .packed.bin | 610f49530fb69de69cacd26974b1f94deae16eeb2b927ff221bdec54dee2c608 |
| v1.3.0 | .bin | e1d70aeb3da309a8ed2fa70ebc07220b0679dfd2196b7590bbcfad88e454c701 |
| v1.2.8 | .packed.bin | 0c5c795978c5746411e2836f6d68f32c48fbaa8d4acd8fb4236f427030ee2cb7 |
| v1.2.8 | .bin | 02cfb30792843504f905ea740f8ac7a73e291a85a04f92ef2c75bcbb40c5dd11 |
| v1.2.7 | .packed.bin | 1e1486c27baefce73d817dec45053914a7a128b8f8af92b01c8760bf270740b0 |
| v1.2.6 | .packed.bin | 7a14b55039f2a9de6f22faf858f3c64fcc7d2cf372309d42208f47a375ded884 |
| v1.2.5 | .packed.bin | 2695c154a9277fd0393c514f5e969650741facbda2b2031c4e91a6520a09ee72 |

Files are named `vuurwerk-<version>.bin` and `vuurwerk-<version>.packed.bin`.
Releases not in the table shipped without a recorded checksum.

73 de KC3TFZ
