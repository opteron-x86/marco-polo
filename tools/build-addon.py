#!/usr/bin/env python3
"""Generate the optional, ESL-flagged Missives plugin. No overrides or scripts."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
HOLDS = ('Whiterun', 'Eastmarch', 'Haafingar', 'The Rift', 'The Reach',
         'Falkreath', 'Hjaalmarch', 'The Pale', 'Winterhold')

def field(tag, value):
    return tag.encode('ascii') + struct.pack('<H', len(value)) + value

def string(text):
    return text.encode('cp1252') + b'\0'

def record(tag, form, fields, flags=0):
    body = b''.join(field(k, v) for k, v in fields)
    return tag.encode('ascii') + struct.pack('<IIIIHH', len(body), flags, form, 0, 44, 0) + body

def build():
    books = []
    for region, hold in enumerate(HOLDS):
        # Preserve the original 18 IDs. The old contract paper becomes the
        # carried receipt; new IDs are reserved for pinned contract postings.
        for kind in ('bulletin', 'receipt', 'posting'):
            local = 0x812 + region if kind == 'posting' else 0x800 + region * 2 + (kind == 'receipt')
            title = {'bulletin': 'Market bulletin', 'receipt': 'Accepted supply order',
                     'posting': 'Supply contract'}[kind]
            data = bytearray.fromhex('00ff0000ffffffff0000000000000000')
            if kind != 'receipt':
                data[0] = 2  # BOOK: Can't be taken; read through the board adapter.
            fields = [('EDID', string(f'MarcoPoloNote{local:03X}')), ('OBND', bytes(12)),
                      ('FULL', string(f'{hold}: {title}')),
                      ('MODL', string('Clutter\\Books\\Note01.nif')),
                      ('DESC', string('This posting has been withdrawn.')),
                      ('YNAM', struct.pack('<I', 0xC7A54)),
                      ('DATA', bytes(data)),
                      ('INAM', struct.pack('<I', 0x97788)), ('CNAM', b'\0')]
            books.append(record('BOOK', 0x03000000 | local, fields))
    body = b''.join(books)
    group = b'GRUP' + struct.pack('<I', len(body)+24) + b'BOOK' + bytes(12) + body
    header = [('HEDR', struct.pack('<fII', 1.7, len(books)+1, 0x81B)),
              ('CNAM', string('opteron-x86')), ('SNAM', string('Marco Polo optional market notices and supply orders.'))]
    for master in ('Skyrim.esm', 'Update.esm', 'Missives.esp'):
        header += [('MAST', string(master)), ('DATA', bytes(8))]
    return record('TES4', 0, header, 0x200) + group

def main():
    output = ROOT / 'addons/missives/Marco Polo - Missives.esp'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build())
    print(f'{output.name}: 27 book records, no overrides, no scripts')

if __name__ == '__main__':
    main()
