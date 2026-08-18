#!/usr/bin/env python3
"""Verify exact PAL anchors for the AC6 renderer gate and XAudio affinity report."""

from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path

BASE = 0x82000000
EXPECTED_SHA256 = "b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01"

ANCHORS = {
    0x821ADB14: 0x2B030011,
    0x821ADB24: 0x2B040006,
    0x821ADB40: 0x913F5460,
    0x821ADB44: 0x917F5458,
    0x821ADB48: 0x917F545C,
    0x821ADCE8: 0x3D40821B,
    0x821ADCEC: 0x39200002,
    0x821ADCF0: 0x394ADAB8,
    0x821ADD44: 0x3860002F,
    0x821C667C: 0x4BFE75FD,
    0x821C5878: 0x80FF5460,
    0x821C5918: 0x4BFE8479,
    0x821BDC6C: 0x48006E7D,
    0x82355F88: 0x396B53A8,
    0x82355FC0: 0x394A53BC,
    0x82356028: 0x93EBA528,
    0x8236F810: 0x892D010C,
    0x8236F8CC: 0x816B0018,
    0x8236F8D4: 0x4E800421,
    0x823543AC: 0x2B050000,
    0x823543B0: 0x3B85FFF8,
    0x823543B8: 0x7EFCBB78,
    0x8235440C: 0x4BFFB545,
    0x8200E8E8: 0x82072DF4,
    0x8200E888: 0x82072E4C,
    0x82072E00: 0x82391A48,
    0x82072E58: 0x82391A48,
    0x82065370: 0x82351340,
    0x82065320: 0x823538E0,
}

EXPECTED_KESETEVENT_CALLS = {
    0x821C4AD0, 0x821C4FD4,
    0x82338C6C, 0x82338DC0, 0x82338E34, 0x82338EE0,
    0x82355128, 0x8235587C, 0x82355D90, 0x82355EA8,
}

def be32(data: bytes, va: int) -> int:
    off = va - BASE
    if off < 0 or off + 4 > len(data):
        raise ValueError(f"address outside image: 0x{va:08X}")
    return struct.unpack_from(">I", data, off)[0]

def sign_extend(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return value - (1 << bits) if value & sign else value

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("flat_image", type=Path)
    args = ap.parse_args()
    data = args.flat_image.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"wrong image SHA-256: {digest}")

    failures = []
    for va, expected in ANCHORS.items():
        actual = be32(data, va)
        if actual != expected:
            failures.append((va, expected, actual))

    calls = set()
    target = 0x823760F4
    for off in range(0, len(data) - 3, 4):
        word = struct.unpack_from(">I", data, off)[0]
        if word >> 26 != 18 or (word & 1) == 0:
            continue
        pc = BASE + off
        disp = sign_extend(word & 0x03FFFFFC, 26)
        destination = disp if ((word >> 1) & 1) else pc + disp
        if destination == target:
            calls.add(pc)
    if calls != EXPECTED_KESETEVENT_CALLS:
        raise SystemExit(
            "KeSetEvent census mismatch: "
            f"got {[hex(v) for v in sorted(calls)]}"
        )

    affinity_hash = hashlib.sha256(data[0x1A5390:0x1A5440]).hexdigest()
    if affinity_hash != "69cdc3705fab26d4c0cb9a2012a5a1750a6d822d736c7e671a1982af13b246bc":
        raise SystemExit(f"affinity hash mismatch: {affinity_hash}")

    if failures:
        for va, expected, actual in failures:
            print(f"FAIL 0x{va:08X}: expected 0x{expected:08X}, got 0x{actual:08X}")
        return 1

    print(f"image_sha256={digest}")
    print(f"anchors={len(ANCHORS)}")
    print(f"kesetevent_direct_calls={len(calls)}")
    print(f"affinity_function_sha256={affinity_hash}")
    print("status=PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
