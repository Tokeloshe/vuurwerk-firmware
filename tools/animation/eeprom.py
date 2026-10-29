# SPDX-License-Identifier: Apache-2.0
"""Builds the EEPROM image the harness boots from: a stock-like calibration
block (so the boot health check passes and the meters have references), the
settings the film needs, VFO A on 146.520, and a few memory channels."""
import struct

CTCSS_HZ = [67.0, 69.3, 71.9, 74.4, 77.0, 79.7, 82.5, 85.4, 88.5, 91.5, 94.8, 97.4, 100.0, 103.5, 107.2,
            110.9, 114.8, 118.8, 123.0, 127.3, 131.8, 136.5, 141.3, 146.2, 151.4, 156.7, 159.8, 162.2,
            165.5, 167.9, 171.3, 173.8, 177.3, 179.9, 183.5, 186.2, 189.9, 192.8, 196.6, 199.5, 203.5,
            206.5, 210.7, 218.1, 225.7, 229.1, 233.6, 241.8, 250.3, 254.1]
FREQ_CHANNEL_FIRST = 200
BANDS = [(5000000, 10800000), (10800000, 13700000), (13700000, 17400000), (17400000, 35000000),
         (35000000, 40000000), (40000000, 47000000), (47000000, 130000000)]   # 10 Hz units


def band_of(f10):
    for i, (lo, hi) in enumerate(BANDS):
        if lo <= f10 < hi:
            return i
    return 6


class Image:
    def __init__(self):
        self.b = bytearray(b"\xFF" * 0x2000)

    def put(self, addr, data):
        self.b[addr:addr + len(data)] = data

    def calibration(self):
        # squelch thresholds, band 4-7 then band 1-3: ten levels each
        for base in (0x1E00, 0x1E60):
            self.put(base + 0x00, bytes([0, 60, 68, 74, 78, 82, 86, 90, 94, 98]))        # open RSSI
            self.put(base + 0x10, bytes([0, 52, 60, 66, 70, 74, 78, 82, 86, 90]))        # close RSSI
            self.put(base + 0x20, bytes([255, 100, 90, 80, 72, 66, 60, 56, 52, 48]))     # open noise
            self.put(base + 0x30, bytes([255, 110, 100, 90, 82, 76, 70, 66, 62, 58]))    # close noise
            self.put(base + 0x40, bytes([255, 90, 80, 72, 64, 58, 52, 48, 44, 40]))      # close glitch
            self.put(base + 0x50, bytes([255, 80, 70, 62, 54, 48, 42, 38, 34, 30]))      # open glitch
        for base in (0x1EC0, 0x1EC8):                                                    # S-meter refs
            self.put(base, struct.pack("<4H", 70, 80, 100, 130))
        for band in range(7):                                                            # TX power
            self.put(0x1ED0 + band * 16, bytes([50, 55, 60, 80, 85, 90, 120, 125, 130] + [0] * 7))
        self.put(0x1F40, struct.pack("<6H", 1910, 2000, 2200, 2300, 2500, 2600))         # battery
        self.put(0x1F80, bytes([0x25, 0x2C, 0x33, 0x3A, 0x41]))                          # mic level
        self.put(0x1F88, struct.pack("<h", 0))                                           # xtal
        self.put(0x1F8E, bytes([0x3A, 0x08]))                                            # volume, dac
        return self

    def settings(self, sql=1, power_on=1, backlight_time=3, scan_resume=1, mic=3, batt_text=0):
        # 0E70: call chan, squelch, tx timeout, noaa, key lock, vox, vox lvl, mic
        self.put(0x0E70, bytes([0, sql, 1, 0, 0, 0, 1, mic]))
        # 0E78: backlight min/max, ch display mode, crossband, bat save, dual watch, backlight time, STE, vfo open
        self.put(0x0E78, bytes([0x0A, 0, 0, 4, 0, backlight_time, 0, 1]))
        # 0E90: beep | m-long<<1, f1 short, f1 long, f2 short, f2 long, scan resume, auto lock, power-on msg
        self.put(0x0E90, bytes([1, 1, 2, 3, 0, scan_resume, 0, power_on]))
        # 0EA0: voice prompt, S0, S9
        self.put(0x0EA0, bytes([0, 130, 76, 0, 0, 0, 0, 0]))
        # 0EA8: alarm, roger, rp ste, tx vfo, battery type
        self.put(0x0EA8, bytes([0, 0, 0, 0, 1, 0, 0, 0]))
        # 0F40: f lock etc: leave defaults (0xFF -> validated)
        return self

    def vfo(self, which, f10, step=4, mod=0, rx_ctcss_hz=None, power=2, bw=0):
        """VFO `which` (0 = A, 1 = B) on frequency f10 (10 Hz units), FM, wide."""
        band = band_of(f10)
        off = (0x0C80 if which == 0 else 0x0C90) + band * 32
        self.put(off, struct.pack("<II", f10, 0))
        rx_code, rx_type = 0, 0
        if rx_ctcss_hz:
            rx_code, rx_type = CTCSS_HZ.index(rx_ctcss_hz), 1
        self.put(off + 8, bytes([rx_code, 0, (0 << 4) | rx_type, (mod << 4) | 0,
                                 (0 << 4) | (power << 2) | (bw << 1), 0, step, 0]))
        return FREQ_CHANNEL_FIRST + band

    def channel(self, n, f10, name="", step=4, power=2, scanlist1=True, rx_ctcss_hz=None):
        off = n * 16
        self.put(off, struct.pack("<II", f10, 0))
        rx_code, rx_type = 0, 0
        if rx_ctcss_hz:
            rx_code, rx_type = CTCSS_HZ.index(rx_ctcss_hz), 1
        self.put(off + 8, bytes([rx_code, 0, rx_type, 0, power << 2, 0, step, 0]))
        att = band_of(f10) | (0x80 if scanlist1 else 0)
        self.b[0x0D60 + n] = att
        nm = name.encode()[:10].ljust(16, b"\0")
        self.put(0x0F50 + n * 16, nm)
        return self

    def screen(self, a_channel, b_channel, a_mr=0, b_mr=0, a_freq=None, b_freq=None):
        self.put(0x0E80, bytes([a_channel, a_mr, a_freq if a_freq is not None else 205,
                                b_channel, b_mr, b_freq if b_freq is not None else 205, 0, 0]))
        return self

    def commands(self):
        out = []
        for a in range(0, 0x2000, 64):
            chunk = self.b[a:a + 64]
            if chunk != b"\xFF" * 64:
                out.append(f"ee 0x{a:04X} {chunk.hex()}")
        return "\n".join(out)


def film_image():
    """The radio in the film: VFO A 146.520 FM with CTCSS 100.0, VFO B on memory 12 (446.000)."""
    im = Image().calibration().settings()
    a = im.vfo(0, 14652000, step=3, rx_ctcss_hz=100.0)
    b = im.vfo(1, 44600000)
    chans = [(0, 14452000, "W6NUT"), (1, 14520000, "PAPA"), (2, 14672000, "CATALINA"), (3, 14700000, "KEEN"),
             (11, 44600000, "FRS 1"), (12, 44602500, "PMR 2"), (13, 46256250, "FRS 3"), (19, 14639000, "SIMPLEX")]
    for n, f, name in chans:
        im.channel(n, f, name)
    im.screen(a, 11, a_mr=0, b_mr=11, a_freq=a, b_freq=b)
    return im
