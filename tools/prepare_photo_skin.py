#!/usr/bin/env python3
"""Extract runtime materials from the approved beta.5 render (requires Pillow).

Only unlabelled surfaces/caps are extracted. All text, scales, needles, LEDs and
control positions remain live JUCE graphics. Coordinates refer to the committed
1438x1094 reference; no image-generation service is needed to reproduce assets.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = Image.open(ROOT / 'Assets/reference/photoreal-rack-beta5.png').convert('RGB')
assert source.size == (1438, 1094)
materials = ROOT / 'Source/Assets/materials'
for name, box in {
    'top': (0, 0, 1438, 60),
    'bottom': (0, 1038, 1438, 1094),
    'left': (0, 60, 28, 1038),
    'right': (1411, 60, 1438, 1038),
}.items():
    source.crop(box).save(ROOT / f'Assets/wood_{name}.png')

# Clean brushed-metal swatches, mirrored vertically to avoid visible tile seams.
for name, box in {
    'metal_dark': (80, 226, 325, 241),
    'metal_black': (410, 742, 685, 775),
    'ivory': (751, 917, 1035, 936),
    'metal_blue': (1105, 980, 1300, 1010),
}.items():
    strip = source.crop(box)
    tile = Image.new('RGB', (strip.width, 640))
    for y in range(0, 640, strip.height):
        tile.paste(strip if (y // strip.height) % 2 == 0 else strip.transpose(Image.Transpose.FLIP_TOP_BOTTOM), (0, y))
    tile.save(materials / f'{name}.png')

# Tight cap crops exclude their original tracks; transparent padding accommodates
# the existing live control geometry and independently rendered contact shadows.
for name, box, padding in [
    ('eq_cap_cream', (208, 284, 241, 304), (4, 4)),
    ('eq_cap_red', (208, 568, 242, 588), (4, 4)),
    ('fader_cap', (1131, 324, 1164, 388), (3, 5)),
]:
    crop = source.crop(box).convert('RGBA')
    px, py = padding
    tile = Image.new('RGBA', (crop.width + 2 * px, crop.height + 2 * py))
    tile.paste(crop, (px, py))
    tile.save(materials / f'{name}.png')
