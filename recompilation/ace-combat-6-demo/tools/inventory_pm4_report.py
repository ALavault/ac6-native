#!/usr/bin/env python3
"""Build a byte-qualified, payload-bounded PM4 inventory from a probe report."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import tempfile
from collections import Counter
from pathlib import Path

TARGET_ID = "ac6-demo-xbox360-pal"
XEX_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
REGISTER_PATTERN = re.compile(
    r"XE_GPU_REGISTER\((0x[0-9A-Fa-f]+),\s*[^,]+,\s*([A-Za-z0-9_]+)\)"
)
TYPE3_OPCODES = {
    0x21: "PM4_REG_RMW",
    0x2B: "PM4_IM_LOAD_IMMEDIATE",
    0x36: "PM4_DRAW_INDX_2",
    0x3B: "PM4_INVALIDATE_STATE",
    0x3C: "PM4_WAIT_REG_MEM",
    0x45: "PM4_COND_WRITE",
    0x46: "PM4_EVENT_WRITE",
    0x48: "PM4_ME_INIT",
    0x50: "PM4_SET_BIN_MASK",
    0x51: "PM4_SET_BIN_SELECT",
    0x54: "PM4_INTERRUPT",
    0x58: "PM4_EVENT_WRITE_SHD",
    0x60: "PM4_SET_BIN_MASK_LO",
    0x61: "PM4_SET_BIN_MASK_HI",
    0x62: "PM4_SET_BIN_SELECT_LO",
    0x63: "PM4_SET_BIN_SELECT_HI",
    0x64: "PM4_XE_SWAP",
}
OPAQUE_STORAGE_WRITES = {
    0x0A02: 0xC0100000,
    0x0A03: 0x07F00000,
    0x0A04: 0xC0000000,
    0x0A05: 0x00100000,
    0x2290: 0x00000000,
    0x2291: 0x00000000,
    **{index: 0x00000000 for index in range(0x230B, 0x2312)},
    0x2313: 0x00000000,
    0x2314: 0x00000000,
}


class InventoryError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def dword_bytes(words: list[int]) -> bytes:
    return b"".join(struct.pack(">I", word) for word in words)


def load_registers(path: Path) -> tuple[dict[int, str], str]:
    data = path.read_bytes()
    names = {int(address, 16): name for address, name in REGISTER_PATTERN.findall(data.decode())}
    if not names:
        raise InventoryError("Xenos register table contains no register records")
    return names, sha256_bytes(data)


def bounded_payload(payload: list[int], opcode: int | None) -> dict[str, object]:
    raw = dword_bytes(payload)
    # Immediate shader payloads may contain proprietary microcode. Never copy
    # their dwords into the durable inventory; retain only the exact hash and
    # the two-word packet descriptor.
    preview_limit = 2 if opcode == 0x2B else 8
    preview = payload[:preview_limit]
    return {
        "dword_count": len(payload),
        "byte_sha256": sha256_bytes(raw),
        "preview_dwords": [f"0x{word:08X}" for word in preview],
        "omitted_dwords": len(payload) - len(preview),
        "microcode_redacted": opcode == 0x2B and len(payload) > 2,
    }


def parse_buffer(buffer: dict[str, object], register_names: dict[int, str],
                 ib_addresses: set[int]) -> tuple[dict[str, object], dict[str, object] | None]:
    address = int(buffer["address"])
    words = [int(word) for word in buffer["dwords"]]
    expected_count = int(buffer["dword_count"])
    if expected_count != len(words) or int(buffer["captured_dword_count"]) != len(words):
        raise InventoryError(f"IB 0x{address:08X} is not captured completely")
    if bool(buffer["truncated"]):
        raise InventoryError(f"IB 0x{address:08X} is marked truncated")
    actual_hash = sha256_bytes(dword_bytes(words))
    if actual_hash != buffer["byte_sha256"]:
        raise InventoryError(f"IB 0x{address:08X} hash mismatch")

    packets: list[dict[str, object]] = []
    cursor = 0
    first_unknown = None
    type_counts: Counter[str] = Counter()
    opcode_counts: Counter[str] = Counter()
    register_writes: set[int] = set()
    references: set[int] = set()
    while cursor < len(words):
        start = cursor
        header = words[cursor]
        cursor += 1
        packet_type = header >> 30
        record: dict[str, object] = {
            "offset_dword": start,
            "header": f"0x{header:08X}",
            "type": packet_type,
        }
        payload: list[int] = []
        opcode = None
        if packet_type == 2:
            record["kind"] = "type2-nop"
            record["count"] = 0
        elif packet_type in (0, 3):
            count = ((header >> 16) & 0x3FFF) + 1
            if count > len(words) - cursor:
                raise InventoryError(f"truncated packet at IB 0x{address:08X} dword {start}")
            payload = words[cursor:cursor + count]
            cursor += count
            record["count"] = count
            if packet_type == 0:
                base = header & 0x7FFF
                one_register = bool((header >> 15) & 1)
                registers = [base if one_register else base + index for index in range(count)]
                register_writes.update(registers)
                decoded = [{"index": f"0x{index:04X}",
                            "name": register_names.get(index),
                            "status": ("opaque-storage-qualified"
                                       if OPAQUE_STORAGE_WRITES.get(index) == value
                                       else "known" if register_names.get(index) and
                                       not str(register_names[index]).startswith("UNKNOWN_")
                                       else "unknown")}
                           for index, value in zip(registers, payload)]
                record.update({"kind": "type0-register-write", "base_register": f"0x{base:04X}",
                               "one_register": one_register, "registers": decoded})
                unknown = next((item for item in decoded if item["status"] == "unknown"), None)
                if unknown is not None and first_unknown is None:
                    first_unknown = {"kind": "register", "ib_address": f"0x{address:08X}",
                                     "offset_dword": start, **unknown}
            else:
                opcode = (header >> 8) & 0x7F
                opcode_name = TYPE3_OPCODES.get(opcode)
                record.update({"kind": "type3", "opcode": f"0x{opcode:02X}",
                               "opcode_name": opcode_name,
                               "predicated": bool(header & 1)})
                opcode_counts[opcode_name or f"UNKNOWN_0x{opcode:02X}"] += 1
                if opcode_name is None and first_unknown is None:
                    first_unknown = {"kind": "opcode", "ib_address": f"0x{address:08X}",
                                     "offset_dword": start, "opcode": f"0x{opcode:02X}"}
        else:
            raise InventoryError(f"unsupported packet type {packet_type} at IB 0x{address:08X} dword {start}")

        references.update(word for word in payload if word in ib_addresses)
        record["payload"] = bounded_payload(payload, opcode)
        record["packet_byte_sha256"] = sha256_bytes(dword_bytes([header, *payload]))
        packets.append(record)
        type_counts[str(packet_type)] += 1

    return ({
        "address": f"0x{address:08X}",
        "dword_count": len(words),
        "byte_sha256": actual_hash,
        "packet_count": len(packets),
        "packet_type_counts": dict(sorted(type_counts.items())),
        "type3_opcode_counts": dict(sorted(opcode_counts.items())),
        "register_write_count": len(register_writes),
        "referenced_ib_addresses": [f"0x{item:08X}" for item in sorted(references)],
        "packets": packets,
    }, first_unknown)


def build_inventory(report_path: Path, register_table: Path) -> dict[str, object]:
    report_bytes = report_path.read_bytes()
    report = json.loads(report_bytes)
    target = report.get("target", {})
    if target.get("id") != TARGET_ID or target.get("xex_sha256") != XEX_SHA256:
        raise InventoryError("probe report target identity mismatch")
    ring = report.get("graphics", {}).get("ring", {})
    buffers = ring.get("indirect_buffers")
    if not isinstance(buffers, list) or not buffers:
        raise InventoryError("probe report contains no indirect buffers")
    register_names, register_table_hash = load_registers(register_table)
    addresses = {int(buffer["address"]) for buffer in buffers}
    inventories = []
    first_unknown = None
    for buffer in buffers:
        inventory, unknown = parse_buffer(buffer, register_names, addresses)
        inventories.append(inventory)
        if first_unknown is None and unknown is not None:
            first_unknown = unknown
    return {
        "schema": "ac6-demo-pm4-inventory/v1",
        "target": {"id": TARGET_ID, "module": target.get("module"),
                   "xex_sha256": XEX_SHA256},
        "source": {"probe_report_sha256": sha256_bytes(report_bytes),
                   "register_table_sha256": register_table_hash,
                   "register_authority": "local Xenos architecture reference; not AC6 binary evidence"},
        "ring": {key: ring.get(key) for key in
                 ("base", "capacity_dwords", "read_pointer", "write_pointer", "submissions")},
        "buffers": inventories,
        "first_unknown": first_unknown,
        "semantic_decode": "blocked_at_first_unknown" if first_unknown else "structurally_known",
    }


def atomic_write_new(path: Path, document: dict[str, object]) -> None:
    if path.exists():
        raise InventoryError(f"refusing to overwrite output: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        Path(temporary).unlink(missing_ok=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--register-table", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        atomic_write_new(arguments.output,
                         build_inventory(arguments.report, arguments.register_table))
    except (OSError, ValueError, KeyError, json.JSONDecodeError, InventoryError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
