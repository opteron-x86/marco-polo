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
# Addon INFO, vanilla voice type name/ID, vanilla response INFO. IDs 800-804
# retain their identities for existing saves. Only response data is shared:
# source scripts, links, conditions and quest actions are never copied.
VOICED_RESPONSES = (
    (0x01000805, 'MaleNord',             0x013AE6, 0x1041A4), # Let's see here....
    (0x01000806, 'MaleYoungEager',       0x013AD1, 0x03DF63), # Let's see here...
    (0x01000807, 'MaleOldKindly',        0x013AD6, 0x0E3A37), # I can certainly share a bit of what I know.
    (0x01000808, 'FemaleNord',           0x013AE7, 0x0D8DAD), # Sure.
    (0x01000809, 'FemaleArgonian',       0x013AEF, 0x0C34A5), # Sure.
    (0x0100080A, 'FemaleUniqueDelphine',  0x014432, 0x098E53), # Certainly.
)

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
    # INFO VMAD v5/object-format 2: one propertyless script, OnEnd fragment.
    # The native popup runs after the shared spoken response, not over it.
    vmad = struct.pack('<HHH', 5, 2, 1) + text(name) + b'\0' + struct.pack('<H', 0)
    vmad += b'\x02\x02' + text(name) + b'\x01' + text(name) + text('Fragment_0')
    gates = [
        ('CTDA', condition(74, ENABLED)),
        ('CTDA', condition(71, 0x5091B, or_next=True)), # JobInnkeeperFaction
        ('CTDA', condition(71, 0x9CC97)),             # JobRentRoomFaction
        ('CTDA', condition(46, 0, 0)),               # GetDead
    ]
    infos = []
    previous = 0
    def response(form, editor, source, voices):
        nonlocal previous
        infos.append(record('INFO', form, [
            ('EDID', ztext(editor)), ('VMAD', vmad),
            ('ENAM', struct.pack('<HH', 0x201, 0)), # Goodbye, Force Subtitle; use shared LIP.
            ('PNAM', u32(previous)), ('CNAM', b'\0'), ('DNAM', u32(source)),
            *gates, *voices,
        ]))
        previous = form
    for form, voice_name, voice, source in VOICED_RESPONSES:
        response(form, 'MarcoPoloTradeRumor' + voice_name, source,
                 [('CTDA', condition(426, voice))]) # GetIsVoiceType
    # Mutually exclusive fallback: every other voice gets the generic response.
    # Custom voices without this vanilla recording retain its localized subtitle.
    response(INFO, 'MarcoPoloTradeRumorResponse', 0xDBA22, # Of course.
             [('CTDA', condition(426, voice, 0)) for _, _, voice, _ in VOICED_RESPONSES])
    content = group(b'GLOB', [record('GLOB', ENABLED, [
        ('EDID', ztext('MarcoPoloTradeRumorsEnabled')), ('FNAM', b's'),
        ('FLTV', struct.pack('<f', 0))])])
    # Use the same canonical top-level group order as CK/xEdit plugins.
    content += group(b'DIAL', [record('DIAL', TOPIC, [
        ('EDID', ztext('MarcoPoloTradeRumorsTopic')), ('FULL', ztext('Any news of trade?')),
        ('PNAM', struct.pack('<f', 50)), ('BNAM', u32(BRANCH)), ('QNAM', u32(QUEST)),
        ('DATA', bytes(4)), ('SNAM', b'CUST'), ('TIFC', u32(len(infos)))]),
        group(u32(TOPIC), infos, 7)])
    content += group(b'QUST', [record('QUST', QUEST, [
        ('EDID', ztext('MarcoPoloTradeRumors')), ('FULL', ztext('Trade rumors')),
        ('DNAM', struct.pack('<HBBII', 1, 30, 65, 0, 0)), ('NEXT', b'')])])
    content += group(b'DLBR', [record('DLBR', BRANCH, [
        ('EDID', ztext('MarcoPoloTradeRumorsBranch')), ('QNAM', u32(QUEST)),
        ('TNAM', u32(0)), ('DNAM', u32(1)), ('SNAM', u32(TOPIC))])])
    header = record('TES4', 0, [
        ('HEDR', struct.pack('<fII', 1.7, 4 + len(infos) + 5, 0x80B)), # Records + groups.
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
