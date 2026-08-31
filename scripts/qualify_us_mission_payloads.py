#!/usr/bin/env python3
"""Qualify bounded US campaign payload slices without copying retail PACs.

The campaign chain proves that selectors 1..15 address DATA.TBL entries 9..23.
This tool reads only those DATA.TBL ranges from the identity-qualified US ISO,
decodes the mode-1 payloads, and records structural FHM facts.  It deliberately
does not publish gameplay/objective semantics or retain a PAC/container copy.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import struct
import sys
from pathlib import Path
from typing import Any


WORKSPACE = Path(__file__).resolve().parents[1]
PRODUCT = WORKSPACE / "recompilation" / "ace-combat-6-retail"
TOOLS = WORKSPACE / "tools"
TARGET = PRODUCT / "targets" / "ntsc-uj.json"
DATA_TBL_DEFAULT = WORKSPACE / "artifacts" / "retail-us-data-tbl-static-20260827" / "DATA.TBL"
MISSION_FIRST = 1
MISSION_LAST = 15
PAC_NAMES = ("DATA00.PAC", "DATA01.PAC")
SECTOR_SIZE = 2048
MAX_STORED_SLICE = 64 * 1024 * 1024
MAX_EXPANDED_PAYLOAD = 128 * 1024 * 1024


class QualificationError(RuntimeError):
    pass


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise QualificationError(f"cannot load helper: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


FHM = load_module("ac6_qualify_fhm", TOOLS / "ac6_fhm.py")
CODEC = load_module("ac6_qualify_mode1", TOOLS / "ac6_mode1_codec.py")
XDVDFS = load_module(
    "ac6_qualify_xdvdfs",
    PRODUCT / "tools" / "extract_xdvdfs_file.py",
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise QualificationError(f"JSON object required: {path}")
    return value


def parse_data_tbl(path: Path) -> tuple[int, int, list[dict[str, int | str]]]:
    raw = path.read_bytes()
    if len(raw) < 8:
        raise QualificationError("DATA.TBL is too small")
    entry_count, pack_count = struct.unpack_from(">II", raw, 0)
    expected = 8 + entry_count * 16
    if len(raw) != expected:
        raise QualificationError(
            f"DATA.TBL size mismatch: got {len(raw)}, expected {expected}"
        )
    archive_indices = {name: 0 for name in PAC_NAMES}
    entries: list[dict[str, int | str]] = []
    for index in range(entry_count):
        group, offset, stored_size, expanded_size = struct.unpack_from(
            ">4I", raw, 8 + index * 16
        )
        archive = "DATA01.PAC" if group & 0x01000000 else "DATA00.PAC"
        codec_index = archive_indices[archive]
        archive_indices[archive] += 1
        entries.append({
            "index": index,
            "group": group,
            "offset": offset,
            "stored_size": stored_size,
            "expanded_size": expanded_size,
            "codec_index": codec_index,
            "archive": archive,
            "storage_kind": "raw" if group & 0x00020000 else "compressed",
        })
    return entry_count, pack_count, entries


def read_iso_range(stream, offset: int, length: int, image_size: int) -> bytes:
    if offset < 0 or length < 0 or offset > image_size or length > image_size - offset:
        raise QualificationError("bounded ISO range exceeds image")
    stream.seek(offset)
    payload = stream.read(length)
    if len(payload) != length:
        raise QualificationError("short bounded ISO read")
    return payload


def structural_summary(blob: bytes) -> dict[str, Any]:
    if blob[:4] != b"FHM ":
        return {"root": "unknown", "magic_hex": blob[:4].hex()}
    counts: dict[str, int] = {}
    fhm_count = 0
    leaf_count = 0
    node_count = 0
    max_depth = 0
    parse_failures = 0

    def add(magic: str) -> None:
        counts[magic] = counts.get(magic, 0) + 1

    def visit(data: bytes, depth: int) -> None:
        nonlocal fhm_count, leaf_count, node_count, max_depth, parse_failures
        max_depth = max(max_depth, depth)
        if data[:4] != b"FHM ":
            leaf_count += 1
            add(data[:4].decode("latin-1", errors="replace") if data else "")
            return
        fhm_count += 1
        add("FHM ")
        children = FHM.parse_fhm(data)
        if children is None:
            parse_failures += 1
            return
        for child in children:
            node_count += 1
            add(child.magic)
            if child.magic == "FHM ":
                visit(child.data, depth + 1)
            else:
                leaf_count += 1

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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument("--data-tbl", type=Path, default=DATA_TBL_DEFAULT)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    target = load_json(TARGET)
    iso = args.iso.resolve()
    data_tbl = args.data_tbl.resolve()
    output = args.output.resolve()
    if output.exists():
        raise QualificationError(f"refusing to overwrite: {output}")
    expected_iso = target.get("iso", {})
    if not iso.is_file() or iso.stat().st_size != expected_iso.get("size"):
        raise QualificationError("US ISO size does not match target definition")
    iso_digest = sha256_file(iso)
    if iso_digest != expected_iso.get("sha256"):
        raise QualificationError(f"US ISO SHA-256 mismatch: {iso_digest}")
    if not data_tbl.is_file():
        raise QualificationError(f"DATA.TBL is missing: {data_tbl}")

    entry_count, pack_count, entries = parse_data_tbl(data_tbl)
    tbl_digest = sha256_file(data_tbl)
    selected = [entries[index + 8] for index in range(MISSION_FIRST, MISSION_LAST + 1)]
    if len(selected) != 15 or [int(item["index"]) for item in selected] != list(range(9, 24)):
        raise QualificationError("mission DATA.TBL selection is not 9..23")
    if any(item["storage_kind"] != "compressed" for item in selected):
        raise QualificationError("mission DATA.TBL entry is not mode-1 compressed")

    image_size = iso.stat().st_size
    with iso.open("rb") as stream:
        volumes: dict[str, tuple[int, int]] = {}
        for name in PAC_NAMES:
            volume, entry = XDVDFS.resolve_entry(stream, image_size, name)
            volumes[name] = (volume.game_offset + entry.sector * SECTOR_SIZE, entry.length)

        records: list[dict[str, Any]] = []
        for mission_id in range(MISSION_FIRST, MISSION_LAST + 1):
            item = selected[mission_id - 1]
            archive = str(item["archive"])
            pac_base, pac_size = volumes[archive]
            offset = int(item["offset"])
            stored_size = int(item["stored_size"])
            expanded_size = int(item["expanded_size"])
            if stored_size > MAX_STORED_SLICE or expanded_size > MAX_EXPANDED_PAYLOAD:
                raise QualificationError(f"bounded mission slice too large: {mission_id}")
            if offset + stored_size > pac_size:
                raise QualificationError(f"mission slice exceeds {archive}: {mission_id}")
            stored = read_iso_range(stream, pac_base + offset, stored_size, image_size)
            payload = CODEC.decompress_entry(stored, int(item["codec_index"]), expanded_size)
            records.append({
                "mission_id": mission_id,
                "campaign_selector": mission_id,
                "dpl_resource_id": mission_id + 8,
                "data_table_entry_index": int(item["index"]),
                "archive": archive,
                "archive_size": pac_size,
                "archive_iso_offset": pac_base,
                "archive_relative_offset": offset,
                "stored_size": stored_size,
                "stored_sha256": sha256_bytes(stored),
                "expanded_size": expanded_size,
                "expanded_sha256": sha256_bytes(payload),
                "codec": "mode1_pi_xor_raw_deflate",
                "codec_index": int(item["codec_index"]),
                "structure": structural_summary(payload),
            })

    manifest = {
        "schema": "ac6.retail-us-mission-payload-static.v1",
        "status": "qualified-structural-only",
        "target": "ntsc-uj",
        "xex_sha256": target["xex"]["sha256"],
        "source_iso": {
            "path": str(iso),
            "size": image_size,
            "sha256": iso_digest,
        },
        "data_tbl": {
            "path": str(data_tbl),
            "size": data_tbl.stat().st_size,
            "sha256": tbl_digest,
            "entry_count": entry_count,
            "pack_count": pack_count,
        },
        "mission_range": [MISSION_FIRST, MISSION_LAST],
        "records": records,
        "policy": {
            "bounded_iso_reads": True,
            "complete_pac_copied": False,
            "payloads_retained": False,
            "objective_semantics_inferred": False,
            "renderer_or_runtime_claimed": False,
        },
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": manifest["status"],
        "missions": len(records),
        "data_tbl_sha256": tbl_digest,
        "expanded_bytes": sum(record["expanded_size"] for record in records),
        "output": str(output),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, QualificationError, json.JSONDecodeError) as error:
        print(f"qualify_us_mission_payloads=fail error={error}", file=sys.stderr)
        raise SystemExit(2)
