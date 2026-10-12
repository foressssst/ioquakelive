import math
import os

from PIL import Image

SIDE = 256
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'content', 'pak01', 'ui', 'ioql',
                   'vignette.png')

INNER = ((0, 102, 153), 0.2)
OUTER = ((0, 0, 0), 0.8)

img = Image.new('RGBA', (SIDE, SIDE))
px = img.load()
for j in range(SIDE):
    for i in range(SIDE):
        u = (i + 0.5) / SIDE * 2 - 1
        v = (j + 0.5) / SIDE * 2 - 1
        t = min(1.0, math.hypot(u, v) / math.sqrt(2))
        f = 0.0 if t <= 0.1 else (t - 0.1) / 0.9
        a = INNER[1] + (OUTER[1] - INNER[1]) * f
        pre = [INNER[0][k] * INNER[1] + (OUTER[0][k] * OUTER[1] - INNER[0][k] * INNER[1]) * f for k in range(3)]
        rgb = [int(round(c / a)) if a > 0 else 0 for c in pre]
        px[i, j] = (rgb[0], rgb[1], rgb[2], int(round(a * 255)))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.save(OUT, optimize=True)
print('wrote', os.path.normpath(OUT))
