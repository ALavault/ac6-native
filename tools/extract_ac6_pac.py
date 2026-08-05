#!/usr/bin/env python3
"""Extract selected AC6 DATA.TBL ranges without loading a complete PAC."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ac6_fhm import parse_fhm
from ac6_mode1_codec import decompress_entry, descramble


HEADER_SIZE = 8
ENTRY_SIZE = 16
MAX_FHM_DEPTH = 32
MAX_FHM_NODES = 1_000_000


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_tbl(path: Path) -> tuple[int, int, list[dict]]:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise ValueError("DATA.TBL is too small")
    entry_count, pack_count = struct.unpack_from(">II", data, 0)
    expected_size = HEADER_SIZE + entry_count * ENTRY_SIZE
    if len(data) != expected_size:
        raise ValueError(f"unexpected DATA.TBL size: got {len(data)}, expected {expected_size}")
    entries = []
    for index in range(entry_count):
        group, offset, stored_size, expanded_size = struct.unpack_from(
            ">4I", data, HEADER_SIZE + index * ENTRY_SIZE
        )
        entries.append(
            {
                "index": index,
                "group": group,
                "group_hex": f"0x{group:08x}",
                "pac_name": "DATA01.PAC" if group & 0x01000000 else "DATA00.PAC",
                "storage_kind": "raw" if group & 0x00020000 else "compressed",
                "offset": offset,
                "stored_size": stored_size,
                "expanded_size": expanded_size,
            }
        )
    return entry_count, pack_count, entries


def read_range(path: Path, offset: int, length: int) -> bytes:
    if offset < 0 or length < 0:
        raise ValueError("negative PAC range")
    if offset + length > path.stat().st_size:
        raise ValueError(
            f"range exceeds {path.name}: offset=0x{offset:x} length=0x{length:x}"
        )
    with path.open("rb") as stream:
        stream.seek(offset)
        blob = stream.read(length)
    if len(blob) != length:
        raise ValueError(f"short PAC read: got {len(blob)}, expected {length}")
    return blob


def summarize_fhm(blob: bytes) -> dict | None:
    counts: Counter[str] = Counter()
    fhm_count = 0
    leaf_count = 0
    max_depth = 0
    node_count = 0
    parse_failures = 0

    def visit(data: bytes, depth: int) -> None:
        nonlocal fhm_count, leaf_count, max_depth, node_count, parse_failures
        max_depth = max(max_depth, depth)
        if data[:4] != b"FHM ":
            leaf_count += 1
            counts[data[:4].decode("latin-1", errors="replace") if data else ""] += 1
            return
        fhm_count += 1
        counts["FHM "] += 1
        children = parse_fhm(data)
        if children is None:
            parse_failures += 1
            return
        for child in children:
            node_count += 1
            if node_count > MAX_FHM_NODES:
                raise ValueError("FHM node limit exceeded")
            counts[child.magic] += 1
            if child.magic == "FHM " and depth < MAX_FHM_DEPTH:
                visit(child.data, depth + 1)
            elif child.magic == "FHM ":
                parse_failures += 1
            else:
                leaf_count += 1

    if blob[:4] != b"FHM ":
        return None
    visit(blob, 0)
    return {
        "root": "FHM",
        "fhm_count": fhm_count,
        "leaf_count": leaf_count,
        "node_count": node_count,
        "max_depth": max_depth,
        "magic_counts": dict(sorted(counts.items())),
        "parse_failures": parse_failures,
    }


def decode_payload(
    stored: bytes,
    entry: dict,
    *,
    decode: bool,
    preserve_raw_storage: bool,
) -> tuple[bytes, dict, str]:
    """Return logical payload bytes, decode metadata and output suffix.

    AC6 applies the same index-derived XOR descrambling to both compressed and
    raw DATA.TBL records. Compressed records are additionally raw-DEFLATE
    streams. ``--preserve-raw-storage`` keeps the exact on-disc bytes for
    forensic work while the normal decode path exposes the logical raw payload.
    """
    if not decode:
        return stored, {"status": "not_attempted"}, ".stored.bin"

    if entry["storage_kind"] == "compressed":
        payload = decompress_entry(stored, entry["index"], entry["expanded_size"])
        return (
            payload,
            {"status": "decoded", "codec": "mode1_pi_xor_raw_deflate"},
            ".decompressed.bin",
        )

    if preserve_raw_storage:
        return (
            stored,
            {
                "status": "preserved",
                "codec": "stored_bytes",
                "reason": "preserve_raw_storage",
            },
            ".stored.bin",
        )

    payload = descramble(stored, entry["index"])
    expected_size = entry["expanded_size"]
    if expected_size not in (0, len(payload)):
        raise ValueError(
            f"raw decoded size mismatch for entry {entry['index']}: "
            f"got {len(payload)}, expected {expected_size}"
        )
    return (
        payload,
        {"status": "decoded", "codec": "mode1_pi_xor_raw"},
        ".descrambled.bin",
    )


def extract_selected(
    asset_root: Path,
    output_root: Path,
    indices: list[int],
    decompress: bool,
    preserve_raw_storage: bool = False,
) -> dict:
    data_tbl = asset_root / "DATA.TBL"
    entry_count, pack_count, all_entries = parse_tbl(data_tbl)
    selected = []
    for index in indices:
        if index < 0 or index >= entry_count:
            raise ValueError(f"DATA.TBL index out of range: {index}")
        selected.append(all_entries[index])
    if len({entry["index"] for entry in selected}) != len(selected):
        raise ValueError("duplicate DATA.TBL index")

    archive_paths = {name: asset_root / name for name in ("DATA00.PAC", "DATA01.PAC")}
    archive_meta = {
        name: {"size": path.stat().st_size, "sha256": sha256_path(path)}
        for name, path in archive_paths.items()
    }
    output_root.mkdir(parents=True, exist_ok=True)
    payload_root = output_root / "payloads"
    payload_root.mkdir(exist_ok=True)
    records = []
    for entry in selected:
        archive = archive_paths[entry["pac_name"]]
        stored = read_range(archive, entry["offset"], entry["stored_size"])
        record = dict(entry)
        record["source_range"] = {
            "archive": entry["pac_name"],
            "offset": entry["offset"],
            "length": entry["stored_size"],
            "end": entry["offset"] + entry["stored_size"],
        }
        record["stored"] = {
            "size": len(stored),
            "sha256": hashlib.sha256(stored).hexdigest(),
            "head_hex": stored[:32].hex(),
        }
        payload, decode_metadata, suffix = decode_payload(
            stored,
            entry,
            decode=decompress,
            preserve_raw_storage=preserve_raw_storage,
        )
        record["decode"] = decode_metadata
        record["payload"] = {
            "size": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(),
            "head_hex": payload[:32].hex(),
        }
        structure = summarize_fhm(payload)
        record["structure"] = structure or {
            "status": "unknown_magic",
            "magic_hex": payload[:4].hex(),
        }
        out_path = payload_root / f"{entry['index']:04d}{suffix}"
        out_path.write_bytes(payload)
        record["payload_path"] = str(out_path.relative_to(output_root)).replace("\\", "/")
        records.append(record)

    manifest = {
        "schema_version": 1,
        "asset_root": str(asset_root),
        "data_tbl": {
            "path": "DATA.TBL",
            "size": data_tbl.stat().st_size,
            "sha256": sha256_path(data_tbl),
            "entry_count": entry_count,
            "pack_count": pack_count,
        },
        "archives": archive_meta,
        "indices": indices,
        "entries": records,
        "policy": {
            "bounded_pac_reads": True,
            "complete_pac_not_copied": True,
            "unknown_magic_fail_closed": True,
            "raw_storage_descrambled_when_decoding": not preserve_raw_storage,
            "raw_storage_preserved": preserve_raw_storage,
        },
    }
    (output_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset_root", type=Path)
    parser.add_argument("--indices", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--decompress",
        action="store_true",
        help="decode logical payloads: descramble raw records and inflate compressed records",
    )
    parser.add_argument(
        "--preserve-raw-storage",
        action="store_true",
        help="with --decompress, keep exact on-disc bytes for raw records",
    )
    args = parser.parse_args()
    if args.preserve_raw_storage and not args.decompress:
        parser.error("--preserve-raw-storage requires --decompress")
    manifest = extract_selected(
        args.asset_root.resolve(),
        args.output.resolve(),
        args.indices,
        args.decompress,
        args.preserve_raw_storage,
    )
    print(
        json.dumps(
            {
                "indices": manifest["indices"],
                "decoded": sum(
                    entry["decode"]["status"] == "decoded"
                    for entry in manifest["entries"]
                ),
                "fhm": sum(
                    entry["structure"].get("root") == "FHM"
                    for entry in manifest["entries"]
                ),
                "output": str(args.output.resolve()),
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
