"""Draws the web page's app icon (tools/pwa/*.png): the page's favicon, a
green outline of the screen with a chart line on it, on the dark tile of the
page itself. Needs Pillow:
    python tools/pwa_icon.py tools/pwa && python tools/gen_pwa.py"""
import sys
from PIL import Image, ImageDraw

out = sys.argv[1]
N = 1024
GREEN = (34, 197, 94)          # --acc of the page (#22c55e)
img = Image.new('RGB', (N, N))
d = ImageDraw.Draw(img)
for y in range(N):             # a soft vertical gradient on the page's background
    t = y / (N - 1)
    c = tuple(int(a + (b - a) * t) for a, b in zip((34, 34, 38), (9, 9, 11)))
    d.line([(0, y), (N, y)], fill=c)

# The favicon's 64-unit drawing (webui.html, <link rel="icon">), centred and
# scaled so its 50 x 44 units fill about 64 % of the tile: iOS rounds the
# corners and shows the icon small.
S = N * 0.64 / 50
OX = (N - 50 * S) / 2 - 7 * S
OY = (N - 44 * S) / 2 - 13 * S
W = 3 * S                      # the favicon's stroke width


def pt(x, y):
    return (OX + x * S, OY + y * S)


def line(points):
    pts = [pt(*p) for p in points]
    d.line(pts, fill=GREEN, width=round(W), joint='curve')
    for x, y in pts:           # round caps and joins
        d.ellipse((x - W / 2, y - W / 2, x + W / 2, y + W / 2), fill=GREEN)


x0, y0 = pt(7, 13)
x1, y1 = pt(57, 51)
d.rounded_rectangle((x0 - W / 2, y0 - W / 2, x1 + W / 2, y1 + W / 2), radius=6 * S + W / 2, outline=GREEN, width=round(W))
line([(15, 38), (23, 30), (30, 34), (37, 22), (44, 28), (50, 20)])
line([(24, 51), (24, 57)])
line([(40, 51), (40, 57)])
line([(20, 57), (28, 57)])
line([(36, 57), (44, 57)])

# 32 colours: the gradient and the anti-aliased strokes lose nothing visible,
# and the icons, kept in the firmware's flash, take a fraction of the space.
for s, name in ((512, 'icon-512.png'), (192, 'icon-192.png'), (180, 'apple-touch-icon.png')):
    small = img.resize((s, s), Image.LANCZOS).quantize(colors=32, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    small.save(f'{out}/{name}', optimize=True)
