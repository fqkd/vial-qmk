#!/usr/bin/env python3
"""Validate the compiled RP2040 image's two independent vendor HID channels."""
import pathlib
import struct
import sys


def load_uf2(path):
    data = pathlib.Path(path).read_bytes()
    assert len(data) % 512 == 0
    count = len(data) // 512
    payload = bytearray()
    for index in range(count):
        block = data[index * 512:(index + 1) * 512]
        a, b, flags, address, size, number, total, family = struct.unpack_from('<8I', block)
        assert (a, b) == (0x0A324655, 0x9E5D5157)
        assert flags & 0x2000 and family == 0xE48BFF56
        assert (size, number, total) == (256, index, count)
        assert address == 0x10000000 + len(payload)
        assert struct.unpack_from('<I', block, 508)[0] == 0x0AB16F30
        payload.extend(block[32:32 + size])
    assert len(payload) < 2 * 1024 * 1024
    return bytes(payload)


def hid_report(image, page, usage):
    prefix = bytes((6, page & 255, page >> 8, 9, usage, 0xA1, 1))
    offset = image.index(prefix)
    pos, depth, report_id, size, count = offset, 0, 0, 0, 0
    fields = {}
    while pos < offset + 128:
        tag = image[pos]
        length = (0, 1, 2, 4)[tag & 3]
        value = int.from_bytes(image[pos + 1:pos + 1 + length], 'little')
        pos += 1 + length
        kind = tag & 0xFC
        if kind == 0x84:
            report_id = value
        elif kind == 0x74:
            size = value
        elif kind == 0x94:
            count = value
        elif kind in (0x80, 0x90, 0xB0):
            key = (kind, report_id)
            fields[key] = fields.get(key, 0) + size * count
        elif kind == 0xA0:
            depth += 1
        elif kind == 0xC0:
            depth -= 1
            if depth == 0:
                return pos - offset, fields
    raise AssertionError('Unterminated vendor descriptor')


def check_config(image, offset, raw_length, micro_length):
    total = int.from_bytes(image[offset + 2:offset + 4], 'little')
    interfaces = image[offset + 4]
    assert 3 <= interfaces <= 8 and 60 <= total <= 512
    pos, end = offset + 9, offset + total
    records, endpoints, current = {}, set(), None
    while pos < end:
        length, kind = image[pos:pos + 2]
        assert length >= 2 and pos + length <= end
        d = image[pos:pos + length]
        if kind == 4:
            assert length == 9 and d[2] not in records and d[3] == 0
            current = {'count': d[4], 'class': d[5], 'eps': [], 'report': None}
            records[d[2]] = current
        elif kind == 0x21:
            assert current is not None and length == 9 and d[7:9]
            current['report'] = int.from_bytes(d[7:9], 'little')
        elif kind == 5:
            assert current is not None and length == 7 and d[2] not in endpoints
            endpoints.add(d[2])
            current['eps'].append((d[2] & 0x80, int.from_bytes(d[4:6], 'little'), d[3]))
        pos += length
    assert pos == end and set(records) == set(range(interfaces))
    for r in records.values():
        assert len(r['eps']) == r['count']
    for interface, size, report_length in ((1, 32, raw_length), (2, 64, micro_length)):
        r = records[interface]
        assert r['class'] == 3 and r['report'] == report_length
        assert sorted(r['eps']) == [(0, size, 3), (128, size, 3)]
    return interfaces


def main():
    image = load_uf2(sys.argv[1])
    raw_length, raw = hid_report(image, 0xFF60, 0x61)
    micro_length, micro = hid_report(image, 0xFF00, 1)
    assert raw == {(0x80, 0): 256, (0x90, 0): 256}, raw
    assert micro == {(0x80, 6): 504, (0x90, 6): 504}, micro
    candidates = []
    for offset in range(len(image) - 512):
        if image[offset:offset + 2] != b'\x09\x02':
            continue
        try:
            candidates.append(check_config(image, offset, raw_length, micro_length))
        except (AssertionError, IndexError, ValueError):
            pass
    assert len(candidates) == 1, candidates
    print(f'UF2 verified: {len(image)} flash bytes; {candidates[0]} USB interfaces; '
          'Vial interface 1 / 32 bytes; Micro interface 2 / report 6 / 64 bytes')


if __name__ == '__main__':
    main()
