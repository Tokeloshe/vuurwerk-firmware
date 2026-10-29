# SPDX-License-Identifier: Apache-2.0
"""A small screenplay language on top of the harness.

A Film collects steps in simulated time and asks the harness to capture the
LCD at a steady frame rate, so the result is a screen recording of the real
firmware. Beside the screen it records what the viewer should see around it:
which key is pressed when, captions, and chapter marks. Simulated time and
film time run 1:1, except inside `skip(ms)`, which lets the radio run
without the film watching (for long waits such as a scan settling)."""
import eeprom
import frames


class Film:
    def __init__(self, fps=10, image=None):
        self.fps = fps
        self.lines = [(image or eeprom.film_image()).commands()]
        self.sim = 0            # ms, simulated
        self.t = 0.0            # s, film time
        self.captures = []      # (sim_ms, film_t)
        self.keys = []          # (film_t, key, hold_s)
        self.captions = []      # (film_t0, film_t1, kind, text...)
        self.chapters = []      # (film_t, title)
        self.last_capture = -1
        self.hidden = False

    # ----------------------------------------------------------- script
    def raw(self, s):
        self.lines.append(s)

    def wait(self, ms):
        """Let the radio run for ms, capturing frames at the film rate."""
        step = int(1000 / self.fps)
        end = self.sim + ms
        while self.sim < end:
            d = min(step, end - self.sim)
            self.lines.append(f"wait {d}")
            self.sim += d
            if not self.hidden:
                self.t += d / 1000
                self.lines.append(f"frame f{len(self.captures)}")
                self.captures.append((self.sim, self.t))
        return self

    def skip(self, ms):
        """Let the radio run without the film watching."""
        self.lines.append(f"wait {ms}")
        self.sim += ms
        return self

    def key(self, name, hold_ms=150, label=None):
        self.lines.append(f"key {name} {hold_ms}")
        self.keys.append((self.t, name, hold_ms / 1000, label))
        return self

    def press(self, name, hold_ms=150, settle_ms=400, label=None):
        """Press a key and watch what the radio does for settle_ms."""
        self.key(name, hold_ms, label)
        return self.wait(settle_ms)

    def ptt(self, down):
        self.lines.append(f"ptt {'down' if down else 'up'}")
        self.keys.append((self.t, "PTT", 0.3 if down else 0, "PTT" if down else None))
        return self

    def signal(self, hz, rssi, noise, half_bw=6000, tone=False):
        """A signal on the air. The radio's squelch opens by itself when it is
        tuned there; with tone, its CTCSS decoder locks as well."""
        self.lines.append(f"signal {hz} {rssi} {noise} {half_bw}{' tone' if tone else ''}")
        return self

    def off(self, hz):
        self.lines.append(f"off {hz}")
        return self

    def sql(self, open_):
        self.lines.append(f"sql {'open' if open_ else 'close'}")
        return self

    def tone(self, on):
        self.lines.append(f"tone {'on' if on else 'off'}")
        return self

    def talk(self, hz, ms, rssi=196, noise=18, gaps=True, tone=True):
        """Someone talking on hz: a few overs with the carrier dropping
        between them. If the radio is tuned there its squelch opens on its own."""
        left = ms
        seg = [1800, 500, 1400, 600, 1100, 500]
        i = 0
        while left > 0:
            d = min(seg[i % len(seg)], left)
            if gaps and i % 2 == 1:
                self.off(hz)
            else:
                self.signal(hz, rssi, noise, tone=tone)
            self.wait(d)
            left -= d
            i += 1
        self.off(hz)
        return self

    # --------------------------------------------------------- metadata
    def caption(self, text, dur=None, kind="say"):
        """Narration shown from now; the next caption of the same kind replaces it."""
        self.captions.append([self.t, None, kind, text])
        if dur:
            self.captions[-1][1] = self.t + dur
        return self

    def chapter(self, title):
        self.chapters.append((self.t, title))
        return self

    # --------------------------------------------------------------- run
    def render(self):
        self.lines.append("quit")
        fr, order = frames.run("\n".join(self.lines) + "\n")
        shots = []
        prev = None
        for i, (sim_ms, ft) in enumerate(self.captures):
            rows = fr[f"f{i}"][0]
            key = bytes(b"".join(rows))
            if key != prev:
                shots.append((ft, rows))
                prev = key
        return shots

    def finish_captions(self, end):
        by_kind = {}
        for c in self.captions:
            k = c[2]
            if k in by_kind and by_kind[k][1] is None:
                by_kind[k][1] = c[0]
            by_kind[k] = c
        for c in self.captions:
            if c[1] is None:
                c[1] = end
        return self.captions
