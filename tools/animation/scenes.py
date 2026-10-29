# SPDX-License-Identifier: Apache-2.0
"""The screenplays. Each returns a Film whose script the harness then acts
out on the real firmware. Signals on the air are staged (the frequencies and
strengths are invented); everything the radio does with them is its own."""
from film import Film

V_A = 146520000      # VFO A
V_CALL = 147070000   # a second voice up the band
V_PATROL_1 = 148020000   # voices PATROL will find, on 25 kHz bins from the range start
V_PATROL_2 = 146520000
V_SCAN = 147270000       # the scan climbs from 147.070 and stops here
V_CARRIER = 146640000


def boot(f):
    f.raw("batt 2550")
    f.skip(900)
    f.wait(1400)          # the welcome screen
    f.skip(2200)          # and the rest of the 2.55 s boot, unwatched
    f.wait(300)


def hero():
    f = Film(fps=8)
    f.description = ("A UV-K5 running VUURWERK v1.5.1, every screen drawn by the firmware itself: the home "
                     "screen with its RX line and signal-quality meter, the voice-seeking spectrum that only "
                     "draws bars for human voice, locking a voice with PTT and carrying it home with EXIT, "
                     "PATROL walking a range by itself, and a scan that stops on a busy channel.")
    f.chapter("power on")
    f.caption("Afrikaans for fireworks. Custom firmware for the $30 Quansheng UV-K5.", kind="say")
    boot(f)
    f.caption("Two VFOs, the band or channel under each, tone and power, and a context line that follows what the radio is doing.", kind="say")
    f.caption("F3 · 146.520 FM, CTCSS, high power   ·   M12 · memory 12   ·   10 kHz step, wide, FM", kind="note")
    f.wait(3200)

    f.chapter("a signal comes in")
    f.caption("Someone keys up on 146.520. The RX line shows S-level and dBm, and Q3: how steady the signal is, not just how strong.", kind="say")
    f.caption("Q3 steady and clean · Q2 easy copy · Q1 noisy · Q0 breaking up", kind="note")
    f.talk(V_A, 5200, rssi=196, noise=18)
    f.wait(1200)

    f.chapter("the voice-seeking spectrum")
    f.caption("Press F, then 5. The spectrum opens around your VFO: 64 steps of 25 kHz. A data carrier is on the air at 146.645, and NORM mode parks on it: it is the strongest thing there.", kind="say")
    f.caption("F is the # key · STAR is the * key", kind="note")
    f.signal(V_CARRIER, 150, 215)         # a data carrier: strong, but the demodulator hears only noise
    f.press("F", settle_ms=350, label="F")
    f.press("5", settle_ms=3000, label="F + 5")
    f.caption("STAR cycles the mode. In VOX only bins that sound like a human voice get a bar. The carrier becomes a dot and the sweep carries on past it.", kind="say")
    f.press("STAR", settle_ms=500, label="STAR")
    f.press("STAR", settle_ms=2700, label="STAR  →  VOX")   # 2700: both voices key up inside one sweep
    f.caption("a dot per bin · half bar: probable voice · full bar: strong voice", kind="note")
    f.caption("Two people key up. Both get bars, and the radio locks onto the stronger one by itself.", kind="say")
    f.signal(V_A, 196, 18, tone=True)
    f.signal(V_CALL, 176, 26)
    f.wait(3000)
    f.caption("UP hops to the next voice up the band. PTT locks on. EXIT carries that frequency home to your VFO.", kind="say")
    f.press("UP", settle_ms=1800, label="UP")
    f.ptt(True)
    f.wait(300)
    f.ptt(False)
    f.caption("LOCKED: it stays there until you press UP, DOWN, STAR or MENU", kind="note")
    f.wait(2000)
    f.off(V_CALL)
    f.off(V_A)
    f.wait(400)
    f.press("EXIT", settle_ms=400, label="EXIT")
    f.caption("147.070 is now VFO A", kind="note")
    f.wait(2400)

    f.chapter("patrol")
    f.caption("New in 1.5.1. PATROL is that hunt with the radio holding the keys: give it a range and it walks it window by window, stops on voices, and moves on by itself.", kind="say")
    f.caption("F+5, STAR, STAR to VOX · 2 sets the low edge · 8 sets the high edge · 0 starts", kind="note")
    f.press("F", settle_ms=300, label="F")
    f.press("5", settle_ms=1600, label="F + 5")
    f.press("STAR", settle_ms=350, label="STAR")
    f.press("STAR", settle_ms=800, label="STAR  →  VOX")
    f.press("2", settle_ms=1300, label="2  ·  SET LO")
    f.press("SIDE1", settle_ms=700, label="SIDE 1")
    f.press("SIDE1", settle_ms=700, label="SIDE 1")
    f.press("8", settle_ms=1300, label="8  ·  SET HI")
    f.press("0", settle_ms=1200, label="0  ·  PATROL")
    f.caption("PTRL in the corner. It sweeps the range, camps on a voice while it lasts, and when the talking stops it moves on.", kind="say")
    f.caption("sweep → camp → resume · a station that never stops is skipped after 60 s", kind="note")
    f.wait(1800)
    f.talk(V_PATROL_1, 3600, rssi=170, noise=22)
    f.wait(2200)
    f.talk(V_PATROL_2, 3200, rssi=190, noise=20)
    f.wait(1800)
    f.press("0", settle_ms=600, label="0  ·  stop")
    f.press("EXIT", settle_ms=800, label="EXIT")

    f.chapter("scan")
    f.caption("Hold STAR to scan. In VFO mode it climbs the band a step at a time; empty frequencies are left quickly, and the line shows how fast it is moving.", kind="say")
    f.caption("S in the status bar · ScnRev in Menu > SCAN picks what happens after a stop · in memory mode it scans your channels", kind="note")
    f.signal(V_SCAN, 184, 22, tone=True)      # someone is already talking a little way up the band
    f.press("STAR", hold_ms=1100, settle_ms=2800, label="hold STAR  ·  SCAN ON")
    f.caption("A voice on 147.270 stops it. The RX line comes back with S-level, dBm and Q. When the carrier drops, ScnRev decides how soon it moves on.", kind="say")
    f.wait(2600)
    f.off(V_SCAN)
    f.wait(2400)
    f.press("STAR", hold_ms=1100, settle_ms=1400, label="hold STAR  ·  SCAN OFF")
    f.caption("Flash it the usual way. Your channels and settings survive the upgrade.", kind="say")
    f.caption("FLASHING.md · FEATURES.md · USER_MANUAL.md", kind="note")
    f.wait(2600)
    return f


def guide():
    f = hero()
    return f
