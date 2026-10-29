#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build the VUURWERK animations.

    python3 tools/animation/make_animation.py            # both films
    python3 tools/animation/make_animation.py hero       # the README film only

Every screen in the films is drawn by the firmware itself: tools/animation/host
compiles the UI and app code for the PC, the screenplay in scenes.py presses
the keys and puts signals on the air, and the LCD is captured frame by frame
(see film.py). The radio drawing around the screen is an original
illustration. Output is a self-contained SVG driven by CSS keyframes (no
script), so GitHub plays it inline in the README.

Outputs: docs/media/vuurwerk-hero.svg and docs/media/vuurwerk-guide.svg, plus
site/index.html (the guide with a player) from tools/animation/page.html.
"""
import os
import random
import re
import sys

import frames
import scenes

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MEDIA = os.path.join(ROOT, "docs", "media")
W, H = 1200, 675

# --------------------------------------------------------------- style
BG, BG2 = "#17130F", "#0E0B09"
BODY, BODY2, BEZEL = "#2B2622", "#1F1B18", "#141110"
KEY, KEY2, LEGEND = "#3B3531", "#2E2925", "#E8DECB"
INK, CREAM, DIM, FAINT = "#F2EADB", "#E8DECB", "#8E8273", "#3A332C"
AMBER, RED, TEAL = "#F2A33A", "#E0553D", "#5BC8B4"
# the UV-K5 panel: a cool white backlight by default; VUURWERK_LCD=green for
# the yellow-green backlight some units ship with
if os.environ.get("VUURWERK_LCD") == "green":
    LCD_FG, LCD_BG, LCD_GLOW = (0x1A, 0x24, 0x14), (0xC9, 0xE0, 0x8F), "#9FD64A"
else:
    LCD_FG, LCD_BG, LCD_GLOW = (0x16, 0x1E, 0x2C), (0xD3, 0xE3, 0xF0), "#7FB8E8"
MONO = "ui-monospace,'SF Mono',SFMono-Regular,Menlo,Consolas,'Liberation Mono',monospace"
SANS = "ui-sans-serif,-apple-system,'Segoe UI',Helvetica,Arial,sans-serif"

# radio geometry
RX, RY, RW, RH = 70, 26, 470, 640          # body
LX, LY, LS = 112, 60, 3                     # LCD origin and pixel scale (384 x 192)
KEYS = {}                                   # name -> (x, y, w, h, legend, sub)


def layout_keys():
    top = LY + 64 * LS + 34
    names = [("MENU", "MENU", ""), ("UP", "▲", ""), ("DOWN", "▼", ""), ("EXIT", "EXIT", "")]
    kw, kh, gap = 94, 40, 12
    x0 = RX + (RW - (4 * kw + 3 * gap)) / 2
    for i, (n, leg, sub) in enumerate(names):
        KEYS[n] = (x0 + i * (kw + gap), top, kw, kh, leg, sub)
    rows = [[("1", "1", "BAND"), ("2", "2", "A/B"), ("3", "3", "VFO/MR")],
            [("4", "4", "SQL"), ("5", "5", "SCOPE"), ("6", "6", "PWR")],
            [("7", "7", "SCOPE"), ("8", "8", "REV"), ("9", "9", "CALL")],
            [("STAR", "∗", "SCAN"), ("0", "0", "MOD"), ("F", "#", "F  LOCK")]]
    kw, kh, gap = 128, 50, 14
    x0 = RX + (RW - (3 * kw + 2 * gap)) / 2
    y = top + 40 + 18
    for r, row in enumerate(rows):
        for c, (n, leg, sub) in enumerate(row):
            KEYS[n] = (x0 + c * (kw + gap), y + r * (kh + gap), kw, kh, leg, sub)
    # side keys on the left edge
    KEYS["PTT"] = (RX - 22, LY + 150, 22, 150, "PTT", "")
    KEYS["SIDE1"] = (RX - 18, LY + 318, 18, 56, "S1", "")
    KEYS["SIDE2"] = (RX - 18, LY + 386, 18, 56, "S2", "")


layout_keys()

# ------------------------------------------------------- css keyframes
CSS = []
_n = [0]
T = [60.0]


def f(x):
    s = f"{x:.2f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def anim(points, prop):
    _n[0] += 1
    name = f"a{_n[0]}"
    T_ = T[0]
    pts = sorted(points, key=lambda p: p[0])
    pre = [p for p in pts if p[0] <= 0]
    pts = [p for p in pts if 0 < p[0] < T_]
    pts.insert(0, (0.0, pre[-1][1] if pre else pts[0][1]))
    pts.append((T_, pts[-1][1]))
    out, last = [], -1.0
    for t, v in pts:
        pct = max(t / T_ * 100, last + 0.001)
        if pct > 100:
            continue
        last = pct
        out.append(f'{f"{pct:.3f}".rstrip("0").rstrip(".")}%{{{prop}:{v}}}')
    CSS.append(f"@keyframes {name}{{{''.join(out)}}}.{name}{{animation:{name} {f(T_)}s linear infinite}}")
    return name


def fade(t0, t1, d=.35, peak=1, d_out=None):
    d_out = d if d_out is None else d_out
    span = max(t1 - t0, .02)
    if d + d_out > span:                     # a short life: fade in and out within it
        k = span / (d + d_out)
        d, d_out = d * k, d_out * k
    return anim([(t0, 0), (t0 + d, peak), (t1 - d_out, peak), (t1, 0)], "opacity")


def show(t0, t1):
    return anim([(t0 - .01, 0), (t0, 1), (t1 - .01, 1), (t1, 0)], "opacity")


def combos(svg):
    seen, rules = set(), []
    for m in re.finditer(r'class="((?:a\d+ )+a\d+)"', svg):
        names = m.group(1).split()
        key = " ".join(names)
        if key not in seen:
            seen.add(key)
            rules.append("." + ".".join(names) + "{animation:" +
                         ",".join(f"{n} {f(T[0])}s linear infinite" for n in names) + "}")
    return "".join(rules)


def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=13, fill=INK, family=MONO, weight=400, anchor="start", ls=0, cls="", extra=""):
    c = f' class="{cls}"' if cls else ""
    l = f' letter-spacing="{ls}"' if ls else ""
    return (f'<text x="{f(x)}" y="{f(y)}" font-family="{family}" font-size="{size}" font-weight="{weight}" '
            f'fill="{fill}" text-anchor="{anchor}"{l}{c}{extra}>{esc(s)}</text>')


def wrap(s, width):
    words, lines, cur = s.split(), [], ""
    for w in words:
        if len(cur) + len(w) + 1 > width and cur:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w).strip()
    if cur:
        lines.append(cur)
    return lines


# --------------------------------------------------------------- radio
def radio_art(A):
    A(f'<g id="radio">')
    # antenna and knob
    A(f'<rect x="{RX + 36}" y="{RY - 22}" width="18" height="30" rx="5" fill="{BODY2}"/>')
    A(f'<rect x="{RX + 41}" y="{RY - 160}" width="8" height="140" rx="4" fill="#1A1613" stroke="#2F2A25"/>')
    A(f'<rect x="{RX + RW - 70}" y="{RY - 26}" width="40" height="34" rx="6" fill="#1D1915" stroke="#3A332C"/>')
    for i in range(6):
        A(f'<line x1="{RX + RW - 66 + i * 6.4}" y1="{RY - 22}" x2="{RX + RW - 66 + i * 6.4}" y2="{RY + 4}" stroke="#2E2924" stroke-width="2"/>')
    # body
    A(f'<rect x="{RX}" y="{RY}" width="{RW}" height="{RH}" rx="26" fill="url(#bodyGrad)" stroke="#3A332C" stroke-width="1.5"/>')
    A(f'<rect x="{RX + 10}" y="{RY + 10}" width="{RW - 20}" height="{RH - 20}" rx="20" fill="none" stroke="#231F1B"/>')
    # speaker grille
    gx, gy = RX + 30, LY + 64 * LS + 14
    for i in range(0, 44, 4):
        A(f'<line x1="{gx + i}" y1="{gy - 2}" x2="{gx + i}" y2="{gy + 8}" stroke="#1A1613" stroke-width="2"/>')
    # LED
    A(f'<circle cx="{RX + RW - 36}" cy="{LY - 14}" r="4" fill="#3A2A22"/>')
    # display bezel and glass
    A(f'<rect x="{LX - 18}" y="{LY - 18}" width="{128 * LS + 36}" height="{64 * LS + 36}" rx="10" fill="{BEZEL}"/>')
    A(f'<rect x="{LX - 6}" y="{LY - 6}" width="{128 * LS + 12}" height="{64 * LS + 12}" rx="4" fill="#0B0E13"/>')
    # keys
    for n, (x, y, w, h, leg, sub) in KEYS.items():
        side = n in ("PTT", "SIDE1", "SIDE2")
        A(f'<rect x="{f(x)}" y="{f(y)}" width="{w}" height="{h}" rx="{8 if not side else 6}" fill="url(#keyGrad)" stroke="#1A1613"/>')
        if side:
            A(text(x + w / 2, y + h / 2 + 4, leg, 9, DIM, SANS, 700, "middle", ls=1,
                   extra=f' transform="rotate(-90 {f(x + w / 2)} {f(y + h / 2)})"'))
        else:
            big = n in ("UP", "DOWN")
            A(text(x + w / 2, y + h / 2 + (6 if not sub else 2), leg, 18 if not big else 13, LEGEND, SANS, 600, "middle"))
            if sub:
                A(text(x + w / 2, y + h - 8, sub, 7.5, DIM, SANS, 600, "middle", ls=1.2))
    A('</g>')


def key_flash(A, name, t0, dur, label=None):
    x, y, w, h, leg, sub = KEYS[name]
    hold = max(dur, .25)
    op = anim([(t0 - .01, 0), (t0, 1), (t0 + hold, 1), (t0 + hold + .35, 0)], "opacity")
    A(f'<rect x="{f(x)}" y="{f(y)}" width="{w}" height="{h}" rx="8" fill="{AMBER}" fill-opacity=".28" '
      f'stroke="{AMBER}" stroke-width="2" class="{op}" opacity="0"/>')
    # ring that expands from the key
    sc = anim([(t0 - .01, "scale(.6)"), (t0, "scale(.6)"), (t0 + .5, "scale(1.25)")], "transform")
    ro = anim([(t0 - .01, 0), (t0, .9), (t0 + .5, 0)], "opacity")
    A(f'<rect x="{f(x - 6)}" y="{f(y - 6)}" width="{w + 12}" height="{h + 12}" rx="12" fill="none" stroke="{AMBER}" '
      f'stroke-width="1.5" style="transform-box:fill-box;transform-origin:center" class="{sc} {ro}" opacity="0"/>')


# ------------------------------------------------------------- the film
def build(fm, shots, title, subtitle, end_lines, loop_tail=2.6):
    """fm: the Film (metadata); shots: [(t, rows)]."""
    T[0] = fm.t + loop_tail
    out = []
    A = out.append
    A(f'<rect width="{W}" height="{H}" fill="url(#bg)"/>')
    rnd = random.Random(467)
    A('<g>')
    for _ in range(260):
        A(f'<rect x="{f(rnd.uniform(0, W))}" y="{f(rnd.uniform(0, H))}" width="1" height="1" fill="#F2EADB" opacity="{f(rnd.uniform(.03, .11))}"/>')
    A('</g>')
    # bench: a soft pool of light under the radio
    A(f'<ellipse cx="{RX + RW / 2}" cy="{RY + RH - 10}" rx="330" ry="40" fill="url(#pool)"/>')
    radio_art(A)

    # the screen: backlight glow, then the frames
    A(f'<rect x="{LX - 30}" y="{LY - 30}" width="{128 * LS + 60}" height="{64 * LS + 60}" rx="18" fill="url(#glow)" opacity=".55"/>')
    A(f'<rect x="{LX}" y="{LY}" width="{128 * LS}" height="{64 * LS}" fill="rgb({LCD_BG[0]},{LCD_BG[1]},{LCD_BG[2]})"/>')
    for i, (t0, rows) in enumerate(shots):
        t1 = shots[i + 1][0] if i + 1 < len(shots) else T[0]
        uri = frames.data_uri(rows, fg=LCD_FG, bg=LCD_BG)
        A(f'<image x="{LX}" y="{LY}" width="{128 * LS}" height="{64 * LS}" href="{uri}" '
          f'style="image-rendering:pixelated;image-rendering:crisp-edges" class="{show(t0, t1)}" opacity="0"/>')
    # a faint pixel grid and glass reflection over the panel
    A(f'<rect x="{LX}" y="{LY}" width="{128 * LS}" height="{64 * LS}" fill="url(#grid)" opacity=".18"/>')
    A(f'<rect x="{LX}" y="{LY}" width="{128 * LS}" height="{64 * LS}" fill="url(#glass)" opacity=".35"/>')

    for t0, name, hold, label in fm.keys:
        key_flash(A, name, t0, hold, label)

    # ---------------------------------------------------- the right page
    PX = 590
    A(text(PX, 66, title.upper(), 13, DIM, MONO, 600, ls=4))
    A(f'<line x1="{PX}" y1="80" x2="1150" y2="80" stroke="{FAINT}"/>')
    # chapter stamps
    for i, (t0, ch) in enumerate(fm.chapters):
        t1 = fm.chapters[i + 1][0] if i + 1 < len(fm.chapters) else T[0]
        A(f'<g class="{fade(t0, t1, .3)}" opacity="0">'
          f'<g transform="rotate(-3 {PX + 8} 118)">'
          f'<rect x="{PX}" y="96" width="{18 + len(ch) * 13.2}" height="34" rx="4" fill="none" stroke="{AMBER}" stroke-width="2"/>'
          + text(PX + 9 + len(ch) * 6.6, 120, ch.upper(), 18, AMBER, MONO, 700, "middle", ls=2.4) + '</g></g>')
    # key callouts: what was pressed, large
    labelled = [k for k in fm.keys if k[3]]
    for i, (t0, name, hold, label) in enumerate(labelled):
        t1 = t0 + max(hold, .3) + 1.6
        if i + 1 < len(labelled):
            t1 = min(t1, labelled[i + 1][0] - .05)   # the next callout takes its place
        A(f'<g class="{fade(t0, t1, .12, 1, .5)}" opacity="0">'
          + text(1150, 120, label, 30, INK, SANS, 300, "end", ls=1) + '</g>')
    # captions: "say" lines, wrapped
    caps = fm.finish_captions(T[0])
    for t0, t1, kind, s in caps:
        y0 = 190 if kind == "say" else 470
        size = 18 if kind == "say" else 13.5
        col = CREAM if kind == "say" else DIM
        lines = wrap(s, 44 if kind == "say" else 58)
        A(f'<g class="{fade(t0, t1, .3)}" opacity="0">')
        for k, ln in enumerate(lines):
            A(text(PX, y0 + k * (size + 9), ln, size, col, MONO if kind != "say" else SANS, 400 if kind != "say" else 300))
        A('</g>')
    # footer
    A(text(PX, 640, subtitle, 11, DIM, MONO, 400, ls=1.2))
    A(text(1150, 640, "73 de KC3TFZ", 11, DIM, MONO, 400, "end", ls=1.2))

    # end card
    E0 = fm.t + .2
    A(f'<g class="{fade(E0, T[0] - .01, .5, 1, .3)}" opacity="0"><rect width="{W}" height="{H}" fill="{BG2}" opacity=".93"/>')
    for k, (s, size, col) in enumerate(end_lines):
        A(text(600, 300 + k * 44, s, size, col, SANS if k == 0 else MONO, 200 if k == 0 else 400, "middle", ls=6 if k == 0 else 1.5))
    A('</g>')
    # progress hairline
    A(f'<rect x="-1200" y="673" width="1200" height="2" fill="{AMBER}" opacity=".6" '
      f'class="{anim([(0, "translateX(0px)"), (T[0], "translateX(1200px)")], "transform")}"/>')

    defs = f'''<defs>
<radialGradient id="bg" cx="38%" cy="40%" r="90%"><stop offset="0" stop-color="#241E18"/><stop offset=".5" stop-color="{BG}"/><stop offset="1" stop-color="{BG2}"/></radialGradient>
<radialGradient id="pool" cx="50%" cy="50%" r="50%"><stop offset="0" stop-color="#F2A33A" stop-opacity=".14"/><stop offset="1" stop-color="#F2A33A" stop-opacity="0"/></radialGradient>
<linearGradient id="bodyGrad" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#332D28"/><stop offset=".5" stop-color="{BODY}"/><stop offset="1" stop-color="{BODY2}"/></linearGradient>
<linearGradient id="keyGrad" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#443D37"/><stop offset="1" stop-color="{KEY2}"/></linearGradient>
<radialGradient id="glow" cx="50%" cy="50%" r="60%"><stop offset="0" stop-color="{LCD_GLOW}" stop-opacity=".55"/><stop offset=".7" stop-color="{LCD_GLOW}" stop-opacity=".12"/><stop offset="1" stop-color="{LCD_GLOW}" stop-opacity="0"/></radialGradient>
<linearGradient id="glass" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#FFFFFF" stop-opacity=".16"/><stop offset=".35" stop-color="#FFFFFF" stop-opacity="0"/><stop offset="1" stop-color="#FFFFFF" stop-opacity=".05"/></linearGradient>
<pattern id="grid" width="{LS}" height="{LS}" patternUnits="userSpaceOnUse"><rect width="{LS}" height="{LS}" fill="none"/><path d="M0,0H{LS}M0,0V{LS}" stroke="#7A8EA3" stroke-width=".4"/></pattern>
<clipPath id="frame"><rect width="{W}" height="{H}" rx="14"/></clipPath>
</defs>'''
    body = "".join(out)
    style = "<style>" + "".join(CSS) + combos(body) + "</style>"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
            f'aria-labelledby="vwTitle vwDesc"><title id="vwTitle">{esc(title)}</title>'
            f'<desc id="vwDesc">{esc(fm.description)}</desc>{defs}{style}<g clip-path="url(#frame)">{body}</g></svg>')


def make(name, fm, title, subtitle, end_lines):
    CSS.clear()
    _n[0] = 0
    shots = fm.render()
    svg = build(fm, shots, title, subtitle, end_lines)
    os.makedirs(MEDIA, exist_ok=True)
    path = os.path.join(MEDIA, f"vuurwerk-{name}.svg")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(svg)
    print(f"wrote {os.path.relpath(path, ROOT)}: {len(shots)} frames, {fm.t:.1f} s, {len(svg) / 1024:.0f} KB", file=sys.stderr)
    return svg, shots


if __name__ == "__main__":
    which = sys.argv[1:] or ["hero", "guide"]
    if "hero" in which:
        fm = scenes.hero()
        make("hero", fm, "VUURWERK v1.5.1", "custom firmware for the Quansheng UV-K5",
             [("VUURWERK", 46, INK), ("Afrikaans for fireworks", 14, DIM),
              ("github.com/Tokeloshe/vuurwerk-firmware", 12, DIM)])
    if "guide" in which:
        fm = scenes.guide()
        make("guide", fm, "VUURWERK v1.5.1  ·  the guide", "custom firmware for the Quansheng UV-K5",
             [("VUURWERK", 46, INK), ("every screen in this film was drawn by the firmware", 14, DIM),
              ("github.com/Tokeloshe/vuurwerk-firmware", 12, DIM)])
