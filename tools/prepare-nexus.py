#!/usr/bin/env python3
"""Download and verify an already published standard release for Nexus."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import uuid
from artifacts import verify

def validate(tag, release, meta, commit, run):
    if not re.fullmatch(r'v\d+\.\d+\.\d+',tag):raise ValueError('Choose a standard release, not a prerelease')
    if release.get('tagName')!=tag or release.get('isDraft') is not False or release.get('isPrerelease') is not False:raise ValueError('Release is not a published standard release')
    if meta.get('commit')!=commit or meta.get('dirty') is not False or meta.get('version')!=tag[1:]:raise ValueError('Tagged source does not match package')
    if str(run.get('id'))!=str(meta.get('workflow_run')) or run.get('path')!='.github/workflows/build.yml' or run.get('head_sha')!=commit or run.get('head_branch')!=tag or run.get('event')!='push' or run.get('conclusion')!='success' or run.get('status')!='completed':raise ValueError('Tagged build/test workflow did not succeed')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tag',required=True);p.add_argument('--repo',required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    if not re.fullmatch(r'v\d+\.\d+\.\d+',args.tag):p.error('Standard release tag required')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',args.repo):p.error('Invalid repository')
    if args.output.exists() and any(args.output.iterdir()):p.error('Output directory must be empty')
    args.output.mkdir(parents=True,exist_ok=True)
    def gh(*command):return json.loads(subprocess.check_output(['gh',*command],text=True))
    release=gh('release','view',args.tag,'--repo',args.repo,'--json','tagName,isDraft,isPrerelease')
    commit=gh('api',f'repos/{args.repo}/commits/{args.tag}')['sha']
    subprocess.run(['gh','release','download',args.tag,'--repo',args.repo,'--dir',str(args.output)],check=True)
    meta,prefix=verify(args.output,args.tag[1:],commit)
    run_id=str(meta.get('workflow_run',''))
    if not re.fullmatch(r'\d+',run_id):raise ValueError('Missing build workflow ID')
    validate(args.tag,release,meta,commit,gh('api',f'repos/{args.repo}/actions/runs/{run_id}'))
    if output:=os.getenv('GITHUB_OUTPUT'):
        delimiter='notes_'+uuid.uuid4().hex
        with open(output,'a') as out:
            out.write(f'version={args.tag[1:]}\nfilename={args.output/(prefix+".zip")}\nchangelog<<{delimiter}\n{(args.output/"release-notes.md").read_text()}\n{delimiter}\n')
            if 'rumors' in meta.get('optional_addons',[]):
                out.write(f'rumors_filename={args.output/(prefix+"-rumors.zip")}\n')
    print('Verified tagged commit, successful CI, package checksums and DLL')

if __name__=='__main__':main()
