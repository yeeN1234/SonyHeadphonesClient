"""Draw the app icon (white headphones on a midnight-blue glass tile) into a Windows .ico.

    python tooling/MakeAppIcon.py client/Platform/Windows/app.ico

Every size is drawn on its own at 4x and scaled down, and the small sizes (24 px and under) use
thicker strokes and solid cups so the shape survives in the taskbar and the title bar. The
geometry is on a 100-unit square. Requires numpy and Pillow.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw

SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
TOP_LEFT = (0x3A, 0x4A, 0x6E)      # Gradient of the tile, lighter at the top left
BOTTOM_RIGHT = (0x1C, 0x24, 0x39)


def draw(size):
    scale = 4
    s = size * scale
    u = s / 100.0  # One unit of the 100-unit design, in pixels
    small = size <= 24

    # The tile: a diagonal gradient clipped to a rounded square.
    t = np.add.outer(np.arange(s), np.arange(s)) / (2.0 * (s - 1))
    rgb = np.empty((s, s, 3), dtype=np.float64)
    for c in range(3):
        rgb[..., c] = TOP_LEFT[c] + (BOTTOM_RIGHT[c] - TOP_LEFT[c]) * t
    tile = Image.fromarray(np.dstack([rgb, np.full((s, s), 255.0)]).astype(np.uint8), "RGBA")
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=22 * u, fill=255)
    icon = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    icon.paste(tile, (0, 0), mask)

    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    if not small:  # A faint sheen across the top half: the glass
        d.rounded_rectangle([8 * u, 8 * u, 92 * u, 48 * u], radius=18 * u, fill=(255, 255, 255, 15))
    icon = Image.alpha_composite(icon, layer)

    layer = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    white = (255, 255, 255, 255)
    stroke = (9 if small else 6) * u
    # Headband: a half circle over the cups, with straight ends and round caps.
    r = 22 * u
    cx, cy = 50 * u, 48 * u
    d.arc([cx - r - stroke / 2, cy - r - stroke / 2, cx + r + stroke / 2, cy + r + stroke / 2],
          180, 360, fill=white, width=int(round(stroke)))
    for x in (28 * u, 72 * u):
        d.line([x, cy, x, 58 * u], fill=white, width=int(round(stroke)))
        d.ellipse([x - stroke / 2, 58 * u - stroke / 2, x + stroke / 2, 58 * u + stroke / 2], fill=white)
    # Ear cups.
    cup = white if small else (255, 255, 255, 235)
    if small:
        boxes, radius = [(19, 52, 37, 79), (63, 52, 81, 79)], 8
    else:
        boxes, radius = [(21, 52, 36, 77), (64, 52, 79, 77)], 7.5
    for x0, y0, x1, y1 in boxes:
        d.rounded_rectangle([x0 * u, y0 * u, x1 * u, y1 * u], radius=radius * u, fill=cup)
    icon = Image.alpha_composite(icon, layer)
    return icon.resize((size, size), Image.LANCZOS)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    images = [draw(size) for size in SIZES]
    images[-1].save(sys.argv[1], format="ICO", sizes=[(n, n) for n in SIZES], append_images=images[:-1])
    print("Wrote", sys.argv[1], "with sizes", SIZES)


if __name__ == "__main__":
    main()
