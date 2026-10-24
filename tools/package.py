#!/usr/bin/env python3
"""Assemble separately installable core and Missives archives with provenance."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile
from artifacts import ROOT, sha, version

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('dist'))
    parser.add_argument('--dependencies', type=Path, help='Local cross-build dependency prefix')
    parser.add_argument('--commonlib', type=Path, help='Local override of pinned CommonLib source')
    parser.add_argument('--minhook', type=Path, help='Local override of pinned MinHook source')
    args = parser.parse_args()
    dll, pdb = args.build/'MarcoPolo.dll', args.build/'MarcoPolo.pdb'
    if not dll.is_file() or not pdb.is_file():
        parser.error('DLL and matching PDB are required')
    if args.output.exists() and any(args.output.iterdir()):
        parser.error('Use an empty output directory')
    subprocess.run([sys.executable,str(ROOT/'tools/audit-dll.py'),str(dll)],check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/build-addon.py')],check=True)
    commit = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    dirty = bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip())
    prefix = f'Marco-Polo-{version()}-{commit[:8]}' + ('-dirty' if dirty else '')
    args.output.mkdir(parents=True,exist_ok=True)
    def write(suffix, files):
        with zipfile.ZipFile(args.output/(prefix+suffix+'.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
            for name,path in sorted(files.items()):
                info = zipfile.ZipInfo(name, (2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
                info.external_attr=0o100644<<16;archive.writestr(info,path.read_bytes())
    files = {p.relative_to(ROOT/'package').as_posix():p for p in (ROOT/'package').rglob('*') if p.is_file()}
    files['SKSE/Plugins/MarcoPolo.dll']=dll
    for name in ('README.md','LICENSE','EXCEPTIONS.md','THIRD_PARTY_NOTICES.md','docs/TESTING.md'):
        files[name]=ROOT/name
    files.update({p.relative_to(ROOT).as_posix():p for p in (ROOT/'licenses').iterdir() if p.is_file()})
    deps = args.dependencies or args.build/'vcpkg_installed/x64-windows-static-md'
    for name in ('directxmath','directxtk','fmt','spdlog','rapidcsv'):
        notice=deps/'share'/name/'copyright'
        if not notice.is_file():
            parser.error(f'Missing dependency license: {notice}')
        files[f'licenses/{name}.txt']=notice
    write('',files)
    write('-missives',{'Marco Polo - Missives.esp':ROOT/'addons/missives/Marco Polo - Missives.esp','README-Missives.md':ROOT/'addons/missives/README.md','LICENSE':ROOT/'LICENSE'})
    write('-symbols',{'MarcoPolo.pdb':pdb})
    tracked=subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode().split('\0')
    source={p:ROOT/p for p in tracked if p and (ROOT/p).is_file()}
    for name,folder in (('CommonLibSSE-NG',args.commonlib or args.build/'_deps/commonlibsse-src'),
                        ('MinHook',args.minhook or args.build/'_deps/minhook-src')):
        if not (folder/'CMakeLists.txt').is_file():parser.error(f'Missing corresponding source: {folder}')
        for path in folder.rglob('*'):
            relative=path.relative_to(folder)
            if path.is_file() and not any(part.startswith('.') or part=='build' for part in relative.parts):
                source[f'dependencies/{name}/{relative.as_posix()}']=path
    write('-source',source)
    meta={'version':version(),'commit':commit,'dirty':dirty,'dll_sha256':sha(dll),
          'workflow_run':os.getenv('GITHUB_RUN_ID'),'runtime':'Skyrim Steam 1.6.1170',
          'commonlib_commit':'12d61f3c39f7d4ba1b1ab543df2bca0e613dc747',
          'in_game_validation':'pending','test_report':'Windows CTest in the linked build workflow'}
    (args.output/'build-info.json').write_text(json.dumps(meta,indent=2)+'\n')
    (args.output/'release-notes.md').write_text((ROOT/'docs/RELEASE.md').read_text())
    (args.output/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(args.output.iterdir()) if p.is_file()))
    print(f'Packaged {prefix}')

if __name__=='__main__':
    main()
