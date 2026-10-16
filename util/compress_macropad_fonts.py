#!/usr/bin/env python3
"""Losslessly repack existing clock glyphs using LVGL's no-prefilter RLE.

No font rasterization or glyph metrics change. Run from the repository root.
The manifest records original decoded pixels for independent decoder checks.
"""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / 'keyboards/ergohaven/src/display/fonts'
MANIFEST = ROOT / 'tests/ergohaven_display/clock_font_pixels.json'
BITMAP = re.compile(r'(glyph_bitmap\[\]\s*=\s*\{)(.*?)(\n\};)', re.S)
GLYPH = re.compile(r'\.bitmap_index = (\d+),.*?\.box_w = (\d+), \.box_h = (\d+)')


def parse(text):
    body = re.sub(r'/\*.*?\*/', '', BITMAP.search(text)[2], flags=re.S)
    data = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', body))
    return data, list(GLYPH.finditer(text))


def compress(pixels):
    # Modified I3BN used by lvgl/lv_font_conv, lib/font/compress.js.
    bits = []
    def put(value, count):
        bits.extend((value >> shift) & 1 for shift in range(count - 1, -1, -1))
    offset = 0
    while offset < len(pixels):
        value = pixels[offset]
        same = 1
        while same < 75 and offset + same < len(pixels) and pixels[offset + same] == value:
            same += 1
        offset += same
        put(value, 4)
        if same == 1:
            continue
        put(value, 4)
        if same <= 11:
            for _ in range(same - 2):
                put(1, 1)
            put(0, 1)
        else:
            for _ in range(11):
                put(1, 1)
            put(same - 12, 6)
    bits.extend([0] * ((-len(bits)) % 8))
    # get_bits() may look ahead one byte at a glyph's final bit boundary.
    return bytes(sum(bits[i + j] << (7 - j) for j in range(8)) for i in range(0, len(bits), 8)) + b'\0'


def main():
    manifest = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}
    saved = 0
    for path in sorted(FONTS.glob('eh_font_clock_*.c')):
        text = path.read_text()
        if '.bitmap_format = 0,' not in text:
            continue
        assert '.bpp = 4,' in text, path
        data, glyphs = parse(text)
        packed, decoded, indices = bytearray(), bytearray(), []
        for match in glyphs:
            start, width, height = map(int, match.groups())
            pixels = bytes((data[start + i // 2] >> (4 if i % 2 == 0 else 0)) & 15 for i in range(width * height))
            indices.append(len(packed))
            decoded.extend(pixels)
            if pixels:
                packed.extend(compress(pixels))
        assert len(packed) < len(data), path
        saved += len(data) - len(packed)
        manifest[path.name] = {'pixels_sha256': hashlib.sha256(decoded).hexdigest(), 'pixels': len(decoded),
                               'original_bytes': len(data), 'compressed_bytes': len(packed)}
        for match, index in reversed(list(zip(glyphs, indices))):
            text = text[:match.start(1)] + str(index) + text[match.end(1):]
        match = BITMAP.search(text)
        body = ''.join('\n    ' + ', '.join(f'0x{v:02x}' for v in packed[i:i+16]) + ',' for i in range(0, len(packed), 16))
        text = text[:match.start(2)] + body + text[match.end(2):]
        text = text.replace('.bitmap_format = 0,', '.bitmap_format = 2, /* LVGL RLE, no XOR prefilter */')
        text = text.replace(' * Opts:', ' * Bitmap repacked losslessly by util/compress_macropad_fonts.py.\n * Original rasterization opts:', 1)
        path.write_text(text, newline='\n')
    MANIFEST.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(f'Clock bitmap flash saved: {saved} bytes')


if __name__ == '__main__':
    main()
