"""Exercise rejection paths with tampered/mixed release artifacts."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from artifacts import check_tag, verify

def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/f'{name}.py')
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

class ReleaseTests(unittest.TestCase):
    def test_version_gate(self):
        for tag in ('v0.1.0','v0.1.0-alpha.1'):check_tag(tag,'0.1.0')
        for tag in ('v0.1.1','0.1.0','v0.1.0\n','v0.1.0-../../bad'):
            with self.assertRaises(ValueError):check_tag(tag,'0.1.0')

    def test_archive_tampering(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);commit='a'*40;prefix='Marco-Polo-0.1.0-aaaaaaaa';dll=b'fixture DLL'
            def archive(suffix,files):
                with zipfile.ZipFile(d/(prefix+suffix+'.zip'),'w') as z:
                    for name,body in files.items():z.writestr(name,body)
            archive('',{'SKSE/Plugins/MarcoPolo.dll':dll,**{f'SKSE/Plugins/MarcoPolo/{n}.json':'{}' for n in ('settings','classification','economy')}})
            archive('-missives',{'Marco Polo - Missives.esp':b'TES4fixture'})
            archive('-symbols',{'MarcoPolo.pdb':b'fixture'})
            archive('-source',{n:b'fixture' for n in ('CMakeLists.txt','src/runtime/Plugin.cpp','src/core/Economy.cpp','LICENSE')})
            meta={'version':'0.1.0','commit':commit,'dirty':False,'dll_sha256':hashlib.sha256(dll).hexdigest()}
            (d/'build-info.json').write_text(json.dumps(meta));(d/'release-notes.md').write_text('Fixture')
            def sums():
                (d/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(d.iterdir()) if p.name!='SHA256SUMS.txt'))
            sums();verify(d,'0.1.0',commit)
            (d/(prefix+'.zip')).write_bytes(b'changed')
            with self.assertRaises(ValueError):verify(d,'0.1.0',commit)
            # Checksums alone cannot bless a DLL that differs from its manifest.
            archive('',{'SKSE/Plugins/MarcoPolo.dll':b'wrong'});sums()
            with self.assertRaises(ValueError):verify(d,'0.1.0',commit)

    def test_nexus_gate(self):
        prepare=module('prepare-nexus');commit='a'*40
        release={'tagName':'v0.1.0','isDraft':False,'isPrerelease':False}
        meta={'version':'0.1.0','commit':commit,'dirty':False,'workflow_run':'10'}
        run={'id':10,'path':'.github/workflows/build.yml','head_sha':commit,'head_branch':'v0.1.0','event':'push','conclusion':'success','status':'completed'}
        prepare.validate('v0.1.0',release,meta,commit,run)
        for key,value in (('head_sha','b'*40),('conclusion','failure'),('event','pull_request'),('status','in_progress'),('path','other.yml')):
            with self.assertRaises(ValueError):prepare.validate('v0.1.0',release,meta,commit,{**run,key:value})
        with self.assertRaises(ValueError):prepare.validate('v0.1.0-alpha.1',release,meta,commit,run)

class AddonTests(unittest.TestCase):
    def test_only_new_light_book_records(self):
        blob=module('build-addon').build()
        self.assertEqual(blob,(ROOT/'addons/missives/Marco Polo - Missives.esp').read_bytes())
        self.assertEqual(blob[:4],b'TES4');self.assertEqual(struct.unpack_from('<I',blob,8)[0],0x200)
        offset=24+struct.unpack_from('<I',blob,4)[0]
        self.assertEqual(blob[offset:offset+4],b'GRUP');self.assertEqual(blob[offset+8:offset+12],b'BOOK')
        self.assertEqual(offset+struct.unpack_from('<I',blob,offset+4)[0],len(blob))
        offset+=24;forms=[]
        while offset<len(blob):
            self.assertEqual(blob[offset:offset+4],b'BOOK');size=struct.unpack_from('<I',blob,offset+4)[0]
            forms.append(struct.unpack_from('<I',blob,offset+12)[0]);end=offset+24+size;offset+=24
            fields=[]
            while offset<end:
                fields.append(blob[offset:offset+4]);n=struct.unpack_from('<H',blob,offset+4)[0];offset+=6+n
            self.assertEqual(offset,end);self.assertNotIn(b'VMAD',fields)
            self.assertTrue({b'EDID',b'FULL',b'DESC',b'DATA',b'INAM'}.issubset(fields))
        self.assertEqual(forms,list(range(0x03000800,0x03000812)))

if __name__=='__main__':unittest.main()
