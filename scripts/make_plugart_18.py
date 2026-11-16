#!/usr/bin/env python3
"""Procedural faceplate art for the 1.8 instrument shelf (no stock, no credits, all original).
Writes Resources/plugart/pa_<key>.jpg at 900x600, matching the existing plates."""
import math, random, os, sys
from PIL import Image, ImageDraw, ImageFilter

W, H = 900, 600
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Resources', 'plugart')

def base(c0, c1, vignette=0.55):
    img = Image.new('RGB', (W, H), c0)
    px = img.load()
    for y in range(H):
        t = y / (H - 1)
        for x in range(W):
            u = x / (W - 1)
            d = math.hypot(u - 0.5, t - 0.5)
            k = max(0.0, 1.0 - d * d * 2.0 * vignette)
            r = int(c0[0] * (1 - t) + c1[0] * t) ; g = int(c0[1] * (1 - t) + c1[1] * t) ; b = int(c0[2] * (1 - t) + c1[2] * t)
            px[x, y] = (int(r * (0.55 + 0.45 * k)), int(g * (0.55 + 0.45 * k)), int(b * (0.55 + 0.45 * k)))
    return img

def grain(img, amount=10, seed=1):
    rnd = random.Random(seed); px = img.load()
    for y in range(0, H, 2):
        for x in range(0, W, 2):
            n = rnd.randint(-amount, amount)
            r, g, b = px[x, y]
            px[x, y] = (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)))
    return img

def glow(img, draw_fn, radius=18, alpha=0.8):
    layer = Image.new('RGB', (W, H), (0, 0, 0))
    draw_fn(ImageDraw.Draw(layer))
    blurred = layer.filter(ImageFilter.GaussianBlur(radius))
    img = Image.blend(img, Image.composite(img, img, Image.new('L', (W, H), 0)), 0)
    out = img.copy(); opx = out.load(); bpx = blurred.load(); lpx = layer.load()
    for y in range(H):
        for x in range(W):
            r, g, b = opx[x, y]; br, bg, bb = bpx[x, y]; lr, lg, lb = lpx[x, y]
            opx[x, y] = (min(255, r + int(br * alpha) + lr), min(255, g + int(bg * alpha) + lg), min(255, b + int(bb * alpha) + lb))
    return out

def save(img, key):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, f'pa_{key}.jpg'), quality=88)
    print('wrote', key)

# ---------------------------------------------------------------- Mothsynth 2: violet, three interleaved anti-aliased waves + moth silhouette dots
def mothsynth2():
    img = base((28, 10, 52), (8, 4, 20))
    def d(dr):
        for k, (col, f, a) in enumerate([((150, 70, 255), 3.0, 70), ((255, 80, 220), 5.0, 48), ((90, 200, 255), 7.0, 32)]):
            pts = []
            for x in range(0, W, 2):
                t = x / W
                y = H * 0.5 + a * math.sin(t * math.pi * 2 * f + k) * (0.6 + 0.4 * math.sin(t * math.pi))
                pts.append((x, y))
            dr.line(pts, fill=col, width=3)
        for i in range(60):   # grid ticks
            x = i * 15
            dr.line([(x, H - 40), (x, H - 32 if i % 4 else H - 20)], fill=(90, 50, 140), width=1)
    img = glow(img, d, 14, 0.7)
    return grain(img, 6, 2)

# ---------------------------------------------------------------- Venom: acid green, hexagon cells and a fang-shaped filter curve
def venom():
    img = base((6, 30, 14), (2, 10, 6))
    def d(dr):
        r = 34
        for row in range(-1, 12):
            for col in range(-1, 18):
                cx = col * r * 1.75 + (r * 0.875 if row % 2 else 0); cy = row * r * 1.5 + 30
                pts = [(cx + r * math.cos(math.pi / 3 * i + math.pi / 6), cy + r * math.sin(math.pi / 3 * i + math.pi / 6)) for i in range(6)]
                dr.polygon(pts, outline=(20, 90, 40))
        pts = []
        for x in range(0, W, 2):
            t = x / W
            y = H * 0.72 - 260 * math.exp(-((t - 0.62) ** 2) * 60) - 40 * (1 - t)
            pts.append((x, y))
        dr.line(pts, fill=(90, 255, 60), width=4)
        dr.line([(0, H * 0.72), (W, H * 0.72)], fill=(40, 140, 60), width=1)
    img = glow(img, d, 12, 0.9)
    return grain(img, 7, 3)

# ---------------------------------------------------------------- Sim Moth: warm amber, three big simple sine rings
def simmoth():
    img = base((70, 40, 8), (24, 12, 2))
    def d(dr):
        for i, (cx, col) in enumerate([(230, (255, 190, 60)), (450, (255, 150, 40)), (670, (255, 220, 110))]):
            for k in range(6):
                rad = 60 + k * 22
                dr.ellipse([cx - rad, 300 - rad, cx + rad, 300 + rad], outline=col if k % 2 == 0 else (120, 70, 20), width=3 if k == 0 else 1)
        for x in range(0, W, 30):
            dr.line([(x, 0), (x, H)], fill=(60, 36, 10), width=1)
    img = glow(img, d, 10, 0.6)
    return grain(img, 9, 4)

# ---------------------------------------------------------------- Mini Moth: cyan, minimal single wave and dot grid
def minimoth():
    img = base((6, 26, 40), (2, 8, 16))
    def d(dr):
        for y in range(40, H, 40):
            for x in range(40, W, 40):
                dr.ellipse([x - 2, y - 2, x + 2, y + 2], fill=(30, 110, 150))
        pts = [(x, H * 0.5 + 90 * math.sin(x / W * math.pi * 4)) for x in range(0, W, 2)]
        dr.line(pts, fill=(80, 220, 255), width=5)
    img = glow(img, d, 16, 0.8)
    return grain(img, 5, 5)

# ---------------------------------------------------------------- Thump: red/orange concentric shock rings off-centre, pitch sweep arc
def thump():
    img = base((60, 12, 6), (14, 3, 2))
    def d(dr):
        cx, cy = 300, 330
        for k in range(14):
            rad = 30 + k * 28
            w = 6 if k < 2 else max(1, 5 - k // 3)
            dr.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], outline=(255, 110 - k * 4, 40), width=w)
        pts = []
        for x in range(380, W, 2):
            t = (x - 380) / (W - 380)
            pts.append((x, 120 + 380 * (1 - math.exp(-t * 6))))
        dr.line(pts, fill=(255, 170, 60), width=4)
    img = glow(img, d, 14, 0.7)
    return grain(img, 10, 6)

# ---------------------------------------------------------------- Dial 10: orange, radial dial ticks and FM sideband spectrum bars
def dial10():
    img = base((50, 24, 4), (12, 6, 1))
    def d(dr):
        cx, cy = 640, 300
        for i in range(60):
            a = -math.pi * 0.75 + i / 59 * math.pi * 1.5
            r0 = 150 if i % 5 else 135
            dr.line([(cx + r0 * math.cos(a), cy + r0 * math.sin(a)), (cx + 165 * math.cos(a), cy + 165 * math.sin(a))], fill=(255, 160, 50) if i % 5 == 0 else (150, 90, 30), width=3 if i % 5 == 0 else 1)
        a = -math.pi * 0.75 + 0.62 * math.pi * 1.5
        dr.line([(cx, cy), (cx + 120 * math.cos(a), cy + 120 * math.sin(a))], fill=(255, 210, 120), width=5)
        for k in range(-9, 10):   # bessel-ish sidebands
            x = 200 + k * 20; hgt = 220 * math.exp(-(k * k) / 14.0) * (0.6 + 0.4 * abs(math.cos(k * 1.3)))
            dr.rectangle([x - 6, 470 - hgt, x + 6, 470], fill=(255, 150 + abs(k) * 4, 60))
    img = glow(img, d, 10, 0.7)
    return grain(img, 8, 7)

# ---------------------------------------------------------------- Hive Pads: honey/amber pad grid (4x4) with hex cells
def hivepads():
    img = base((40, 28, 6), (10, 7, 1))
    def d(dr):
        for r in range(4):
            for c in range(4):
                x0, y0 = 150 + c * 150, 60 + r * 120
                dr.rounded_rectangle([x0, y0, x0 + 130, y0 + 100], radius=14, outline=(255, 190, 60) if (r + c) % 3 else (255, 230, 150), width=3)
                if (r * 4 + c) in (0, 5, 10, 15): dr.rounded_rectangle([x0 + 8, y0 + 8, x0 + 122, y0 + 92], radius=10, fill=(120, 80, 20))
    img = glow(img, d, 12, 0.6)
    return grain(img, 8, 8)

# ---------------------------------------------------------------- Blossom 2: cherry-blossom pink, petals as five soft ellipses, string lines
def blossom2():
    img = base((48, 12, 30), (14, 3, 10))
    def d(dr):
        for (cx, cy, s) in [(240, 220, 1.0), (640, 380, 1.3), (520, 150, 0.6), (180, 460, 0.7), (780, 120, 0.5)]:
            for i in range(5):
                a = i / 5 * math.pi * 2
                px, py = cx + 38 * s * math.cos(a), cy + 38 * s * math.sin(a)
                dr.ellipse([px - 30 * s, py - 22 * s, px + 30 * s, py + 22 * s], outline=(255, 150, 200), width=2)
            dr.ellipse([cx - 8 * s, cy - 8 * s, cx + 8 * s, cy + 8 * s], fill=(255, 220, 120))
        for i in range(6):
            y = 500 + i * 14
            dr.line([(0, y), (W, y - 40 * math.sin(i))], fill=(200, 90, 150), width=1)
    img = glow(img, d, 12, 0.7)
    return grain(img, 7, 9)

if __name__ == '__main__':
    which = sys.argv[1:] or ['mothsynth2', 'venom', 'simmoth', 'minimoth', 'thump', 'dial10', 'hivepads', 'blossom2']
    for k in which:
        save(globals()[k](), k)
