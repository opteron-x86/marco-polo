"""Check the shipped dialogue's binary boundaries, references and callback."""
import importlib.util
import itertools
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('rumors', ROOT/'tools/build-rumors.py')
rumors = importlib.util.module_from_spec(spec); spec.loader.exec_module(rumors)

def read_plugin(data):
    records, groups = {}, []
    def walk(start, end, parent=None):
        at = start
        while at < end:
            tag, length = struct.unpack_from('<4sI', data, at)
            if tag == b'GRUP':
                assert length >= 24 and at + length <= end
                label, kind = struct.unpack_from('<II', data, at+8)
                groups.append((label,kind))
                walk(at+24, at+length, label if kind == 7 else None)
                at += length; continue
            flags, form = struct.unpack_from('<II', data, at+8)
            assert form not in records and at+24+length <= end
            fields = []; pos = at+24
            while pos < at+24+length:
                key, size = struct.unpack_from('<4sH', data, pos)
                value = data[pos+6:pos+6+size]
                assert len(value) == size
                fields.append((key, value)); pos += 6+size
            assert pos == at+24+length
            records[form] = (tag,flags,fields,parent)
            at = pos
        assert at == end
    walk(0,len(data))
    return records,groups

class RumorPlugin(unittest.TestCase):
    def test_shipped_records(self):
        outputs = rumors.generate()
        for path,data in outputs.items(): self.assertEqual(path.read_bytes(),data)
        blob = (rumors.OUT/rumors.PLUGIN).read_bytes()
        records,groups = read_plugin(blob)
        self.assertEqual([struct.pack('<I',label) for label,kind in groups if kind == 0],
                         [b'GLOB',b'DIAL',b'QUST',b'DLBR'])
        self.assertEqual(set(records), {0, *range(0x01000800,0x0100080B)})
        self.assertEqual(records[0][1],0x200) # Old-runtime-compatible ESL range/header.
        self.assertEqual(struct.unpack_from('<H',blob,20)[0],44)
        header = dict(records[0][2])
        self.assertEqual(header[b'MAST'],b'Skyrim.esm\0')
        self.assertEqual(struct.unpack_from('<I',header[b'HEDR'],4)[0],len(records)-1+len(groups))
        self.assertEqual(struct.unpack_from('<I',header[b'HEDR'],8)[0],0x80B)
        self.assertEqual((rumors.OUT/'SEQ/MarcoPolo - Trade Rumors.seq').read_bytes(),struct.pack('<I',0x01000800))
        quest = dict(records[0x01000800][2])
        self.assertEqual(struct.unpack_from('<H',quest[b'DNAM'])[0],1)
        self.assertNotIn(b'VMAD',quest) # No quest scripts, aliases, objectives or inventory objects.
        self.assertEqual(set(quest), {b'EDID',b'FULL',b'DNAM',b'NEXT'})
        branch = dict(records[0x01000801][2]); topic = dict(records[0x01000802][2])
        self.assertEqual(branch[b'QNAM'],struct.pack('<I',0x01000800))
        self.assertEqual(branch[b'SNAM'],struct.pack('<I',0x01000802))
        self.assertEqual(topic[b'BNAM'],struct.pack('<I',0x01000801))
        self.assertEqual(topic[b'QNAM'],struct.pack('<I',0x01000800))
        self.assertEqual(topic[b'TIFC'],struct.pack('<I',7))
        infos = [(form,r) for form,r in records.items() if r[0]==b'INFO']
        self.assertEqual([form for form,_ in infos], [*range(0x01000805,0x0100080B),0x01000803])
        previous = 0
        for form,r in infos:
            self.assertEqual(r[3],0x01000802)
            fields = r[2]; info = dict(fields)
            # Reference only response data. No copied training/scene actions,
            # source links, prompt, inline placeholder or No-LIP flag.
            self.assertEqual(set(info),{b'EDID',b'VMAD',b'ENAM',b'PNAM',b'CNAM',b'DNAM',b'CTDA'})
            self.assertEqual(info[b'ENAM'],struct.pack('<HH',0x201,0))
            self.assertEqual(info[b'PNAM'],struct.pack('<I',previous))
            previous = form
            conditions = [v for k,v in fields if k==b'CTDA']
            self.assertTrue(all(len(v)==32 for v in conditions))
            # Enabled AND (innkeeper OR room-renter) AND alive, before voice gates.
            self.assertEqual([(v[0],struct.unpack_from('<H',v,8)[0],struct.unpack_from('<I',v,12)[0])
                              for v in conditions[:4]],
                             [(0,74,0x01000804),(1,71,0x5091B),(0,71,0x9CC97),(0,46,0)])
            self.check_end_fragment(info[b'VMAD'])

    def check_end_fragment(self, vmad):
        at = 6
        self.assertEqual(struct.unpack_from('<HHH',vmad),(5,2,1))
        def string():
            nonlocal at
            size, = struct.unpack_from('<H',vmad,at); at+=2
            value=vmad[at:at+size].decode(); at+=size; return value
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        # Unknown=2, fragment flags=2: End only. A value of 1 runs before speech.
        self.assertEqual(vmad[at:at+5],b'\0\0\0\2\2'); at+=5
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        self.assertEqual(vmad[at],1); at+=1
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        self.assertEqual(string(),'Fragment_0')
        self.assertEqual(at,len(vmad))

    def test_voice_selection_and_eligibility(self):
        records,_ = read_plugin((rumors.OUT/rumors.PLUGIN).read_bytes())
        infos = [r[2] for r in records.values() if r[0]==b'INFO']
        # Audited Skyrim.esm VTYP -> INFO pairs. Other voices retain Of course.
        expected = {0x13AE6:0x1041A4, 0x13AD1:0x3DF63, 0x13AD6:0xE3A37,
                    0x13AE7:0xD8DAD, 0x13AEF:0xC34A5, 0x14432:0x98E53}
        voices = [*expected, 0, 0x13AF1, 0xFE012800]
        def matches(fields, voice, enabled, innkeeper, rooms, dead):
            groups = []; either = False
            for key,v in fields:
                if key!=b'CTDA':continue
                self.assertIn(v[0],(0,1)) # Equality; optional OR with next.
                target,function,param = struct.unpack_from('<fH2xI',v,4)
                if function==74:
                    self.assertEqual(param,0x01000804); actual=enabled
                elif function==71:
                    actual={0x5091B:innkeeper,0x9CC97:rooms}[param]
                elif function==46:
                    self.assertEqual(param,0); actual=dead
                elif function==426:actual=voice==param
                else:self.fail(f'Unexpected condition function {function}')
                either |= actual==target
                if not v[0]&1:
                    groups.append(either); either=False
            self.assertFalse(either) # Last condition must close its OR group.
            return all(groups)
        for voice,enabled,innkeeper,rooms,dead in itertools.product(voices,(0,1),(0,1),(0,1),(0,1)):
            with self.subTest(voice=hex(voice),enabled=enabled,innkeeper=innkeeper,rooms=rooms,dead=dead):
                selected=[struct.unpack('<I',dict(fields)[b'DNAM'])[0] for fields in infos
                          if matches(fields,voice,enabled,innkeeper,rooms,dead)]
                wanted=[expected.get(voice,0xDBA22)] if enabled and (innkeeper or rooms) and not dead else []
                self.assertEqual(selected,wanted)

    def test_compiled_scripts(self):
        rumors.verify_scripts()
        # Verify the compiled string tables name the runtime binding and topic
        # entry point, independently of the source/PEX hash manifest.
        for name,required in [('MarcoPolo_Native',{'HearTradeRumors','Actor'}),
                              ('MarcoPolo_RumorTopic',{'Fragment_0','HearTradeRumors','MarcoPolo_Native','TopicInfo'})]:
            blob=(rumors.OUT/f'Scripts/{name}.pex').read_bytes(); at=16
            def string():
                nonlocal at
                size,=struct.unpack_from('>H',blob,at);at+=2
                s=blob[at:at+size].decode();at+=size;return s
            for _ in range(3):string()
            count,=struct.unpack_from('>H',blob,at);at+=2
            strings={string().lower() for _ in range(count)}
            self.assertTrue({s.lower() for s in required}.issubset(strings))

if __name__=='__main__':unittest.main()
