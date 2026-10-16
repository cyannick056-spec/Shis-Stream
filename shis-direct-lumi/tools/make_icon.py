#!/usr/bin/env python3
"""Generate a small SHIS Direct Lumi gamer icon using only Python's standard library."""
from pathlib import Path
import math
import struct
import zlib

W = H = 256
pixels = bytearray(W * H * 3)

def blend_pixel(x, y, color):
    if 0 <= x < W and 0 <= y < H:
        i = (y * W + x) * 3
        pixels[i:i+3] = bytes(color)

def ellipse(cx, cy, rx, ry, color, outline=None, width=1):
    x0, x1 = max(0, int(cx-rx-width)), min(W-1, int(cx+rx+width))
    y0, y1 = max(0, int(cy-ry-width)), min(H-1, int(cy+ry+width))
    for y in range(y0, y1+1):
        for x in range(x0, x1+1):
            q = ((x-cx)/max(rx,1))**2 + ((y-cy)/max(ry,1))**2
            if q <= 1:
                if outline and q >= (1-width/max(rx,ry,1))**2:
                    blend_pixel(x,y,outline)
                else:
                    blend_pixel(x,y,color)

def rect(x0,y0,x1,y1,color):
    for y in range(max(0,y0), min(H,y1)):
        for x in range(max(0,x0), min(W,x1)):
            blend_pixel(x,y,color)

def polygon(points, color):
    xs=[p[0] for p in points]; ys=[p[1] for p in points]
    for y in range(max(0,min(ys)), min(H,max(ys)+1)):
        for x in range(max(0,min(xs)), min(W,max(xs)+1)):
            inside=False
            j=len(points)-1
            for i in range(len(points)):
                xi,yi=points[i]; xj,yj=points[j]
                if ((yi>y)!=(yj>y)) and x < (xj-xi)*(y-yi)/(yj-yi+1e-9)+xi:
                    inside=not inside
                j=i
            if inside: blend_pixel(x,y,color)

def line(x0,y0,x1,y1,color,width=2):
    steps=max(abs(x1-x0),abs(y1-y0),1)
    for n in range(steps+1):
        x=round(x0+(x1-x0)*n/steps); y=round(y0+(y1-y0)*n/steps)
        ellipse(x,y,width,width,color)

# Deep purple radial background.
for y in range(H):
    for x in range(W):
        d=math.sqrt((x-128)**2+(y-128)**2)/181
        blend_pixel(x,y,(int(38-16*d),int(17-7*d),int(79-28*d)))
# Neon border.
for x in range(9,247):
    for y in range(9,247):
        if x < 14 or x > 241 or y < 14 or y > 241:
            blend_pixel(x,y,(188,108,255))
# Sparkles.
for x,y,r in [(31,35,5),(220,43,4),(225,147,4),(36,180,3),(182,20,3)]:
    polygon([(x,y-r*2),(x+r//2,y-r//2),(x+r*2,y),(x+r//2,y+r//2),(x,y+r*2),(x-r//2,y+r//2),(x-r*2,y),(x-r//2,y-r//2)],(255,215,255))
# Cat ears and lavender hair.
ellipse(128,103,69,75,(166,119,246),(239,197,255),3)
polygon([(67,91),(54,18),(101,51),(126,42),(191,17),(190,92)],(184,148,255))
polygon([(68,72),(62,32),(91,53)],(255,178,225))
polygon([(173,52),(190,28),(184,75)],(255,178,225))
# Face and hair locks.
ellipse(128,117,54,54,(255,224,237),(111,62,163),2)
ellipse(78,130,16,45,(174,128,246))
ellipse(178,130,16,45,(174,128,246))
polygon([(74,91),(94,57),(128,66),(147,95),(126,111),(109,96),(91,115)],(215,185,255))
polygon([(126,66),(163,57),(184,85),(163,102),(145,92)],(229,202,255))
# Bright playful eyes.
for x in (106,150):
    ellipse(x,119,10,17,(54,30,103))
    ellipse(x,119,6,12,(145,89,236))
    ellipse(x-1,115,4,6,(78,222,206))
    ellipse(x-3,112,2,3,(255,255,255))
ellipse(89,137,8,4,(255,153,201))
ellipse(167,137,8,4,(255,153,201))
ellipse(128,142,3,2,(217,104,168))
line(119,146,128,150,(131,57,137),2); line(128,150,137,146,(131,57,137),2)
# Gamer headset/clip.
rect(165,78,190,91,(37,22,63))
for x,y in [(170,82),(177,86),(184,82)]:
    ellipse(x,y,2,2,(255,175,241))
ellipse(177,97,7,7,(151,82,238),(226,182,255),2)
# Hoodie.
ellipse(128,187,73,44,(39,23,67),(121,72,180),3)
# Purple handheld console.
ellipse(128,207,92,32,(25,17,42),(220,170,255),3)
rect(59,192,197,224,(77,38,128))
ellipse(47,207,15,29,(193,159,255),(244,211,255),2)
ellipse(209,207,15,29,(193,159,255),(244,211,255),2)
ellipse(43,203,6,6,(64,38,100)); ellipse(211,203,6,6,(64,38,100))
for x,y in [(209,214),(215,207),(203,207),(209,200)]:
    ellipse(x,y,2,2,(66,39,105))
# Cat-eared play symbol.
rect(112,198,144,216,(239,177,255))
polygon([(117,201),(122,196),(127,201)],(239,177,255))
polygon([(130,201),(135,196),(140,201)],(239,177,255))
polygon([(122,201),(122,213),(136,207)],(255,218,255))
# Floating chibi cats.
for x,y in [(34,112),(222,112)]:
    ellipse(x,y,13,10,(246,226,255),(174,112,255),2)
    polygon([(x-10,y-3),(x-11,y-15),(x-2,y-8),(x+3,y-8),(x+10,y-15),(x+10,y-3)],(246,226,255))
    ellipse(x-4,y,2,2,(84,39,140)); ellipse(x+4,y,2,2,(84,39,140))
# Write PNG using only stdlib.
def chunk(kind, data):
    body=kind+data
    return struct.pack("!I",len(data))+body+struct.pack("!I",zlib.crc32(body)&0xffffffff)
raw=b"".join(b"\x00"+bytes(pixels[y*W*3:(y+1)*W*3]) for y in range(H))
png=bytes([137])+b"PNG"+bytes([13,10,26,10])+chunk(b"IHDR",struct.pack("!2I5B",W,H,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b"")
out=Path(__file__).resolve().parents[1]/"data"/"icon.png"
out.parent.mkdir(parents=True,exist_ok=True)
out.write_bytes(png)
print(f"Generated {out} (256x256, standard-library Lumi gamer icon)")
