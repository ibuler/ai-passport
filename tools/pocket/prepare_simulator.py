#!/usr/bin/env python3
"""Overlay public virtual-network data on a copy of verified firmware; never flash."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf', default=os.environ.get('IDF_PATH'), required=not os.environ.get('IDF_PATH'))
    parser.add_argument('--firmware', default='build/FoloToy-AI-Passport-full.bin')
    parser.add_argument('--output', default='build/Pocket-Focus-simulator-preview.bin')
    parser.add_argument('--image', help='Optional PFI1 image, e.g. a browser-converted GIF')
    parser.add_argument('--card-test', action='store_true', help='Public sample company/city and manual radio fixtures')
    parser.add_argument('--tone-tests', action='store_true', help='Use isolated HTTP tone fixtures, not Internet audio')
    args = parser.parse_args()
    idf = Path(args.idf)
    header = (idf / 'components/esp_common/include/esp_idf_version.h').read_text()
    for name, number in [('MAJOR', 5), ('MINOR', 5), ('PATCH', 3)]:
        if not re.search(rf'#define\s+ESP_IDF_VERSION_{name}\s+{number}\b', header):
            raise SystemExit('This fixture layout is verified only for ESP-IDF 5.5.3')
    original = Path(args.firmware).read_bytes()
    if len(original) != 0x800000 or original[0] != 0xe9 or original[0x10000] != 0xe9:
        raise SystemExit('Expected the verified 8 MiB Pocket Focus merged image')
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pocket-fixture-', dir=output.parent.resolve()) as temp:
        temp = Path(temp)
        wifi = bytearray(184)  # sizeof(wifi_config_t), checked in the matching ELF.
        ssid = b'Emulator Host Bridge'
        wifi[:len(ssid)] = ssid
        csv = 'key,type,encoding,value\npocket_focus,namespace,,\nwifi,data,hex2bin,' + wifi.hex() + '\n'
        if args.tone_tests:
            config = struct.pack('<I96sBB', 1, '开发者'.encode(), 10, 3)
            tracks = [('立体声 MP3 测试', '/tone.mp3'), ('单声道分块测试', '/chunked.mp3'), ('错误音频测试', '/invalid')]
            for i in range(8):
                title, path = tracks[i] if i < len(tracks) else ('', '')
                url = 'http://93.184.216.34' + path if path else ''
                config += struct.pack('<96s512s', title.encode(), url.encode())
            config += b'\0' * (-len(config) % 4)
            assert len(config) == 4968  # sizeof(pf_config_t), tracks at byte 102.
            csv += 'config,data,hex2bin,' + config.hex() + '\n'
        if args.card_test:
            profile = struct.pack('<I96s64siihB', 1, '示例公司'.encode(), '珠海'.encode(), 222707, 1135767, 480, 2)
            for i in range(6):
                city, title, url = [('珠海', '珠海测试电台', 'http://93.184.216.34/tone.mp3'), ('广州', '广州测试电台', 'http://93.184.216.34/chunked.mp3')][i] if i < 2 else ('', '', '')
                profile += struct.pack('<64s96s512s', city.encode(), title.encode(), url.encode())
            profile += b'\0' * (-len(profile) % 4)
            assert len(profile) == 4208  # sizeof(pf_profile_t); radios at byte 175.
            csv += 'profile,data,hex2bin,' + profile.hex() + '\n'
        (temp / 'nvs.csv').write_text(csv)
        subprocess.run([sys.executable, str(idf / 'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'),
                        'generate', str(temp / 'nvs.csv'), str(temp / 'nvs.bin'), '0x6000'], check=True)
        data = bytearray(original)
        data[0x9000:0xf000] = (temp / 'nvs.bin').read_bytes()
        changes = (['public company/city/manual radio fixtures'] if args.card_test else []) + ['public virtual Wi-Fi SSID in NVS', 'generated tone track list' if args.tone_tests else 'production track list']
        if args.image:
            image = Path(args.image).read_bytes()
            if len(image) < 16 or image[:4] != b'PFI1': raise SystemExit('Invalid PFI1 file')
            width, height, frames, flags, crc = struct.unpack_from('<HHHHI', image, 4)
            if (width, height) != (96, 96) or not 1 <= frames <= 48 or flags > 3 or len(image) != 16 + frames * 18434 or zlib.crc32(image[16:]) != crc:
                raise SystemExit('Invalid PFI1 dimensions, length, or checksum')
            source = temp / 'storage'; source.mkdir(); (source / 'home0.pfi').write_bytes(image)
            subprocess.run([sys.executable, str(idf / 'components/spiffs/spiffsgen.py'), '0x300000', str(source), str(temp / 'storage.bin'),
                            '--page-size', '256', '--block-size', '4096', '--obj-name-len', '32', '--meta-len', '4', '--use-magic', '--use-magic-len'], check=True)
            data[0x500000:] = (temp / 'storage.bin').read_bytes()
            changes.append('test image in SPIFFS')
        assert data[:0x9000] == original[:0x9000] and data[0xf000:0x500000] == original[0xf000:0x500000]
        output.write_bytes(data)
        manifest = {'production_sha256': hashlib.sha256(original).hexdigest(), 'preview_sha256': hashlib.sha256(data).hexdigest(),
                    'code_identical': True, 'changes': changes, 'physical_flash': False}
        output.with_suffix('.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(f'Simulator-only copy: {output}')

if __name__ == '__main__':
    main()
