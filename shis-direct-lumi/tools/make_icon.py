#!/usr/bin/env python3
"""Render a compact Lumi gamer mascot icon for the standalone SHIS Direct NRO."""
from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path
import math

S = 512
im = Image.new("RGB", (S, S), (23, 9, 48))
d = ImageDraw.Draw(im, "RGBA")

# Purple neon backdrop and rounded tile.
for r in range(250, 0, -2):
    t = r / 250
    col = (int(52-25*t), int(20-10*t), int(105-48*t), 255)
    d.ellipse((256-r, 256-r, 256+r, 256+r), fill=col)
d.rounded_rectangle((12,12,500,500), radius=82, outline=(219,115,255,255), width=7)
d.rounded_rectangle((23,23,489,489), radius=74, outline=(124,55,215,170), width=3)

# Sparkles and floating kawaii cats.
for x,y,r in [(62,83,13),(446,97,10),(440,290,12),(78,355,9),(367,48,7),(130,450,8)]:
    d.polygon([(x,y-r*2),(x+r*.42,y-r*.42),(x+r*2,y),(x+r*.42,y+r*.42),(x,y+r*2),(x-r*.42,y+r*.42),(x-r*2,y),(x-r*.42,y-r*.42)], fill=(255,205,255,235))
for x,y in [(75,183),(429,210),(390,365)]:
    d.ellipse((x-30,y-24,x+30,y+24), fill=(246,226,255,255), outline=(174,112,255,255), width=3)
    d.polygon([(x-24,y-13),(x-25,y-38),(x-5,y-24),(x+5,y-24),(x+25,y-39),(x+24,y-12)], fill=(246,226,255,255), outline=(174,112,255,255))
    d.ellipse((x-12,y-4,x-5,y+4), fill=(84,39,140,255)); d.ellipse((x+5,y-4,x+12,y+4), fill=(84,39,140,255))
    d.arc((x-8,y+1,x+8,y+12), 5, 175, fill=(196,95,190,255), width=2)
    d.ellipse((x-20,y+3,x-14,y+8), fill=(255,142,202,200)); d.ellipse((x+14,y+3,x+20,y+8), fill=(255,142,202,200))

# Hair silhouette with cat ears.
d.ellipse((113,62,399,354), fill=(165,119,246,255), outline=(240,194,255,255), width=7)
d.polygon([(122,178),(88,43),(205,94),(244,77),(360,37),(393,178)], fill=(179,143,255,255), outline=(239,197,255,255))
d.polygon([(124,151),(111,70),(176,108)], fill=(255,178,225,255))
d.polygon([(353,147),(374,65),(319,108)], fill=(255,178,225,255))
# Side locks and face.
d.ellipse((105,174,174,326), fill=(174,128,246,255))
d.ellipse((342,174,409,326), fill=(174,128,246,255))
d.ellipse((145,124,367,330), fill=(255,224,237,255), outline=(111,62,163,255), width=4)
# Fringe swoops.
d.pieslice((130,91,367,259), 185, 350, fill=(213,181,255,255), outline=(242,211,255,255), width=3)
d.polygon([(148,176),(185,112),(246,129),(278,183),(232,204),(203,179),(172,208)], fill=(215,185,255,255))
d.polygon([(260,127),(330,111),(365,167),(327,190),(289,173)], fill=(229,202,255,255))
# Eyes, highlights and blush.
for x in (208,304):
    d.ellipse((x-27,202,x+27,260), fill=(64,35,119,255))
    d.ellipse((x-18,211,x+17,250), fill=(147,91,239,255))
    d.ellipse((x-14,211,x+12,232), fill=(72,218,204,255))
    d.ellipse((x-10,211,x-1,224), fill=(255,255,255,255))
    d.ellipse((x+8,239,x+15,246), fill=(255,187,239,255))
d.ellipse((162,254,197,269), fill=(255,142,196,100)); d.ellipse((317,254,352,269), fill=(255,142,196,100))
d.ellipse((252,264,261,270), fill=(223,105,168,255))
d.arc((232,266,280,295), 10, 165, fill=(131,57,137,255), width=4)
# Gamer hair clips.
d.rounded_rectangle((326,147,371,181), radius=13, fill=(37,22,63,255), outline=(206,159,255,255), width=3)
for x,y in [(339,158),(350,168),(361,158),(350,155)]:
    d.ellipse((x-3,y-3,x+3,y+3), fill=(255,175,241,255))
d.ellipse((333,186,355,208), fill=(151,82,238,255), outline=(226,182,255,255), width=3)
d.ellipse((348,186,370,208), fill=(151,82,238,255), outline=(226,182,255,255), width=3)
d.ellipse((346,192,357,203), fill=(255,207,247,255))
# Dark oversized hoodie shoulders.
d.rounded_rectangle((112,300,402,414), radius=55, fill=(39,23,67,255), outline=(121,72,180,255), width=5)
d.rounded_rectangle((130,316,204,378), radius=24, fill=(68,42,102,255))
# Handheld console foreground.
d.rounded_rectangle((64,333,448,462), radius=28, fill=(28,20,47,255), outline=(12,8,24,255), width=8)
d.rounded_rectangle((81,348,431,448), radius=12, fill=(76,37,127,255), outline=(199,135,255,255), width=4)
# Purple joycons.
d.rounded_rectangle((51,342,108,453), radius=22, fill=(193,159,255,255), outline=(244,211,255,255), width=4)
d.rounded_rectangle((405,342,462,453), radius=22, fill=(193,159,255,255), outline=(244,211,255,255), width=4)
d.ellipse((67,365,91,389), fill=(64,38,100,255)); d.ellipse((420,366,444,390), fill=(64,38,100,255))
for x,y in [(79,408),(434,407),(421,421),(447,421)]:
    d.ellipse((x-5,y-5,x+5,y+5), fill=(66,39,105,255))
# Glowing cat-eared play logo on screen.
d.rounded_rectangle((193,366,318,433), radius=19, outline=(239,177,255,255), width=5)
d.polygon([(219,375),(231,357),(244,375)], fill=(239,177,255,255))
d.polygon([(267,375),(280,357),(293,375)], fill=(239,177,255,255))
d.polygon([(239,382),(239,416),(268,399)], fill=(255,218,255,255))
# Lemon accent, but subtle.
d.ellipse((362,375,399,412), fill=(255,224,72,255), outline=(255,249,183,255), width=3)
d.ellipse((373,387,380,394), fill=(94,47,133,255)); d.ellipse((384,387,391,394), fill=(94,47,133,255))
d.arc((374,391,391,403), 10, 170, fill=(94,47,133,255), width=2)
# Tiny heart badge.
d.ellipse((377,77,411,111), fill=(255,150,229,255))
d.polygon([(379,94),(394,113),(409,94)], fill=(255,150,229,255))

# Supersample render already 512; export crisp 256x256 NRO icon.
im = im.resize((256,256), Image.Resampling.LANCZOS)
out = Path(__file__).resolve().parents[1] / "data" / "icon.png"
out.parent.mkdir(parents=True, exist_ok=True)
im.save(out, optimize=True)
print(f"Generated {out} (256x256, Lumi gamer icon)")
