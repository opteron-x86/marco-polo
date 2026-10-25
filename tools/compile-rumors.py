#!/usr/bin/env python3
"""Rebuild rumor PEX files using Bethesda's compiler (Wine on Linux).

Example:
  python tools/compile-rumors.py --compiler /path/PapyrusCompiler.exe \
      --imports /path/game/Data/Scripts/Source --imports /path/extra/headers \
      --flags /path/TESV_Papyrus_Flags.flg

The compiler and Bethesda script headers are build inputs, not redistributed.
CI validates the checked-in source/bytecode manifest and generated ESP/SEQ.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'addons/rumors'

def normalize(data, filename):
    if data[:4] != bytes.fromhex('fa57c0de'):
        raise ValueError('Not Skyrim PEX')
    # Normalize compile time and machine-specific header strings.
    at = 16
    for _ in range(3):
        n, = struct.unpack_from('>H', data, at)
        at += 2 + n
    def string(s):
        b = s.encode()
        return struct.pack('>H', len(b)) + b
    out = bytearray(data[:8] + bytes(8) + string(filename) + string('MarcoPolo') + string('Build') + data[at:])
    at = 16 + sum(2 + len(s.encode()) for s in (filename, 'MarcoPolo', 'Build'))
    count, = struct.unpack_from('>H', out, at); at += 2
    for _ in range(count):
        n, = struct.unpack_from('>H', out, at); at += 2 + n
    if out[at]:
        out[at + 1:at + 9] = bytes(8) # Debug information timestamp.
    return bytes(out)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler', type=Path, required=True)
    p.add_argument('--imports', type=Path, action='append', required=True)
    p.add_argument('--flags', type=Path, required=True)
    args = p.parse_args()
    env = dict(os.environ)
    if os.name != 'nt':
        env.setdefault('WINEPREFIX', str(ROOT / 'build/papyrus-wine'))
        env.setdefault('WINEDEBUG', '-all')
        Path(env['WINEPREFIX']).mkdir(parents=True, exist_ok=True)
    def winpath(path):
        if os.name == 'nt': return str(path.resolve())
        return subprocess.check_output(['winepath', '-w', str(path.resolve())], env=env, text=True).strip()
    imports = ';'.join(winpath(d) for d in [OUT / 'Scripts/Source', *args.imports])
    for source in sorted((OUT / 'Scripts/Source').glob('*.psc')):
        command = ([] if os.name == 'nt' else ['wine']) + [str(args.compiler), winpath(source),
                   '-i=' + imports, '-o=' + winpath(OUT / 'Scripts'), '-f=' + winpath(args.flags)]
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout)
        if result.returncode or '1 succeeded, 0 failed' not in result.stdout:
            raise SystemExit('Papyrus compilation failed')
        binary = OUT / 'Scripts' / (source.stem + '.pex')
        binary.write_bytes(normalize(binary.read_bytes(), source.name))
    files = sorted((OUT / 'Scripts').rglob('*'))
    hashes = {p.relative_to(OUT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in files if p.suffix in ('.psc', '.pex')}
    manifest = {'compiler': 'Bethesda PapyrusCompiler', 'sha256': hashes}
    (OUT / 'script-build.json').write_text(json.dumps(manifest, indent=2) + '\n')

if __name__ == '__main__': main()
