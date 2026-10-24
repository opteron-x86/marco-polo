#!/usr/bin/env python3
"""Validate the tag and publish only the archives produced by the tested build."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from artifacts import check_tag, verify, version

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tag',required=True);p.add_argument('--dist',type=Path);p.add_argument('--commit');args=p.parse_args()
    check_tag(args.tag,version())
    if not args.dist:
        print(f'{args.tag} matches {version()}');return
    if not args.commit:p.error('--commit is required')
    verify(args.dist,version(),args.commit)
    result=subprocess.run(['gh','release','view',args.tag,'--json','isDraft,url'],capture_output=True,text=True)
    existing=json.loads(result.stdout) if result.returncode==0 else None
    if existing and not existing['isDraft']:
        with tempfile.TemporaryDirectory() as temp:
            subprocess.run(['gh','release','download',args.tag,'--dir',temp],check=True)
            verify(Path(temp),version(),args.commit)
        print(existing['url']);return
    files=[str(p) for p in sorted(args.dist.iterdir())]
    if existing:
        subprocess.run(['gh','release','upload',args.tag,*files,'--clobber'],check=True)
    else:
        subprocess.run(['gh','release','create',args.tag,*files,'--verify-tag','--draft','--title',f'Marco Polo {args.tag}','--notes-file',str(args.dist/'release-notes.md')],check=True)
    options=['--prerelease','--latest=false'] if '-' in args.tag else ['--prerelease=false']
    subprocess.run(['gh','release','edit',args.tag,'--draft=false','--notes-file',str(args.dist/'release-notes.md'),*options],check=True)
    if summary:=os.getenv('GITHUB_STEP_SUMMARY'):
        with open(summary,'a') as out:out.write(f'Released Marco Polo {args.tag} from {args.commit}.\n')

if __name__=='__main__':main()
