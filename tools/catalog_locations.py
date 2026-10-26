#!/usr/bin/env python3
"""Read location IDs, keywords, parents and marker positions from official masters.

Read-only; no game records or extracted scripts are written. Field meanings follow
https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES5.pas
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def fields(data):
    at, extended = 0, None
    while at < len(data):
        if at + 6 > len(data):
            raise ValueError('Truncated subrecord')
        tag, size = struct.unpack_from('<4sH', data, at)
        at += 6
        if tag == b'XXXX':
            if size != 4 or at + 4 > len(data):
                raise ValueError('Invalid extended subrecord')
            extended = struct.unpack_from('<I', data, at)[0]
            at += 4
            continue
        if extended is not None:
            size, extended = extended, None
        if at + size > len(data):
            raise ValueError('Subrecord exceeds record')
        yield tag.decode('ascii'), data[at:at + size]
        at += size
    if extended is not None:
        raise ValueError('Unused extended size')


def string(data):
    return data.rstrip(b'\0').decode('utf-8')


def read_plugin(path, types, wanted_ids=()):
    data, masters, records = path.read_bytes(), [], {}

    def key(form):
        if not form:
            return None
        index = form >> 24
        if index >= len(masters):
            raise ValueError(f'Unresolved master for {form:08X} in {path.name}')
        return f'{masters[index]}|{form & 0xFFFFFF:06X}'

    def walk(start, end, world=None):
        at = start
        while at < end:
            if at + 24 > end:
                raise ValueError('Truncated record header')
            tag, size = struct.unpack_from('<4sI', data, at)
            if tag == b'GRUP':
                label, kind = struct.unpack_from('<Ii', data, at + 8)
                if size < 24 or at + size > end:
                    raise ValueError('Invalid group size')
                walk(at + 24, at + size, key(label) if kind == 1 else world)
                at += size
                continue
            flags, form = struct.unpack_from('<II', data, at + 8)
            if at + 24 + size > end:
                raise ValueError('Record exceeds group')
            relevant = tag.decode('ascii') in types or (wanted_ids and tag in (b'REFR', b'ACHR'))
            identity = key(form) if relevant and masters else None
            if tag == b'TES4' or tag.decode('ascii') in types or identity in wanted_ids:
                body = data[at + 24:at + 24 + size]
                if flags & 0x40000:
                    expected = struct.unpack_from('<I', body)[0]
                    body = zlib.decompress(body[4:])
                    if len(body) != expected:
                        raise ValueError('Invalid decompressed size')
                pairs = list(fields(body))
                if tag == b'TES4':
                    masters.extend(string(v) for t, v in pairs if t == 'MAST')
                    masters.append(path.name)
                else:
                    records[identity] = {'type': tag.decode('ascii'), 'fields': dict(pairs),
                                         'world': world, 'source': path.name, 'flags': flags,
                                         'masters': tuple(masters)}
            at += size + 24
    walk(0, len(data))
    return records


def form(record, raw):
    number = struct.unpack('<I', raw)[0]
    if not number:
        return None
    return f'{record["masters"][number >> 24]}|{number & 0xFFFFFF:06X}'


def classify(tags):
    if 'CWFort' in tags and 'LocTypeMilitaryFort' in tags:
        return 'fort'
    kinds = [kind for tag, kind in [('LocTypeBanditCamp', 'bandit'),
                                    ('LocTypeForswornCamp', 'forsworn'),
                                    ('LocTypeDraugrCrypt', 'crypt')] if tag in tags]
    return kinds[0] if len(kinds) == 1 else 'unknown'


def catalog(paths):
    records, provenance = {}, []
    for path in paths:
        records.update(read_plugin(path, {'KYWD', 'LCTN', 'WRLD'}))
        provenance.append({'plugin': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    markers = {form(r, r['fields']['MNAM']) for r in records.values()
               if r['type'] == 'LCTN' and 'MNAM' in r['fields']}
    refs = {}
    for path in paths:
        refs.update(read_plugin(path, set(), markers))
    locations = []
    for identity, r in sorted(records.items()):
        if r['type'] != 'LCTN':
            continue
        f = r['fields']
        keys = [form(r, raw[0]) for raw in struct.iter_unpack('4s', f.get('KWDA', b''))]
        tags = [string(records[k]['fields'].get('EDID', b'')) for k in keys if k in records]
        marker = form(r, f['MNAM']) if 'MNAM' in f else None
        ref = refs.get(marker, {})
        position = ref.get('fields', {}).get('DATA', b'')
        row = {'location': identity, 'editor_id': string(f.get('EDID', b'')),
               'parent': form(r, f['PNAM']) if 'PNAM' in f else None,
               'keywords': sorted(tags), 'kind': classify(tags), 'marker': marker,
               'world': ref.get('world'),
               'position': [round(v, 3) for v in struct.unpack_from('<3f', position)] if len(position) >= 12 else None,
               'winning_official_plugin': r['source']}
        locations.append(row)
    return {'schema': 1, 'sources': provenance, 'locations': locations}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    paths = [args.data / name for name in ('Skyrim.esm', 'Update.esm', 'Dawnguard.esm',
                                          'HearthFires.esm', 'Dragonborn.esm')]
    result = catalog(paths)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Read {len(result["locations"])} locations from {len(paths)} official masters')


if __name__ == '__main__':
    main()
