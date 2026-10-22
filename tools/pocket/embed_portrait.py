"""Embed the same checked RGB565A8 portrait bytes in the native UI test."""
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
data = source.read_bytes()
if len(data) != 9 * 128 * 128 * 3:
    raise SystemExit("Expected nine 128x128 RGB565A8 frames")
rows = [",".join(str(b) for b in data[i:i+32]) for i in range(0, len(data), 32)]
target.write_text("const unsigned char pf_furina_data[] = {\n" + ",\n".join(rows) + "\n};\n")
