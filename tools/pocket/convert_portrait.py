"""Pack a transparent 3x3 expression sheet as nine 128x128 LVGL I4 frames.

Requires Pillow. Usage: python3 tools/pocket/convert_portrait.py SOURCE OUTPUT
"""
from pathlib import Path
import sys
from PIL import Image

source, target = map(Path, sys.argv[1:])
sheet = Image.open(source).convert("RGBA")
if sheet.width != sheet.height or sheet.width % 3:
    raise SystemExit("Expected a square 3x3 expression sheet")
cell = sheet.width // 3
frames = []
for i in range(9):
    x, y = i % 3 * cell, i // 3 * cell
    sprite = sheet.crop((x, y, x + cell, y + cell)).resize((120, 120), Image.Resampling.LANCZOS)
    frame = Image.new("RGBA", (128, 128))
    frame.paste(sprite, (4, 4))
    frames.append(frame)

# One shared palette avoids color flicker between expressions. Transparent
# pixels do not consume the fifteen opaque-color slots.
samples = [p[:3] for frame in frames for p in frame.get_flattened_data() if p[3] >= 128]
if not samples:
    raise SystemExit("No visible portrait pixels")
sample_image = Image.new("RGB", (len(samples), 1))
sample_image.putdata(samples)
palette = sample_image.quantize(colors=15, method=Image.Quantize.MEDIANCUT)
rgb_palette = palette.getpalette()[:45]
bgra_palette = bytes(4) + bytes(channel for i in range(15)
    for channel in (*reversed(rgb_palette[i * 3:i * 3 + 3]), 255))
output = bytearray()
preview = Image.new("RGBA", (128 * 3, 128 * 3))
for i, frame in enumerate(frames):
    indexed = frame.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE)
    pixels = [value + 1 if alpha >= 128 else 0 for value, alpha in
        zip(indexed.get_flattened_data(), frame.getchannel("A").get_flattened_data())]
    output.extend(bgra_palette)
    output.extend((a << 4) | b for a, b in zip(pixels[::2], pixels[1::2]))
    reconstructed = Image.new("RGBA", frame.size)
    reconstructed.putdata([tuple(reversed(bgra_palette[v * 4:v * 4 + 3])) +
                           (bgra_palette[v * 4 + 3],) for v in pixels])
    preview.paste(reconstructed, (i % 3 * 128, i // 3 * 128))
target.write_bytes(output)
preview.save(target.with_suffix(".png"))
print(f"Portrait: 9 x 128x128 I4, {len(output)} bytes")
