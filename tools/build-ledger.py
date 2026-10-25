#!/usr/bin/env python3
"""Build the native Scaleform ledger; --verify only checks the shipped artifact."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import platform
import struct
import subprocess
import tarfile
import urllib.request
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'interface/TradeLedger.as'
OUTPUT = ROOT / 'package/Interface/MarcoPoloTradeLedger.swf'
MANIFEST = ROOT / 'interface/build.json'
URL = 'https://archive.debian.org/debian/pool/main/m/mtasc/mtasc_1.14-3+b3_amd64.deb'
SHA256 = '0e02157747ddd11570972db3a881c18ec123ba19d1cd04635c160d713d4e7e26'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def compiler():
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        raise RuntimeError('Rebuild the SWF on x86-64 Linux; --verify works on every platform')
    folder = ROOT / 'build/ledger-compiler'
    archive = folder / 'mtasc.deb'
    folder.mkdir(parents=True, exist_ok=True)
    data = archive.read_bytes() if archive.is_file() else urllib.request.urlopen(URL, timeout=60).read()
    if sha(data) != SHA256:
        raise RuntimeError('MTASC archive checksum mismatch')
    archive.write_bytes(data)
    if data[:8] != b'!<arch>\n':
        raise RuntimeError('Invalid MTASC archive')
    pos = 8
    while pos < len(data):
        header = data[pos:pos + 60]
        size = int(header[48:58])
        name = header[:16].decode().strip().rstrip('/')
        body = data[pos + 60:pos + 60 + size]
        pos += 60 + size + size % 2
        if name.startswith('data.tar'):
            with tarfile.open(fileobj=io.BytesIO(body)) as tf:
                for member in tf:
                    path = Path(member.name.removeprefix('./'))
                    if '..' in path.parts or path.is_absolute():
                        raise RuntimeError('Unsafe compiler archive path')
                    if member.isfile() and (str(path).startswith('usr/share/mtasc/') or str(path) == 'usr/bin/mtasc'):
                        target = folder / path
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(tf.extractfile(member).read())
                        target.chmod(member.mode & 0o755)
    return folder

def validate(data):
    if data[:4] != b'FWS\x08' or struct.unpack_from('<I', data, 4)[0] != len(data):
        raise ValueError('Ledger must be an uncompressed Flash 8 movie')
    bits = data[8] >> 3
    pos = 8 + (5 + bits * 4 + 7) // 8 + 4
    tags = []
    while pos < len(data):
        header = struct.unpack_from('<H', data, pos)[0]
        pos += 2
        tag, size = header >> 6, header & 63
        if size == 63:
            size = struct.unpack_from('<I', data, pos)[0]
            pos += 4
        if pos + size > len(data):
            raise ValueError('Truncated SWF tag')
        tags.append(tag)
        pos += size
        if tag == 0:
            break
    if pos != len(data) or not tags or tags[-1] != 0 or 12 not in tags or 1 not in tags:
        raise ValueError('Incomplete ledger movie')
    if any(t in tags for t in (10, 48, 75, 91, 82)):
        raise ValueError('Ledger must use AS2 and game-provided fonts, without embedded font glyphs')
    for name in (b'$EverywhereFont', b'$EverywhereMediumFont', b'STEMNative', b'SetData'):
        if name not in data:
            raise ValueError(f'Missing ledger interface: {name!r}')
    return tags

def metadata(data):
    return {'compiler': 'MTASC 1.14 (Debian 1.14-3+b3)', 'compiler_archive_sha256': SHA256,
            'source_sha256': sha(SOURCE.read_bytes()), 'swf_sha256': sha(data),
            'font_aliases': ['$EverywhereFont', '$EverywhereMediumFont']}

def verify():
    data = OUTPUT.read_bytes()
    validate(data)
    if json.loads(MANIFEST.read_text()) != metadata(data):
        raise ValueError('Ledger source/artifact manifest mismatch; rebuild with tools/build-ledger.py')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Rebuild and compare with the shipped movie')
    parser.add_argument('--verify', action='store_true', help='Verify source, movie structure and hashes without a compiler')
    args = parser.parse_args()
    if args.verify:
        verify()
    else:
        folder = compiler()
        temp = ROOT / 'build/ledger-compiler/TradeLedger.swf'
        subprocess.run([str(folder/'usr/bin/mtasc'), '-cp', str(folder/'usr/share/mtasc/std'),
                        '-cp', str(folder/'usr/share/mtasc/std8'), '-version', '8',
                        '-header', '1280:720:60', '-main', '-swf', str(temp), str(SOURCE)], check=True)
        data = temp.read_bytes()
        if data[:3] == b'CWS':
            data = b'FWS' + data[3:8] + zlib.decompress(data[8:])
        validate(data)
        if args.check:
            verify()
            if data != OUTPUT.read_bytes():
                raise ValueError('Shipped movie differs from a fresh build')
        else:
            OUTPUT.parent.mkdir(parents=True, exist_ok=True)
            OUTPUT.write_bytes(data)
            MANIFEST.write_text(json.dumps(metadata(data), indent=2) + '\n')
    print('TradeLedger.swf verified' if args.verify or args.check else 'TradeLedger.swf built')

if __name__ == '__main__':
    main()
