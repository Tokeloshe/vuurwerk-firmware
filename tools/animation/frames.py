# SPDX-License-Identifier: Apache-2.0
"""Run a harness script and return the captured LCD frames.

Each frame is the panel's 8 pages x 128 columns, bit 0 = top pixel of the
page, exactly as the ST7565 holds it; to_png turns one into a PNG."""
import base64
import os
import struct
import subprocess
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.join(HERE, "host", "vuurwerk-host")
W, H = 128, 64


def run(script, timeout=600):
    r = subprocess.run([HOST], input=script, capture_output=True, text=True, timeout=timeout)
    if r.returncode != 0:
        raise RuntimeError(f"harness exit {r.returncode}: {r.stderr[-800:]}")
    frames = {}
    order = []
    lines = r.stdout.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith("frame "):
            _, name, ms = lines[i].split()
            rows = [bytes.fromhex(lines[i + 1 + k]) for k in range(8)]
            frames[name] = (rows, int(ms))
            order.append(name)
            i += 9
        else:
            i += 1
    return frames, order


def pixels(rows):
    px = bytearray(W * H)
    for page in range(8):
        for x in range(W):
            b = rows[page][x]
            for bit in range(8):
                if b >> bit & 1:
                    px[(page * 8 + bit) * W + x] = 1
    return px


def to_png(rows, fg=(0x18, 0x22, 0x30), bg=(0xD6, 0xE6, 0xF2)):
    px = pixels(rows)
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw += bytes(fg if px[y * W + x] else bg)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def data_uri(rows, **kw):
    return "data:image/png;base64," + base64.b64encode(to_png(rows, **kw)).decode()


def ascii(rows):
    px = pixels(rows)
    return "\n".join("".join("#" if px[y * W + x] else "." for x in range(W)) for y in range(H))
