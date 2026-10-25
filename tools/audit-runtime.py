#!/usr/bin/env python3
"""Verify CommonLib hook slots and supplied desktop runtime address tables.

Reads game files only. Does not copy/distribute executable code or modify them.
The PE's code may be packed; this verifies vtable addresses, not instructions.
"""
import argparse
import csv
import hashlib
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

    fmt = read('I')
    version = read('4I')
    if fmt == 5:
        pos += 64  # Fixed executable name; dense RVA table indexed by ID.
        pointer_size, data_format, count = read('3I')
        if pointer_size != 8 or pos + count * 4 != len(blob):
            raise ValueError('Invalid format-5 address database')
        return version, {i: value[0] for i, value in
                         enumerate(struct.iter_unpack('<I', blob[pos:])) if value[0]}
    if fmt not in (1, 2):
        raise ValueError('Expected Address Library format 1, 2 or 5')
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
    return version, result


def check_exe(path, version, database, hook):
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

    ae = version >= (1, 6, 0, 0)
    ref_table, barter_table = (190259, 214926) if ae else (235511, 267991)
    reset_id = hook['ResetInventoryAE' if ae else 'ResetInventorySE']
    reverse = {rva: ident for ident, rva in database.items()}
    result = []
    for label, table, slot, ident in [
        ('ResetInventory', ref_table, hook['ResetInventorySlot'], reset_id),
        ('BarterMenu::Accept', barter_table, hook['BarterAcceptSlot'], None),
        ('BarterMenu::AdvanceMovie', barter_table, hook['BarterAdvanceSlot'], None),
    ]:
        actual = struct.unpack_from('<Q', data, offset(database[table]) + slot * 8)[0] - image_base
        if ident is None:
            ident = reverse.get(actual)
        if ident is None or actual != database[ident]:
            raise ValueError(f'{label}: slot {slot:#x} points to {actual:#x}, not ID {ident}')
        offset(actual)
        result.append({'hook': label, 'slot': hex(slot), 'id': ident, 'rva': hex(actual)})
    return result


def check_table(database, ae, hook):
    sites = [('TESObjectREFR::ResetInventory', hook['ResetInventorySE'], hook['ResetInventoryAE']),
             ('TESObjectREFR vtable', 235511, 190259),
             ('BarterMenu vtable', 267991, 214926),
             ('BarterMenu target handle', 519283, 405823)]
    result = []
    for name, se_id, ae_id in sites:
        ident = ae_id if ae else se_id
        if not database.get(ident):
            raise ValueError(f'Missing {name}: ID {ident}')
        result.append({'site': name, 'id': ident, 'address': hex(database[ident])})
    return result


def csv_addresses(path, ae):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return {int(row['aeid' if ae else 'sseid']): int(row['ae_addr' if ae else 'sse_addr'], 16)
                for row in csv.DictReader(stream)}


def check_information(database, version):
    # Match pinned CommonLib's MessageBoxMenu::Create dispatch. This is a call,
    # not a new detour, and must work on both sides of AE's ID renumbering.
    ident = 51421 if version < (1, 6, 0, 0) else 52270 if version < (1, 6, 1130, 0) else 442726
    if not database.get(ident):
        raise ValueError(f'Missing MessageBoxMenu::Create: ID {ident}')
    return [{'site': 'Trade-rumor message box', 'id': ident, 'address': hex(database[ident])}]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commonlib', type=Path, required=True)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--address-library', type=Path)
    parser.add_argument('--se-csv', type=Path)
    parser.add_argument('--ae17-csv', type=Path)
    parser.add_argument('--database-directory', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if bool(args.exe) != bool(args.address_library):
        parser.error('--exe and --address-library must be supplied together')
    hook = targets()
    check_commonlib(args.commonlib, hook)
    print('PASS: hook slots match pinned CommonLib SE/AE declarations and dispatch')
    if args.exe:
        version, database = addresses(args.address_library)
        print(json.dumps(check_exe(args.exe, version, database, hook), indent=2))
        print('PASS: hook slots resolve in the supplied executable; reset target matches its ID')
    tables = []
    for path, ae, version in [(args.se_csv, False, (1, 5, 97, 0)),
                               (args.ae17_csv, True, (1, 7, 104, 0))]:
        if path:
            tables.append((path, version, csv_addresses(path, ae)))
    if args.database_directory:
        for path in sorted(args.database_directory.glob('version*.bin')):
            version, database = addresses(path)
            tables.append((path, version, database))
    records = []
    for path, version, database in tables:
        sites = check_table(database, version >= (1, 6, 0, 0), hook)
        sites += check_information(database, version)
        records.append({'version': '.'.join(map(str, version)), 'table': path.name,
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'sites': sites})
    if records:
        print(f'PASS: hook relocation availability in {len(records)} runtime databases')
    if args.report:
        args.report.write_text(json.dumps({'databases': records, 'passed': True}, indent=2)+'\n')
