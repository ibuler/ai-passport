#!/usr/bin/env python3
"""Check embedded settings sources and the compact XiaoZhi portrait pack."""
from pathlib import Path
import hashlib
import json
import gzip
root = Path(__file__).resolve().parents[2]
hashes = json.loads((root / 'main/settings-assets.json').read_text())
for name, expected in hashes.items():
    actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f'Stale settings asset: {name}; run cd web && npm ci && npm run build')
html = gzip.decompress((root / 'main/settings.html.gz').read_bytes()).decode()
assert '/*APP_SCRIPT*/' not in html
assert '保留彩色' in html and '播放动画' in html
print('Embedded settings assets: PASS')
portrait_root = root / 'assets/images/huolinger'
metadata = json.loads((portrait_root / 'metadata.json').read_text())
portrait = (portrait_root / 'huolinger.i4').read_bytes()
assert len(portrait) == 9 * (64 + 128 * 128 // 2) <= 80 * 1024
assert metadata['display_bytes'] == len(portrait)
for name, expected in metadata['files'].items():
    assert hashlib.sha256((portrait_root / name).read_bytes()).hexdigest() == expected, name
for offset in range(0, len(portrait), 8256):
    assert portrait[offset:offset + 4] == bytes(4)
    assert all(portrait[offset + i * 4 + 3] == 255 for i in range(1, 16))
print('Portrait assets: PASS (nine 128x128 I4 frames, 74,304 bytes)')
