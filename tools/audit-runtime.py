#!/usr/bin/env python3
"""Verify hook slots against CommonLib; optionally audit a local 1.6.1170 PE.

Reads game files only. Does not copy/distribute executable code or modify them.
The PE's code may be packed; this verifies vtable addresses, not instructions.
"""
import argparse
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]


def targets():
    text = (ROOT / 'src/runtime/HookTargets.h').read_text()
    return {name: int(value, 0) for name, value in re.findall(
        r'constexpr std::\w+ (\w+) = (0x[0-9A-Fa-f]+|\d+);', text)}


def check_commonlib(path, hook):
    text = (path / 'src/RE/T/TESObjectREFR.cpp').read_text()
    match = re.search(r'RelocateVirtual<decltype\(&TESObjectREFR::ResetInventory\)>\('
                      r'(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),', text)
    if not match or int(match[1], 0) != hook['ResetInventorySlot']:
        raise ValueError('ResetInventory slot does not match CommonLib SE/AE dispatch')
    for relative, method, key in [('include/RE/B/BarterMenu.h', 'Accept', 'BarterAcceptSlot'),
                              ('include/RE/I/IMenu.h', 'AdvanceMovie', 'BarterAdvanceSlot')]:
        text = (path / relative).read_text()
        match = re.search(r'\b' + method + r'\([^\n]*//\s*([0-9A-Fa-f]{2})\b', text)
        if not match or int(match[1], 16) != hook[key]:
            raise ValueError(f'{method} slot does not match CommonLib declaration')


def addresses(path):
    blob = path.read_bytes()
    pos = 0

    def read(fmt):
        nonlocal pos
        result = struct.unpack_from('<' + fmt, blob, pos)
        pos += struct.calcsize('<' + fmt)
        return result[0] if len(result) == 1 else result

    if read('I') != 2 or read('4I') != (1, 6, 1170, 0):
        raise ValueError('Expected the AE 1.6.1170.0 address database')
    name_size = read('I')
    pos += name_size
    pointer_size, count = read('2I')
    if pointer_size != 8:
        raise ValueError('Expected 64-bit pointers')

    def expand(kind, previous):
        if kind == 0: return read('Q')
        if kind == 1: return previous + 1
        if kind == 2: return previous + read('B')
        if kind == 3: return previous - read('B')
        if kind == 4: return previous + read('H')
        if kind == 5: return previous - read('H')
        if kind == 6: return read('H')
        if kind == 7: return read('I')
        raise ValueError(f'Unknown address encoding {kind}')

    ident = offset = 0
    result = {}
    for _ in range(count):
        kind = read('B')
        ident = expand(kind & 15, ident)
        scaled = bool(kind & 128)
        offset = expand((kind >> 4) & 7, offset // pointer_size if scaled else offset)
        if scaled: offset *= pointer_size
        result[ident] = offset
    if pos != len(blob):
        raise ValueError('Trailing address database data')
    return result


def check_exe(path, database, hook):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Not a PE executable')
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    image_base = struct.unpack_from('<Q', data, pe + 48)[0]

    def offset(rva):
        for i in range(count):
            size, va, raw_size, raw = struct.unpack_from('<4I', data, pe + 24 + optional_size + i * 40 + 8)
            if va <= rva < va + raw_size:
                return raw + rva - va
        raise ValueError(f'RVA {rva:#x} is outside file-backed sections')

    result = []
    for label, table, slot, ident in [
        ('ResetInventory', 190259, hook['ResetInventorySlot'], hook['ResetInventoryAE']),
        ('BarterMenu::Accept', 214926, hook['BarterAcceptSlot'], 50947),
        ('BarterMenu::AdvanceMovie', 214926, hook['BarterAdvanceSlot'], 82307),
    ]:
        actual = struct.unpack_from('<Q', data, offset(database[table]) + slot * 8)[0] - image_base
        if actual != database[ident]:
            raise ValueError(f'{label}: slot {slot:#x} points to {actual:#x}, not ID {ident}')
        result.append({'hook': label, 'slot': hex(slot), 'id': ident, 'rva': hex(actual)})
    # Description detour uses the ID bound by CommonLib's TESDescription wrapper.
    offset(database[14552])
    result.append({'hook': 'TESDescription::GetDescription', 'id': 14552, 'rva': hex(database[14552])})
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commonlib', type=Path, required=True)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--address-library', type=Path)
    args = parser.parse_args()
    if bool(args.exe) != bool(args.address_library):
        parser.error('--exe and --address-library must be supplied together')
    hook = targets()
    check_commonlib(args.commonlib, hook)
    print('PASS: hook slots match pinned CommonLib SE/AE declarations and dispatch')
    if args.exe:
        print(json.dumps(check_exe(args.exe, addresses(args.address_library), hook), indent=2))
        print('PASS: hook targets match the supplied 1.6.1170 executable vtables')
