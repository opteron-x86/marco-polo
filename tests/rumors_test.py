"""Check the shipped dialogue's binary boundaries, references and callback."""
import importlib.util
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
        self.assertEqual(set(records), {0,0x01000800,0x01000801,0x01000802,0x01000803,0x01000804})
        self.assertEqual(records[0][1],0x200) # Old-runtime-compatible ESL range/header.
        self.assertEqual(struct.unpack_from('<H',blob,20)[0],44)
        header = dict(records[0][2])
        self.assertEqual(header[b'MAST'],b'Skyrim.esm\0')
        self.assertEqual(struct.unpack_from('<I',header[b'HEDR'],4)[0],len(records)-1+len(groups))
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
        self.assertEqual(topic[b'TIFC'],struct.pack('<I',1))
        self.assertEqual(records[0x01000803][3],0x01000802)
        fields = records[0x01000803][2]; info = dict(fields)
        self.assertEqual(info[b'ENAM'],struct.pack('<HH',0x801,0))
        self.assertTrue(info[b'NAM1'].endswith(b'\0'))
        self.assertTrue(info[b'NAM1'][:-1].decode('utf-8').strip())
        # Enabled AND (innkeeper OR room-renter) AND alive.
        conditions = [v for k,v in fields if k==b'CTDA']
        self.assertTrue(all(len(v)==32 for v in conditions))
        self.assertEqual([(v[0],struct.unpack_from('<H',v,8)[0],struct.unpack_from('<I',v,12)[0]) for v in conditions],
                         [(0,74,0x01000804),(1,71,0x5091B),(0,71,0x9CC97),(0,46,0)])
        vmad = info[b'VMAD']; at = 6
        self.assertEqual(struct.unpack_from('<HHH',vmad),(5,2,1))
        def string():
            nonlocal at
            size, = struct.unpack_from('<H',vmad,at); at+=2
            value=vmad[at:at+size].decode(); at+=size; return value
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        self.assertEqual(vmad[at:at+5],b'\0\0\0\2\1'); at+=5
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        self.assertEqual(vmad[at],1); at+=1
        self.assertEqual(string(),'MarcoPolo_RumorTopic')
        self.assertEqual(string(),'Fragment_0')
        self.assertEqual(at,len(vmad))

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
