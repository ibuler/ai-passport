#!/usr/bin/env python3
"""Fail when the embedded settings page no longer matches its reviewed sources."""
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
