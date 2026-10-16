"""Decode every repacked glyph with the actual pinned LVGL C decoder.

Compare pixel hashes with the original uncompressed upstream rasterization.
Run after initializing the lib/lvgl submodule; requires a native C compiler.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'util'))
from compress_macropad_fonts import FONTS, MANIFEST, parse

source = (ROOT / 'lib/lvgl/src/font/lv_font_fmt_txt.c').read_text()


def function(name):
    match = re.search(r'static inline [^;{}]*\b' + name + r'\([^;{}]*\)\s*\{', source)
    assert match, name
    start = match.start()
    pos, depth = match.end(), 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]


program = '''#include <stdint.h>
#include <stdio.h>
enum {RLE_STATE_SINGLE, RLE_STATE_REPEATE, RLE_STATE_COUNTER};
static uint32_t rle_rdp;
static const uint8_t *rle_in;
static uint8_t rle_bpp, rle_prev_v, rle_cnt, rle_state;
'''
program += '\n'.join(function(name) for name in ['get_bits', 'rle_init', 'rle_next'])
manifest = json.loads(MANIFEST.read_text())
calls = []
for n, (name, expected) in enumerate(manifest.items()):
    text = (FONTS / name).read_text()
    assert '.bitmap_format = 2,' in text and '.bpp = 4,' in text
    data, glyphs = parse(text)
    assert len(data) == expected['compressed_bytes'], name
    program += f'\nstatic const uint8_t font_{n}[] = {{' + ','.join(map(str, data)) + '};\n'
    for glyph in glyphs:
        start, width, height = map(int, glyph.groups())
        if width * height:
            calls.append(f'rle_init(font_{n}+{start},4); for(unsigned i=0;i<{width*height};i++) putchar(rle_next());')
program += '\nint main(void) {\n' + '\n'.join(calls) + '\nreturn 0; }\n'
with tempfile.TemporaryDirectory(prefix='clock-fonts-') as tmp:
    tmp = Path(tmp)
    (tmp / 'decode.c').write_text(program)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', str(tmp / 'decode.c'), '-o', str(tmp / 'decode')], check=True)
    pixels = subprocess.check_output([str(tmp / 'decode')])
offset = 0
for name, expected in manifest.items():
    chunk = pixels[offset:offset + expected['pixels']]
    assert hashlib.sha256(chunk).hexdigest() == expected['pixels_sha256'], name
    offset += len(chunk)
assert offset == len(pixels)
print(f'{len(manifest)} fonts: all {offset} pixels identical to the originals; LVGL decoder and sanitizers passed')
