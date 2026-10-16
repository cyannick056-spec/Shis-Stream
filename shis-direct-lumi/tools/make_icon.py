#!/usr/bin/env python3
"""Generate a small, dependency-free PNG icon for the SHIS Direct Lumi NRO."""
import math, struct, zlib
from pathlib import Path

W = H = 256
pixels = bytearray()
for y in range(H):
    for x in range(W):
        # Deep violet background with a soft radial glow.
        d = math.sqrt((x-128)**2 + (y-112)**2)
        glow = max(0, 1-d/190)
        r = int(24 + 45*glow)
        g = int(12 + 20*glow)
        b = int(48 + 90*glow)
        # Lime slice badge in the lower-right corner.
        dx, dy = x-185, y-183
        rr = math.sqrt(dx*dx + dy*dy)
        if rr < 43:
            r, g, b = (174, 239, 68) if rr < 36 else (235, 255, 205)
            if rr < 36:
                # Lime segment spokes.
                angle = math.atan2(dy, dx)
                if min(abs(math.sin(angle)), abs(math.cos(angle))) < 0.045:
                    r, g, b = 242, 255, 210
        # Cute cat ears/head silhouette, deliberately simple at icon scale.
        inside_head = ((x-116)/63)**2 + ((y-119)/58)**2 <= 1
        left_ear = x >= 65 and x <= 105 and y >= 42 and y <= 92 and (y-42) >= abs(x-85)*0.65
        right_ear = x >= 127 and x <= 167 and y >= 42 and y <= 92 and (y-42) >= abs(x-147)*0.65
        if inside_head or left_ear or right_ear:
            r, g, b = 224, 199, 255
        # Eyes and smile.
        if ((x-96)**2 + (y-120)**2 < 20 or (x-137)**2 + (y-120)**2 < 20):
            r, g, b = 76, 40, 120
        if (x-116)**2 + (y-145)**2 < 11:
            r, g, b = 255, 135, 190
        pixels.extend((r, g, b, 255))

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind+data) & 0xffffffff)

raw = b"".join(b"\x00" + bytes(pixels[y*W*4:(y+1)*W*4]) for y in range(H))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
out = Path(__file__).resolve().parents[1] / "data" / "icon.png"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(png)
print(f"Generated {out} ({W}x{H})")
