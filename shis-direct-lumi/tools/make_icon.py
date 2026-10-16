#!/usr/bin/env python3
"""Generate Lumi's mascot icon for the standalone SHIS Direct NRO."""
import math, struct, zlib
from pathlib import Path

W = H = 256
pixels = bytearray()

def ellipse(x, y, cx, cy, rx, ry):
    return ((x-cx)/rx)**2 + ((y-cy)/ry)**2 <= 1

def line_distance(x, y, x1, y1, x2, y2):
    dx, dy = x2-x1, y2-y1
    t = max(0, min(1, ((x-x1)*dx + (y-y1)*dy) / max(1, dx*dx+dy*dy)))
    return math.hypot(x-(x1+t*dx), y-(y1+t*dy))

for y in range(H):
    for x in range(W):
        d = math.hypot(x-120, y-100)
        glow = max(0, 1-d/220)
        c = (int(24+34*glow), int(13+20*glow), int(51+74*glow))

        # Soft rounded lavender portrait backing.
        if ellipse(x,y,119,119,103,103):
            c = (43, 25, 79)
        # Hair behind the face, including pointed cat ears.
        hair = (ellipse(x,y,119,121,70,77) or
                (x>=49 and x<=105 and y>=31 and y<=104 and y>=31+abs(x-76)*0.88) or
                (x>=133 and x<=190 and y>=31 and y<=104 and y>=31+abs(x-161)*0.88))
        if hair:
            c = (185, 145, 255)
        # Ear inner accents.
        if line_distance(x,y,69,53,78,83)<3 or line_distance(x,y,158,83,169,53)<3:
            c = (255, 164, 218)
        # Face.
        if ellipse(x,y,120,129,55,58):
            c = (255, 225, 235)
        # Side locks.
        if ellipse(x,y,70,139,15,48) or ellipse(x,y,169,139,15,48):
            c = (170, 126, 248)
        # Swept fringe across forehead.
        if (y>=63 and y<=103 and x>=70 and x<=165 and
            (y < 72 + 0.27*(x-70) or (x>119 and y < 93-0.42*(x-119)))):
            c = (198, 163, 255)
        # Large violet eyes with white highlights.
        if ellipse(x,y,100,128,10,15) or ellipse(x,y,141,128,10,15):
            c = (67, 39, 112)
        if ellipse(x,y,103,123,3,4) or ellipse(x,y,144,123,3,4):
            c = (255,255,255)
        if ellipse(x,y,96,134,2,2) or ellipse(x,y,137,134,2,2):
            c = (255, 178, 221)
        # Tiny nose and curved smile.
        if ellipse(x,y,120,145,3,2):
            c = (229, 111, 173)
        if 150 <= y <= 159 and abs((y-150)-0.018*(x-120)**2) < 2 and 108 <= x <= 132:
            c = (130, 67, 139)
        # Purple bow/accessory near one ear.
        if ellipse(x,y,174,88,12,8) or ellipse(x,y,190,88,12,8):
            c = (126, 76, 211)
        if ellipse(x,y,182,88,4,5):
            c = (255, 211, 245)
        # Kawaii lemon badge, front-most lower-right.
        if ellipse(x,y,190,190,35,30):
            c = (255, 239, 93)
        if ellipse(x,y,190,190,29,24):
            c = (255, 221, 53)
        if line_distance(x,y,176,176,202,202)<2 or line_distance(x,y,204,176,177,203)<2:
            c = (255, 250, 190)
        if ellipse(x,y,207,160,8,4):
            c = (137, 213, 92)
        if line_distance(x,y,207,160,213,151)<2:
            c = (137, 213, 92)
        pixels.extend((*c,255))

def chunk(kind, data):
    return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)

raw = b"".join(b"\x00"+bytes(pixels[y*W*4:(y+1)*W*4]) for y in range(H))
png = b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",W,H,8,6,0,0,0))+chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b"")
out = Path(__file__).resolve().parents[1]/"data"/"icon.png"
out.parent.mkdir(parents=True,exist_ok=True)
out.write_bytes(png)
print(f"Generated {out} ({W}x{H})")
