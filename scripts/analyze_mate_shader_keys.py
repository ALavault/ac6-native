#!/usr/bin/env python3
"""Compare MATE material word 0 with qualified NSXR context keys.

The output contains metadata only. Retail DATA payloads are extracted into a
temporary directory and removed before the process exits.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import subprocess
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any


def require_range(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise ValueError(f"{label} exceeds its payload")


def read_u16(data: bytes, offset: int, *, big_endian: bool) -> int:
    require_range(data, offset, 2, "16-bit word")
    return int.from_bytes(data[offset : offset + 2], "big" if big_endian else "little")


def read_u32(data: bytes, offset: int, *, big_endian: bool) -> int:
    require_range(data, offset, 4, "32-bit word")
    return int.from_bytes(data[offset : offset + 4], "big" if big_endian else "little")


def fhm_member(data: bytes, index: int) -> bytes:
    if len(data) < 12 or data[:4] != b"FHM ":
        raise ValueError("path component does not refer to an FHM payload")
    byte_order = data[5]
    if byte_order not in (0, 1):
        raise ValueError("FHM byte-order selector is invalid")
    big_endian = byte_order == 1
    directory_offset = read_u16(data, 6, big_endian=big_endian)
    count = read_u32(data, directory_offset, big_endian=big_endian)
    if index < 0 or index >= count:
        raise ValueError(f"FHM member {index} is outside count {count}")
    arrays = directory_offset + 4
    require_range(data, arrays, count * 16, "FHM directory")
    offset = read_u32(data, arrays + index * 4, big_endian=big_endian)
    size = read_u32(data, arrays + (count + index) * 4, big_endian=big_endian)
    require_range(data, offset, size, "FHM member")
    return data[offset : offset + size]


def resolve_path(data: bytes, path: str) -> bytes:
    payload = data
    for component in path.split("."):
        payload = fhm_member(payload, int(component, 10))
    return payload


def parse_mate_materials(data: bytes) -> list[dict[str, int]]:
    if len(data) < 0x18 or data[:4] != b"MATE":
        raise ValueError("payload is not a complete MATE wrapper")
    material_count = read_u16(data, 4, big_endian=True)
    material_table = read_u32(data, 0x0C, big_endian=True)
    batch_table = read_u32(data, 0x10, big_endian=True)
    if material_table & 0xF or batch_table & 0xF or batch_table < material_table:
        raise ValueError("MATE material table bounds are invalid")
    if material_count > (batch_table - material_table) // 0x10:
        raise ValueError("MATE material table exceeds its region")

    materials: list[dict[str, int]] = []
    for index in range(material_count):
        table_offset = material_table + index * 0x10
        require_range(data, table_offset, 0x10, "MATE material table entry")
        if any(
            read_u32(data, table_offset + word * 4, big_endian=True) != 0
            for word in range(1, 4)
        ):
            raise ValueError("MATE material table reserved word is nonzero")
        material_offset = read_u32(data, table_offset, big_endian=True)
        require_range(data, material_offset, 0x20, "MATE material record")
        materials.append(
            {
                "material_index": index,
                "material_offset": material_offset,
                "word_00": read_u32(data, material_offset, big_endian=True),
                "word_04": read_u32(data, material_offset + 4, big_endian=True),
                "halfword_08": read_u16(data, material_offset + 8, big_endian=True),
                "texture_count_0a": read_u16(
                    data, material_offset + 0x0A, big_endian=True
                ),
            }
        )
    return materials


def load_nsxr_keys(path: Path) -> tuple[set[int], str]:
    raw = path.read_bytes()
    document = json.loads(raw)
    keys = {
        int(entry["key"])
        for container in document["containers"]
        for entry in container["entries"]
    }
    return keys, hashlib.sha256(raw).hexdigest()


def load_mate_rows(path: Path) -> list[dict[str, str]]:
    rows = [row for row in csv.DictReader(path.open(newline="")) if row["kind"] == "mate"]
    rows.sort(key=lambda row: (int(row["entry"]), tuple(map(int, row["path"].split(".")))))
    return rows


def extract_entry(
    extractor: Path,
    data_table: Path,
    data_00: Path,
    data_01: Path,
    entry: int,
    output: Path,
) -> bytes:
    subprocess.run(
        [
            str(extractor),
            str(data_table),
            str(data_00),
            str(data_01),
            str(entry),
            str(output),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    return output.read_bytes()


def analyze(args: argparse.Namespace) -> dict[str, Any]:
    nsxr_keys, nsxr_key_source_sha256 = load_nsxr_keys(args.nsxr_keys)
    mate_rows = load_mate_rows(args.manifest)
    rows_by_entry: dict[int, list[dict[str, str]]] = {}
    for row in mate_rows:
        rows_by_entry.setdefault(int(row["entry"]), []).append(row)

    result_rows: list[dict[str, Any]] = []
    key_counts: Counter[int] = Counter()
    missing_key_counts: Counter[int] = Counter()
    with tempfile.TemporaryDirectory(prefix="ac6-mate-key-scan-") as temporary:
        temporary_path = Path(temporary)
        for entry, rows in rows_by_entry.items():
            entry_path = temporary_path / f"entry-{entry}.bin"
            entry_data = extract_entry(
                args.extractor,
                args.data_table,
                args.data_00,
                args.data_01,
                entry,
                entry_path,
            )
            for row in rows:
                payload = resolve_path(entry_data, row["path"])
                expected_size = int(row["size"])
                if len(payload) != expected_size:
                    raise ValueError(
                        f"entry {entry} path {row['path']} size {len(payload)} "
                        f"does not match manifest {expected_size}"
                    )
                materials = parse_mate_materials(payload)
                for material in materials:
                    key = material["word_00"]
                    exact_match = key in nsxr_keys
                    key_counts[key] += 1
                    if not exact_match:
                        missing_key_counts[key] += 1
                    result_rows.append(
                        {
                            "entry": entry,
                            "path": row["path"],
                            "mate_size": len(payload),
                            **material,
                            "word_00_is_nsxr_context_key": exact_match,
                        }
                    )

    matched = sum(row["word_00_is_nsxr_context_key"] for row in result_rows)
    return {
        "schema": "ac6-mate-nsxr-key-coverage/v1",
        "target_id": args.target_id,
        "xex_sha256": args.xex_sha256,
        "inputs": {
            "manifest": str(args.manifest),
            "manifest_sha256": hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
            "nsxr_keys": str(args.nsxr_keys),
            "nsxr_keys_sha256": nsxr_key_source_sha256,
            "data_entry_for_nsxr_keys": 163,
        },
        "summary": {
            "mate_payload_count": len(mate_rows),
            "source_entry_count": len(rows_by_entry),
            "material_count": len(result_rows),
            "matched_material_count": matched,
            "unmatched_material_count": len(result_rows) - matched,
            "unique_material_word_00_count": len(key_counts),
            "unique_unmatched_word_00_count": len(missing_key_counts),
        },
        "material_word_00_counts": [
            {"word_00": key, "count": count, "is_nsxr_context_key": key in nsxr_keys}
            for key, count in sorted(key_counts.items())
        ],
        "unmatched_word_00_counts": [
            {"word_00": key, "count": count}
            for key, count in sorted(missing_key_counts.items())
        ],
        "materials": result_rows,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--extractor", type=Path, required=True)
    parser.add_argument("--data-table", type=Path, required=True)
    parser.add_argument("--data-00", type=Path, required=True)
    parser.add_argument("--data-01", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--nsxr-keys", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target-id", default="ac6-xbox360-pal")
    parser.add_argument(
        "--xex-sha256",
        default="acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    document = analyze(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary_output.write_text(json.dumps(document, indent=2) + "\n")
    temporary_output.replace(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
