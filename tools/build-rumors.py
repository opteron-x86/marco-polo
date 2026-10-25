#!/usr/bin/env python3
"""Generate the small, optional innkeeper dialogue plugin and startup SEQ."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'addons/rumors'
PLUGIN = 'MarcoPolo - Trade Rumors.esp'
QUEST, BRANCH, TOPIC, INFO, ENABLED = [0x01000800 + i for i in range(5)]

def u32(n): return struct.pack('<I', n)
def text(s):
    b = s.encode('utf-8')
    return struct.pack('<H', len(b)) + b
def ztext(s): return s.encode('utf-8') + b'\0'
def record(tag, form, fields, flags=0):
    body = b''.join(k.encode() + struct.pack('<H', len(v)) + v for k, v in fields)
    return tag.encode() + struct.pack('<IIIIHH', len(body), flags, form, 0, 44, 0) + body
def group(label, records, kind=0):
    body = b''.join(records)
    return b'GRUP' + u32(len(body) + 24) + label + u32(kind) + bytes(8) + body
def condition(function, param, value=1, or_next=False):
    return bytes([int(or_next), 0, 0, 0]) + struct.pack('<fH2xIIIIi', value, function,
                                                       param, 0, 0, 0, -1)

def generate():
    name = 'MarcoPolo_RumorTopic'
    # INFO VMAD v5/object-format 2: one propertyless script, OnBegin fragment.
    # Start the popup when the topic is selected, independent of audio playback.
    vmad = struct.pack('<HHH', 5, 2, 1) + text(name) + b'\0' + struct.pack('<H', 0)
    vmad += b'\x02\x01' + text(name) + b'\x01' + text(name) + text('Fragment_0')
    info = record('INFO', INFO, [
        ('EDID', ztext('MarcoPoloTradeRumorResponse')), ('VMAD', vmad),
        ('ENAM', struct.pack('<HH', 0x801, 0)), # Goodbye, No LIP file.
        ('CNAM', b'\0'),
        ('TRDT', bytes(12) + b'\x01' + bytes(11)),
        # A real response keeps this a normal selectable dialogue INFO. The
        # OnBegin fragment supplies the news in a popup without voice assets.
        ('NAM1', ztext("Here's what I've heard.")), ('NAM2', ztext('')), ('NAM3', ztext('')),
        ('CTDA', condition(74, ENABLED)),
        ('CTDA', condition(71, 0x5091B, or_next=True)), # JobInnkeeperFaction
        ('CTDA', condition(71, 0x9CC97)),             # JobRentRoomFaction
        ('CTDA', condition(46, 0, 0)),               # GetDead
    ])
    content = group(b'GLOB', [record('GLOB', ENABLED, [
        ('EDID', ztext('MarcoPoloTradeRumorsEnabled')), ('FNAM', b's'),
        ('FLTV', struct.pack('<f', 0))])])
    # Use the same canonical top-level group order as CK/xEdit plugins.
    content += group(b'DIAL', [record('DIAL', TOPIC, [
        ('EDID', ztext('MarcoPoloTradeRumorsTopic')), ('FULL', ztext('Any news of trade?')),
        ('PNAM', struct.pack('<f', 50)), ('BNAM', u32(BRANCH)), ('QNAM', u32(QUEST)),
        ('DATA', bytes(4)), ('SNAM', b'CUST'), ('TIFC', u32(1))]),
        group(u32(TOPIC), [info], 7)])
    content += group(b'QUST', [record('QUST', QUEST, [
        ('EDID', ztext('MarcoPoloTradeRumors')), ('FULL', ztext('Trade rumors')),
        ('DNAM', struct.pack('<HBBII', 1, 30, 65, 0, 0)), ('NEXT', b'')])])
    content += group(b'DLBR', [record('DLBR', BRANCH, [
        ('EDID', ztext('MarcoPoloTradeRumorsBranch')), ('QNAM', u32(QUEST)),
        ('TNAM', u32(0)), ('DNAM', u32(1)), ('SNAM', u32(TOPIC))])])
    header = record('TES4', 0, [
        ('HEDR', struct.pack('<fII', 1.7, 10, 0x805)), # Five records + five groups.
        ('CNAM', ztext('opteron-x86')),
        ('SNAM', ztext('Optional innkeeper trade news for Marco Polo.')),
        ('MAST', ztext('Skyrim.esm')), ('DATA', bytes(8))], 0x200)
    return {OUT / PLUGIN: header + content, OUT / 'SEQ' / (Path(PLUGIN).stem + '.seq'): u32(QUEST)}

def verify_scripts():
    manifest = json.loads((OUT / 'script-build.json').read_text())
    expected = {f'Scripts/{name}.pex' for name in ('MarcoPolo_Native', 'MarcoPolo_RumorTopic')}
    expected |= {f'Scripts/Source/{name}.psc' for name in ('MarcoPolo_Native', 'MarcoPolo_RumorTopic')}
    if set(manifest['sha256']) != expected:
        raise ValueError('Script manifest incomplete')
    for name, digest in manifest['sha256'].items():
        data = (OUT / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Recompile changed script: {name}')
        if name.endswith('.pex') and data[:4] != bytes.fromhex('fa57c0de'):
            raise ValueError('Expected Skyrim Papyrus bytecode')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    for path, data in generate().items():
        if args.check:
            if path.read_bytes() != data:
                raise ValueError(f'Regenerate {path}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if args.check:
        verify_scripts()
    print('Trade-rumor dialogue records and SEQ verified' if args.check else 'Built trade-rumor dialogue records and SEQ')

if __name__ == '__main__': main()
