#!/usr/bin/env python3
"""Check the architecture, exports and deliberately narrow runtime metadata."""
import argparse
from pathlib import Path
import struct

def audit(data):
    u16=lambda off:struct.unpack_from('<H',data,off)[0]
    u32=lambda off:struct.unpack_from('<I',data,off)[0]
    if data[:2]!=b'MZ':raise ValueError('Not a PE image')
    pe=u32(0x3c);opt=pe+24
    if data[pe:pe+4]!=b'PE\0\0' or u16(pe+4)!=0x8664 or u16(opt)!=0x20b:raise ValueError('Not a Windows x64 DLL')
    sections=opt+u16(pe+20)
    def offset(rva):
        for i in range(u16(pe+6)):
            size,va,raw_size,raw=struct.unpack_from('<IIII',data,sections+i*40+8)
            if va<=rva<va+max(size,raw_size):return raw+rva-va
        raise ValueError('Unmapped RVA')
    exports=offset(u32(opt+112));functions,names,ordinals=(offset(u32(exports+i)) for i in (28,32,36));symbols={}
    for i in range(u32(exports+24)):
        pos=offset(u32(names+i*4));name=data[pos:data.index(b'\0',pos)].decode();symbols[name]=u32(functions+u16(ordinals+i*2)*4)
    if not {'SKSEPlugin_Load','SKSEPlugin_Version'}.issubset(symbols):raise ValueError('Missing SKSE exports')
    v=offset(symbols['SKSEPlugin_Version'])
    if u32(v)!=1 or not(u32(v+776)&1) or not(u32(v+776)&4):raise ValueError('Incorrect Address Library/struct metadata')
    if not(u32(v+772)&2):raise ValueError('Current Address Library metadata is required')
    if u32(v+780)!=((1<<24)|(6<<16)|(1170<<4)) or any(u32(v+780+i*4) for i in range(1,16)):raise ValueError('Unexpected runtime whitelist')
    return symbols

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('dll',type=Path);args=p.parse_args()
    print('PE audit passed:', ', '.join(audit(args.dll.read_bytes())))
