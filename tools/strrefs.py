"""Instructions in tp.exe from VA up to N bytes that name a string: py -3 tools/strrefs.py 0x405630 0x900"""
import re
import sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
pe = pefile.PE('game/tp.exe')
img = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
va, n = int(sys.argv[1], 16), int(sys.argv[2], 16)
for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(img[va - base:va - base + n], va):
    for tok in re.findall(r'0x[0-9a-f]+', i.op_str):
        a = int(tok, 16)
        if base <= a < base + len(img):
            s = img[a - base:a - base + 60].split(b'\0')[0]
            if len(s) >= 2 and all(32 <= c < 127 for c in s):
                print('%08X %-6s %-40s ; %r' % (i.address, i.mnemonic, i.op_str, s.decode()))
