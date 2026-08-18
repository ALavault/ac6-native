#!/usr/bin/env python3
"""Verify the AC6 PAL cert-monitor / renderer correction.

Input is the reconstructed flat memory image. The verifier emits no game bytes.
"""

from __future__ import annotations
import argparse, hashlib, struct
from pathlib import Path

BASE = 0x82000000
EXPECTED_SHA = "b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01"

ANCHORS = {
    0x82000610: 0x00010266,  # xboxkrnl ordinal 0x266: KeCertMonitorData
    0x820006E4: 0x00010059,  # xboxkrnl ordinal 0x59: KeDebugMonitorData
    0x820A4778: 0x39600011,  # record 12 runtime tag 17
    0x820A4780: 0x3960000D,  # adjacent record 13 key 13
    0x820A4788: 0x39694138,  # adjacent record 13 constructor
    0x820A4790: 0x39600006,  # adjacent record 13 runtime tag 6
    0x820A47E8: 0x3929000C,  # record stride 12
    0x820A4804: 0x1FCB000C,  # selected index * 12
    0x820A480C: 0x7D7E582E,  # load constructor from +4
    0x820A481C: 0x7D7E582E,  # load runtime tag from +8
    0x820A4820: 0x916300B8,  # publish runtime tag
    0x821ADCE8: 0x3D40821B,
    0x821ADCEC: 0x39200002,  # category 2
    0x821ADCF0: 0x394ADAB8,  # callback 0x821ADAB8
    0x821ADD44: 0x3860002F,  # command 47
    0x821ADB14: 0x2B030011,  # event 17
    0x821ADB24: 0x2B040006,  # channel 6
    0x821ADB40: 0x913F5460,
    0x821ADB74: 0x917F5460,
    0x821C5878: 0x80FF5460,
    0x821C58B4: 0x419A006C,  # zero -> 0x821C5920
    0x821C5920: 0x2B1A0000,  # continuation, not epilogue
    0x821C59AC: 0x4BFFAE4D,  # normal path call 0x821C07F8
    0x821C5918: 0x4BFE8479,  # first marker
    0x821C5A58: 0x4BFE8339,  # second marker
    0x821ADDC4: 0x3D40C002,
    0x821ADDC8: 0x61495800,  # packet header 0xC0025800
    0x821ADDF0: 0x3D20DEAD,
    0x821ADDF4: 0x6129BEEF,
}

FUNCTION_HASHES = {
    (0x820A45E0, 0x820A4898): "7affb8cc429db6813fed4fa372d4feada456700b91cce5d2181ff72226c754ad",
    (0x821ADAB8, 0x821ADC78): "723dcd6f1680e5bfa22657510176790ee9fec54c301b3d651a07a468ca4fdc85",
    (0x821ADC78, 0x821ADD90): "22ea5482a6bb9266f0de8a4bf275476efc6be36efbcaa2ad15ef724bcd4a721a",
    (0x821ADD90, 0x821ADE20): "0823ac0091c6ae7cf83cb011c7705f1f0e9680ccff6f8c249b2e99c620215ccc",
    (0x821C57D0, 0x821C5D90): "2fd82e4a50f82f7dddde8aee2e3e70281f63558a3aa7039cee5e9a1d618dc5fe",
}

EXPECTED_METRIC_TABLE = [
    (0x00000002,0x00020001,0),(0x00000008,0x00020003,3),
    (0x00000010,0x00020004,4),(0x00000020,0x00020005,5),
    (0x00000040,0x00020006,6),(0x00000080,0x00020007,7),
    (0x00000100,0x00020008,8),(0x00000200,0x00020009,9),
    (0x00000400,0x0002000A,10),(0x00000800,0x0002000B,11),
    (0x00001000,0x0002000C,12),(0x00002000,0x0002000D,13),
    (0x00004000,0x0002000E,14),(0x00008000,0x0002000F,15),
    (0x00010000,0x00020010,16),(0x00020000,0x00020011,17),
    (0x00040000,0x00020012,18),
]

def be32(data: bytes, va: int) -> int:
    off = va - BASE
    if off < 0 or off + 4 > len(data):
        raise ValueError(f"outside image: 0x{va:08X}")
    return struct.unpack_from(">I", data, off)[0]

def branch_target(pc: int, word: int):
    if word >> 26 != 18:
        return None
    disp = word & 0x03FFFFFC
    if disp & 0x02000000:
        disp -= 0x04000000
    return (disp if ((word >> 1) & 1) else pc + disp) & 0xFFFFFFFF, word & 1

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("flat_image", type=Path)
    args = ap.parse_args()
    data = args.flat_image.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA:
        raise SystemExit(f"wrong image: {digest}")

    failures = []
    for va, expected in ANCHORS.items():
        actual = be32(data, va)
        if actual != expected:
            failures.append(f"0x{va:08X}: expected 0x{expected:08X}, got 0x{actual:08X}")

    for (start,end), expected in FUNCTION_HASHES.items():
        actual = hashlib.sha256(data[start-BASE:end-BASE]).hexdigest()
        if actual != expected:
            failures.append(f"hash 0x{start:08X}..0x{end:08X}: {actual}")

    table = []
    for i in range(17):
        table.append(struct.unpack_from(">III", data, 0x823C2EA8-BASE+i*12))
    if table != EXPECTED_METRIC_TABLE:
        failures.append("metric table 0x823C2EA8 differs")

    stores_5460, stores_mask = [], []
    for off in range(0x90000, 0x375984, 4):
        word = struct.unpack_from(">I", data, off)[0]
        if word >> 26 == 36:
            imm = word & 0xFFFF
            pc = BASE + off
            if imm == 0x5460:
                stores_5460.append(pc)
            elif imm == 0xD2F4:
                stores_mask.append(pc)
    if stores_5460 != [0x821ADB40, 0x821ADB74]:
        failures.append(f"unexpected +0x5460 writers: {[hex(x) for x in stores_5460]}")
    if stores_mask != [0x821ADB34,0x821ADB68,0x821ADB84,0x821ADD0C]:
        failures.append(f"unexpected mask writers: {[hex(x) for x in stores_mask]}")

    calls = []
    for off in range(0x90000, 0x375984, 4):
        pc = BASE + off
        decoded = branch_target(pc, struct.unpack_from(">I", data, off)[0])
        if decoded == (0x821ADD90, 1):
            calls.append(pc)
    if calls != [0x821C5918, 0x821C5A58]:
        failures.append(f"unexpected marker callsites: {[hex(x) for x in calls]}")

    if failures:
        print("\n".join("FAIL " + item for item in failures))
        return 1
    print(f"image_sha256={digest}")
    print("anchors=%d" % len(ANCHORS))
    print("metric_records=17")
    print("marker_callsites=2")
    print("status=PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
