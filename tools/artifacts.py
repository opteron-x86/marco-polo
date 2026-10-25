"""Shared release contract. Validation fails closed on incomplete/mixed builds."""
from pathlib import Path
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def version():
    return re.search(r'project\(MarcoPolo VERSION ([\d.]+)', (ROOT / 'CMakeLists.txt').read_text())[1]

def check_tag(tag, expected):
    if not re.fullmatch(r'v'+re.escape(expected)+r'(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?', tag):
        raise ValueError(f'Tag must match v{expected} or v{expected}-<prerelease>')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def verify(directory, expected_version, commit):
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Full source commit required')
    prefix = f'Marco-Polo-{expected_version}-{commit[:8]}'
    with_rumors = tuple(map(int, expected_version.split('.'))) >= (0, 2, 0)
    suffixes = ('', '-symbols', '-source', '-rumors') if with_rumors else ('', '-symbols', '-source')
    expected = {prefix+s+'.zip' for s in suffixes}
    expected |= {'build-info.json', 'release-notes.md'}
    sums = {}
    for line in (directory / 'SHA256SUMS.txt').read_text().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([^/\\]+)', line)
        if not match or match[2] in sums:
            raise ValueError('Invalid or duplicate checksum')
        sums[match[2]] = match[1]
    if set(sums) != expected or {p.name for p in directory.iterdir()} != expected | {'SHA256SUMS.txt'}:
        raise ValueError('Missing or unexpected artifacts')
    for name, digest in sums.items():
        if sha(directory / name) != digest:
            raise ValueError(f'Checksum mismatch: {name}')
    meta = json.loads((directory / 'build-info.json').read_text())
    if meta.get('version') != expected_version or meta.get('commit') != commit or meta.get('dirty') is not False:
        raise ValueError('Package does not match clean tagged source')
    for name in expected:
        if not name.endswith('.zip'):
            continue
        with zipfile.ZipFile(directory / name) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)) or archive.testzip() or any(n.startswith(('/', '\\')) or '..' in Path(n).parts for n in names):
                raise ValueError('Invalid archive contents')
    with zipfile.ZipFile(directory / (prefix+'.zip')) as archive:
        dll = archive.read('SKSE/Plugins/MarcoPolo.dll')
        if hashlib.sha256(dll).hexdigest() != meta.get('dll_sha256'):
            raise ValueError('DLL differs from build metadata')
        for name in ('economy', 'classification', 'settings'):
            json.loads(archive.read(f'SKSE/Plugins/MarcoPolo/{name}.json'))
    with zipfile.ZipFile(directory / (prefix+'-symbols.zip')) as archive:
        if archive.namelist() != ['MarcoPolo.pdb']:
            raise ValueError('Unexpected symbols archive')
    if with_rumors:
        if meta.get('optional_addons') != ['rumors']:
            raise ValueError('Rumor addon missing from build metadata')
        with zipfile.ZipFile(directory / (prefix+'-rumors.zip')) as archive:
            required = {'MarcoPolo - Trade Rumors.esp', 'SEQ/MarcoPolo - Trade Rumors.seq',
                        'Scripts/MarcoPolo_Native.pex', 'Scripts/MarcoPolo_RumorTopic.pex',
                        'Scripts/Source/MarcoPolo_Native.psc', 'Scripts/Source/MarcoPolo_RumorTopic.psc',
                        'README.md', 'LICENSE', 'script-build.json'}
            if set(archive.namelist()) != required:
                raise ValueError('Rumor addon incomplete or contains unexpected files')
            scripts = json.loads(archive.read('script-build.json'))['sha256']
            if set(scripts) != {n for n in required if n.startswith('Scripts/')}:
                raise ValueError('Rumor scripts manifest incomplete')
            for name,digest in scripts.items():
                if hashlib.sha256(archive.read(name)).hexdigest() != digest:
                    raise ValueError('Rumor script differs from compiled-source manifest')
    with zipfile.ZipFile(directory / (prefix+'-source.zip')) as archive:
        if not {'CMakeLists.txt','src/runtime/Plugin.cpp','src/core/Economy.cpp','LICENSE'}.issubset(archive.namelist()):
            raise ValueError('Source archive incomplete')
    return meta, prefix
