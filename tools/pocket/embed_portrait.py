"""Embed the same checked I4 portrait bytes in the native UI test."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
data = source.read_bytes()
if len(data) != 9 * (64 + 128 * 128 // 2):
    raise SystemExit("Expected nine 128x128 I4 frames with 16-entry BGRA palettes")
rows = [",".join(str(b) for b in data[i:i+32]) for i in range(0, len(data), 32)]
target.write_text("const unsigned char pf_portrait_data[] = {\n" + ",\n".join(rows) + "\n};\n")
