#!/usr/bin/env python3
"""Validate and canonicalize bounded AC6 Xenos replay capsules.

Capsules contain derived MMIO/PM4 words only.  They never embed XEX, ISO, PAC,
shader or other retail byte ranges.  Validation is fail-closed and reports the
first packet offset that cannot be interpreted.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any


SCHEMA = "ac6.xenos-capsule.v1"
MAX_EVENTS = 1_000_000
MAX_PM4_DWORDS = 1 << 20
MAX_PACKET_DWORDS = 0x4000
TYPE0 = 0
TYPE1 = 1
TYPE2 = 2
TYPE3 = 3
WAIT_FOR_IDLE = 0x26
WAIT_REG_MEM = 0x3C
DRAW_INDX = 0x22
DRAW_INDX_2 = 0x36
SET_CONSTANT = 0x2D
CONTEXT_UPDATE = 0x5E
INTERRUPT = 0x54
XE_SWAP = 0x64
INDIRECT_BUFFER = 0x3F
SWAP_SIGNATURE = 0x53574150


def _valid_primitive(value: int) -> bool:
    return 1 <= value <= 8 or 0x0C <= value <= 0x12


class CapsuleError(ValueError):
    """Malformed capsule with stable machine-readable location."""

    def __init__(self, message: str, offset: int | None = None) -> None:
        super().__init__(message)
        self.offset = offset


def _integer(value: Any, name: str, *, minimum: int = 0, maximum: int | None = None) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise CapsuleError(f"{name} must be an integer")
    if value < minimum or (maximum is not None and value > maximum):
        raise CapsuleError(f"{name} outside [{minimum}, {maximum}]")
    return value


def _packet(words: list[int], offset: int) -> int:
    if not words:
        raise CapsuleError("PM4 header is missing", offset)
    header = _integer(words[0], "PM4 header", maximum=0xFFFFFFFF)
    packet_type = header >> 30
    count = 2 if packet_type == TYPE1 else ((header >> 16) & 0x3FFF) + 1
    if count > MAX_PACKET_DWORDS or len(words) < count + 1:
        raise CapsuleError("PM4 packet payload is truncated", offset)
    payload = words[1 : count + 1]
    if packet_type == TYPE0:
        base = header & 0x7FFF
        if not (header & 0x8000) and base + count > 0x4000:
            raise CapsuleError("TYPE0 register range exceeds Xenos state", offset)
        return count + 1
    if packet_type == TYPE1:
        if count != 2:
            raise CapsuleError("TYPE1 packet must contain two payload values", offset)
        for index in ((header & 0x7FF), ((header >> 11) & 0x7FF)):
            if index >= 0x4000:
                raise CapsuleError("TYPE1 register is outside Xenos state", offset)
        return count + 1
    if packet_type == TYPE2:
        if header & 0x3FFFFFFF:
            raise CapsuleError("TYPE2 NOP has non-zero payload bits", offset)
        return 1
    if packet_type != TYPE3:
        raise CapsuleError("unknown PM4 packet type", offset)
    opcode = (header >> 8) & 0x7F
    if header & 1:
        raise CapsuleError("predicated TYPE3 packets are unsupported", offset)
    if opcode == WAIT_FOR_IDLE:
        if count != 1 or payload[0] != 0:
            raise CapsuleError("WAIT_FOR_IDLE requires a zero payload", offset + 1)
    elif opcode == WAIT_REG_MEM:
        if count != 5 or payload[0] & ~0x17:
            raise CapsuleError("WAIT_REG_MEM envelope is invalid", offset + 1)
    elif opcode == DRAW_INDX:
        if count < 2 or not _valid_primitive(payload[1] & 0x3F) or ((payload[1] >> 16) & 0xFFFF) == 0 or ((payload[1] >> 6) & 3) in (1, 3):
            raise CapsuleError("DRAW_INDX payload or source selector is invalid", offset + 1)
        if ((payload[1] >> 6) & 3) == 0 and (count < 4 or payload[2] == 0):
            raise CapsuleError("DRAW_INDX index address is invalid", offset + 2)
    elif opcode == DRAW_INDX_2:
        if count < 1 or not _valid_primitive(payload[0] & 0x3F) or ((payload[0] >> 16) & 0xFFFF) == 0 or ((payload[0] >> 6) & 3) != 2:
            raise CapsuleError("DRAW_INDX_2 payload is invalid", offset + 1)
    elif opcode == SET_CONSTANT:
        if count < 2 or ((payload[0] >> 16) & 0xFF) != 4:
            raise CapsuleError("SET_CONSTANT only accepts direct registers", offset + 1)
        base = payload[0] & 0x7FF
        if base + count - 1 > 0x4000:
            raise CapsuleError("SET_CONSTANT register range exceeds Xenos state", offset + 1)
    elif opcode == CONTEXT_UPDATE:
        if count != 1 or payload[0] != 0:
            raise CapsuleError("CONTEXT_UPDATE payload is invalid", offset + 1)
    elif opcode == INTERRUPT:
        if count != 1 or payload[0] > 0x3F:
            raise CapsuleError("INTERRUPT payload is invalid", offset + 1)
    elif opcode == XE_SWAP:
        if count < 4 or payload[0] != SWAP_SIGNATURE or not (0 < payload[2] <= 4096 and 0 < payload[3] <= 4096):
            raise CapsuleError("XE_SWAP signature or dimensions are invalid", offset + 1)
    elif opcode == INDIRECT_BUFFER:
        if count != 2 or payload[0] == 0 or not (0 < payload[1] <= 1 << 20):
            raise CapsuleError("INDIRECT_BUFFER range is invalid", offset + 1)
    else:
        raise CapsuleError(f"unsupported TYPE3 opcode 0x{opcode:02x}", offset)
    return count + 1


def validate_capsule(document: dict[str, Any]) -> dict[str, Any]:
    if document.get("schema") != SCHEMA:
        raise CapsuleError(f"schema must be {SCHEMA}")
    if document.get("target") != "ntsc-uj":
        raise CapsuleError("capsule target must be ntsc-uj")
    source = document.get("source")
    if not isinstance(source, dict):
        raise CapsuleError("source identity is required")
    for key in ("xex_sha256", "iso_sha256", "ghidra_project", "route_sha256"):
        if not isinstance(source.get(key), str) or not source[key]:
            raise CapsuleError(f"source.{key} is required")
    ring = document.get("ring")
    if not isinstance(ring, dict):
        raise CapsuleError("ring contract is required")
    ring_size = _integer(ring.get("dword_count"), "ring.dword_count", minimum=64, maximum=1 << 20)
    if ring_size & (ring_size - 1):
        raise CapsuleError("ring.dword_count must be a power of two")
    events = document.get("events")
    if not isinstance(events, list) or not events:
        raise CapsuleError("events must be a non-empty list")
    if len(events) > MAX_EVENTS:
        raise CapsuleError("events exceed capsule bound")
    pm4_words: list[int] = []
    for index, event in enumerate(events):
        if not isinstance(event, dict):
            raise CapsuleError(f"event {index} must be an object")
        kind = event.get("kind")
        allowed = {
            "mmio": {"kind", "address", "value"},
            "interrupt": {"kind", "vector"},
            "pm4": {"kind", "dwords"},
            "present": {"kind", "surface", "sequence"},
        }.get(kind)
        if allowed is None:
            raise CapsuleError(f"event {index} has unsupported kind {kind!r}")
        unknown = sorted(set(event) - allowed)
        if unknown:
            raise CapsuleError(f"event {index} has unsupported fields: {', '.join(unknown)}")
        if kind == "mmio":
            _integer(event.get("address"), f"event {index}.address", maximum=0xFFFFFFFF)
            _integer(event.get("value"), f"event {index}.value", maximum=0xFFFFFFFF)
        elif kind == "interrupt":
            _integer(event.get("vector"), f"event {index}.vector", maximum=0xFFFFFFFF)
        elif kind == "pm4":
            words = event.get("dwords")
            if not isinstance(words, list) or not words:
                raise CapsuleError(f"event {index}.dwords must be non-empty")
            for word in words:
                _integer(word, f"event {index}.dword", maximum=0xFFFFFFFF)
            pm4_words.extend(words)
            if len(pm4_words) > MAX_PM4_DWORDS:
                raise CapsuleError("PM4 stream exceeds capsule bound")
        elif kind == "present":
            _integer(event.get("surface"), f"event {index}.surface", maximum=15)
            _integer(event.get("sequence"), f"event {index}.sequence", maximum=0xFFFFFFFFFFFFFFFF)
    offset = 0
    while offset < len(pm4_words):
        offset += _packet(pm4_words[offset:], offset)
    return {
        "schema": SCHEMA,
        "target": "ntsc-uj",
        "source": source,
        "ring": {"dword_count": ring_size},
        "events": events,
        "validated": {"event_count": len(events), "pm4_dword_count": len(pm4_words)},
    }


def canonical_bytes(document: dict[str, Any]) -> bytes:
    return (json.dumps(validate_capsule(document), sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capsule", type=pathlib.Path)
    parser.add_argument("--write-canonical", type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        document = json.loads(args.capsule.read_text(encoding="utf-8"))
        if not isinstance(document, dict):
            raise CapsuleError("capsule root must be an object")
        canonical = canonical_bytes(document)
        if args.write_canonical:
            args.write_canonical.write_bytes(canonical)
        else:
            sys.stdout.buffer.write(canonical)
        return 0
    except (OSError, json.JSONDecodeError, CapsuleError) as error:
        offset = getattr(error, "offset", None)
        suffix = f" at dword offset {offset}" if offset is not None else ""
        print(f"xenos-capsule: {error}{suffix}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
