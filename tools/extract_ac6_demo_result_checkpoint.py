#!/usr/bin/env python3
"""Extract AC6 demo result routing and checkpoint registration callsites.

Inputs:
  MEMORY_IMAGE  reconstructed flat memory image (base 0x82000000)
  LISTING       complete disassembly produced from the corrected memory-image PE
  OUT_DIR       output directory

The utility emits text CSV only. It does not redistribute game bytes.
"""
from __future__ import annotations
import csv, re, struct, sys
from pathlib import Path

BASE = 0x82000000
RESULT_TABLE = 0x82391F10
CALL = re.compile(r'^([0-9a-fA-F]{8}):.*\bbl\s+0x822174a0\b')
LI_R5 = re.compile(r'^([0-9a-fA-F]{8}):.*\bli\s+5,\s*([0-9]+)\b')


def be32(data: bytes, va: int) -> int:
    off = va - BASE
    if off < 0 or off + 4 > len(data):
        raise ValueError(f'VA outside image: 0x{va:08X}')
    return struct.unpack_from('>I', data, off)[0]


def main() -> int:
    if len(sys.argv) != 4:
        print('usage: extract_ac6_demo_result_checkpoint.py MEMORY_IMAGE LISTING OUT_DIR')
        return 1
    image = Path(sys.argv[1]).read_bytes()
    lines = Path(sys.argv[2]).read_text(encoding='utf-8', errors='replace').splitlines()
    out = Path(sys.argv[3]); out.mkdir(parents=True, exist_ok=True)

    factories = [be32(image, RESULT_TABLE + i*4) for i in range(3)]
    with (out/'demo_result_factory_table.csv').open('w', newline='', encoding='utf-8') as f:
        w=csv.writer(f); w.writerow(['result','factory'])
        for i, value in enumerate(factories): w.writerow([i, f'0x{value:08X}'])

    rows=[]
    for i,line in enumerate(lines):
        m=CALL.match(line)
        if not m: continue
        callsite=int(m.group(1),16)
        size='dynamic_or_not_recovered'
        for prev in reversed(lines[max(0,i-12):i]):
            lm=LI_R5.match(prev)
            if lm:
                size=str(int(lm.group(2)))
                break
        rows.append([f'0x{callsite:08X}', size])
    with (out/'checkpoint_registration_callsites.csv').open('w', newline='', encoding='utf-8') as f:
        w=csv.writer(f); w.writerow(['callsite','nearest_immediate_r5_size']); w.writerows(rows)
    print(f'results={len(factories)} registrations={len(rows)}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
