#!/usr/bin/env python3
"""Audit the bounded Scene/TCAM corpus in PAL campaign payloads 9..23."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import re
import struct
import tempfile
from dataclasses import dataclass
from pathlib import Path

from ac6_fhm import FhmChild, parse_fhm
from audit_ac6_retail_content_cache import (
    CURRENT,
    RECORD,
    V2_HEADER,
    AuditError as CacheAuditError,
    parse_index,
)


SCHEMA = "ac6.scene-tcam-corpus.v1"
MISSION_ENTRIES = tuple(range(9, 24))
SCENE_RECORD_SIZE = 0x80
MOP_RECORD_STRIDE = 0x30
MAXIMUM_FHM_CHILDREN = 4096
MAXIMUM_FHM_DEPTH = 64
MAXIMUM_FHM_CONTAINERS = 100_000

# scene tables, Scene paths, Tcam resources, top-level FHM branches
EXPECTED = {
    1: (44, 553, 22, (22, 23)),
    2: (0, 0, 0, ()),
    3: (0, 0, 0, ()),
    4: (0, 0, 0, ()),
    5: (0, 0, 0, ()),
    6: (0, 0, 0, ()),
    7: (24, 290, 12, (22, 23)),
    8: (0, 0, 0, ()),
    9: (16, 662, 8, (22, 23)),
    10: (0, 0, 0, ()),
    11: (0, 0, 0, ()),
    12: (0, 0, 0, ()),
    13: (30, 363, 15, (22, 23)),
    14: (0, 0, 0, ()),
    15: (62, 1082, 31, (22, 23, 24, 25)),
}
EXPECTED_TOTALS = (176, 2950, 88)
SHA256_TEXT = re.compile(r"[0-9a-f]{64}")


class CorpusError(ValueError):
    """A fail-closed corpus or cache validation error."""


@dataclass(frozen=True)
class StrictFhm:
    declared_count: int
    live_children: tuple[FhmChild, ...]


@dataclass(frozen=True)
class TcamResource:
    mission_id: int
    path: str
    size: int
    sha256: str


@dataclass(frozen=True)
class MissionScan:
    scene_tables: int
    scene_paths: int
    tcam_resources: tuple[TcamResource, ...]
    branches: tuple[int, ...]


def _be16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise CorpusError("bounded big-endian u16 read failed")
    return struct.unpack_from(">H", data, offset)[0]


def _be32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise CorpusError("bounded big-endian u32 read failed")
    return struct.unpack_from(">I", data, offset)[0]


def parse_strict_fhm(data: bytes, location: str = "root") -> StrictFhm:
    """Apply the qualified FHM parser plus immutable fail-closed bounds."""
    if len(data) < 0x18 or data[:4] != b"FHM " or data[4] != 1 or data[5] != 1 or _be16(data, 6) != 0x10:
        raise CorpusError(f"invalid FHM header at {location}")
    count = _be32(data, 0x10)
    if count == 0 or count > MAXIMUM_FHM_CHILDREN:
        raise CorpusError(f"invalid FHM child count at {location}")
    tables_end = 0x14 + count * 16
    if tables_end > len(data):
        raise CorpusError(f"truncated FHM parallel tables at {location}")

    offsets = struct.unpack_from(f">{count}I", data, 0x14)
    sizes = struct.unpack_from(f">{count}I", data, 0x14 + count * 4)
    last_live = max((index for index, size in enumerate(sizes) if size), default=-1)
    previous_end = tables_end
    live_indices: list[int] = []
    for index, (offset, size) in enumerate(zip(offsets, sizes, strict=True)):
        if size == 0:
            # Retail FHM tables reserve sparse slots both inside and after the
            # live set. They are metadata-only when their declared size is
            # zero; never dereference their otherwise opaque offset word.
            continue
        if offset < tables_end or offset < previous_end or size > len(data) - offset:
            raise CorpusError(f"invalid FHM child extent at {location}/{index}")
        previous_end = offset + size
        live_indices.append(index)

    parsed = parse_fhm(data)
    if parsed is None:
        raise CorpusError(f"qualified FHM parser rejected {location}")
    by_index = {child.index: child for child in parsed if child.declared_size != 0}
    if sorted(by_index) != live_indices:
        raise CorpusError(f"FHM parser disagreement at {location}")
    live: list[FhmChild] = []
    for index in live_indices:
        child = by_index[index]
        unexpected_notes = [
            note
            for note in child.notes
            if not (note == "next child offset is not monotonic" and index + 1 < count and sizes[index + 1] == 0)
        ]
        if unexpected_notes or child.size != child.declared_size:
            raise CorpusError(f"non-strict FHM child at {location}/{index}")
        if child.offset != offsets[index] or child.declared_size != sizes[index]:
            raise CorpusError(f"FHM metadata disagreement at {location}/{index}")
        live.append(child)
    return StrictFhm(count, tuple(live))


def parse_scene_paths(data: bytes, location: str = "Scen") -> tuple[str, ...]:
    if not data or len(data) % SCENE_RECORD_SIZE != 0:
        raise CorpusError(f"invalid Scen table extent at {location}")
    paths: list[str] = []
    for index in range(len(data) // SCENE_RECORD_SIZE):
        record = data[index * SCENE_RECORD_SIZE : (index + 1) * SCENE_RECORD_SIZE]
        terminator = record.find(b"\0")
        if terminator < len(b"Scene/") or any(record[terminator + 1 :]):
            raise CorpusError(f"invalid Scen string record at {location}/{index}")
        raw_path = record[:terminator]
        try:
            path = raw_path.decode("ascii")
        except UnicodeDecodeError as exc:
            raise CorpusError(f"non-ASCII Scen string at {location}/{index}") from exc
        components = path.split("/")
        if (
            not path.startswith("Scene/")
            or any(byte < 0x20 or byte > 0x7E for byte in raw_path)
            or "\\" in path
            or any(component in {"", ".", ".."} for component in components)
        ):
            raise CorpusError(f"invalid Scen path at {location}/{index}")
        paths.append(path)
    return tuple(paths)


def validate_nfic_cut(data: bytes, location: str) -> None:
    if len(data) < 16 or data[:8] != b"NFICCUT\0":
        raise CorpusError(f"invalid NFICCUT sibling at {location}")


def validate_tcam_mop(data: bytes, location: str) -> None:
    """Validate the bounded three-record GYZ table used by Tcam MOPs."""
    if len(data) < 0x34:
        raise CorpusError(f"truncated Tcam MOP at {location}")
    declared_size = _be32(data, 0x20)
    gyz_offset = _be32(data, 0x30)
    if gyz_offset < 0x34 or gyz_offset > len(data):
        raise CorpusError(f"invalid Tcam GYZ offset at {location}")
    if declared_size != len(data) - gyz_offset or declared_size < 0x50:
        raise CorpusError(f"invalid Tcam outer size at {location}")
    if data[gyz_offset : gyz_offset + 4] != b"GYZ\0":
        raise CorpusError(f"invalid Tcam GYZ magic at {location}")
    if _be32(data, gyz_offset + 8) != declared_size:
        raise CorpusError(f"invalid Tcam inner size at {location}")
    if _be32(data, gyz_offset + 0x0C) != 0x20:
        raise CorpusError(f"invalid Tcam GYZ header size at {location}")
    if _be32(data, gyz_offset + 0x20) != 0x00011E00:
        raise CorpusError(f"invalid Tcam GYZ content tag at {location}")
    record_count = _be32(data, gyz_offset + 0x2C)
    record_table = _be32(data, gyz_offset + 0x30)
    if record_count != 3 or record_table < 0x50:
        raise CorpusError(f"invalid Tcam GYZ record table at {location}")
    record_bytes = record_count * MOP_RECORD_STRIDE
    if record_table > declared_size or record_bytes > declared_size - record_table:
        raise CorpusError(f"out-of-bounds Tcam GYZ record table at {location}")
    data_floor = record_table + record_bytes
    for index in range(record_count):
        record = gyz_offset + record_table + index * MOP_RECORD_STRIDE
        for field in (0x14, 0x18):
            data_offset = _be32(data, record + field)
            if data_offset < data_floor or data_offset >= declared_size:
                raise CorpusError(f"out-of-bounds Tcam data offset at {location}/{index}")


def _is_tcam_path(path: str) -> bool:
    return fnmatch.fnmatchcase(path.rsplit("/", 1)[-1], "Tcam*.mop")


def scan_mission_payload(data: bytes, mission_id: int) -> MissionScan:
    branches: set[int] = set()
    tcams: list[TcamResource] = []
    scene_tables = 0
    scene_paths = 0
    container_count = 0

    def parse_at(blob: bytes, path: tuple[int, ...]) -> StrictFhm:
        nonlocal container_count
        container_count += 1
        if container_count > MAXIMUM_FHM_CONTAINERS:
            raise CorpusError("FHM container limit exceeded")
        location = ".".join(map(str, path)) if path else "root"
        return parse_strict_fhm(blob, location)

    def walk(
        blob: bytes,
        path: tuple[int, ...],
        preparsed: StrictFhm | None = None,
    ) -> None:
        nonlocal scene_paths, scene_tables
        if len(path) > MAXIMUM_FHM_DEPTH:
            raise CorpusError("FHM depth limit exceeded")
        container = preparsed if preparsed is not None else parse_at(blob, path)
        by_index = {child.index: child for child in container.live_children}
        parsed_children: dict[int, StrictFhm] = {}
        for scene_child in container.live_children:
            if scene_child.data[:4] != b"Scen":
                continue
            scene_path = path + (scene_child.index,)
            location = ".".join(map(str, scene_path))
            if not scene_path:
                raise CorpusError("root Scen table has no campaign branch")
            state = by_index.get(scene_child.index - 2)
            resources_child = by_index.get(scene_child.index - 1)
            if state is None or resources_child is None:
                raise CorpusError(f"incomplete Scene triplet at {location}")
            validate_nfic_cut(state.data, f"{location}:state")
            if resources_child.data[:4] != b"FHM ":
                raise CorpusError(f"invalid Scene resource sibling at {location}")
            resource_path = path + (resources_child.index,)
            resources = parse_at(resources_child.data, resource_path)
            parsed_children[resources_child.index] = resources
            paths = parse_scene_paths(scene_child.data, location)
            if (
                resources.declared_count != len(paths)
                or len(resources.live_children) != len(paths)
                or [child.index for child in resources.live_children] != list(range(len(paths)))
            ):
                raise CorpusError(f"Scene/resource cardinality mismatch at {location}")
            scene_tables += 1
            scene_paths += len(paths)
            branches.add(scene_path[0])
            for index, retail_path in enumerate(paths):
                if not _is_tcam_path(retail_path):
                    continue
                resource = resources.live_children[index]
                validate_tcam_mop(resource.data, f"{location}:resource:{index}")
                tcams.append(
                    TcamResource(
                        mission_id=mission_id,
                        path=retail_path,
                        size=len(resource.data),
                        sha256=hashlib.sha256(resource.data).hexdigest(),
                    )
                )
        for child in container.live_children:
            if child.data[:4] == b"FHM ":
                walk(
                    child.data,
                    path + (child.index,),
                    parsed_children.get(child.index),
                )

    walk(data, ())
    return MissionScan(
        scene_tables=scene_tables,
        scene_paths=scene_paths,
        tcam_resources=tuple(sorted(tcams, key=lambda resource: (resource.path, resource.sha256))),
        branches=tuple(sorted(branches)),
    )


def validate_expected(mission_id: int, scan: MissionScan) -> None:
    try:
        expected_tables, expected_paths, expected_tcams, expected_branches = EXPECTED[mission_id]
    except KeyError as exc:
        raise CorpusError(f"mission id outside 1..15: {mission_id}") from exc
    observed = (
        scan.scene_tables,
        scan.scene_paths,
        len(scan.tcam_resources),
        scan.branches,
    )
    expected = (
        expected_tables,
        expected_paths,
        expected_tcams,
        expected_branches,
    )
    if observed != expected:
        raise CorpusError(f"Mission {mission_id:02d} census mismatch: observed={observed} expected={expected}")


def validate_matrix_document(document: object) -> dict:
    """Validate the closed metadata-only form committed as corpus evidence."""
    if not isinstance(document, dict) or set(document) != {
        "cache_index_sha256",
        "missions",
        "policy",
        "schema",
        "schema_version",
        "status",
        "tcam_resources",
        "totals",
    }:
        raise CorpusError("Scene/TCAM matrix top-level shape mismatch")
    if (
        document["schema"] != SCHEMA
        or type(document["schema_version"]) is not int
        or document["schema_version"] != 1
        or document["status"] != "qualified"
        or document["policy"] != {"cache_paths_embedded": False, "retail_bytes_embedded": False}
        or not isinstance(document["cache_index_sha256"], str)
        or SHA256_TEXT.fullmatch(document["cache_index_sha256"]) is None
    ):
        raise CorpusError("Scene/TCAM matrix identity or policy mismatch")
    expected_totals = {
        "mission_payloads": 15,
        "scene_paths": EXPECTED_TOTALS[1],
        "scene_tables": EXPECTED_TOTALS[0],
        "tcam_resources": EXPECTED_TOTALS[2],
    }
    if document["totals"] != expected_totals:
        raise CorpusError("Scene/TCAM matrix totals mismatch")

    missions = document["missions"]
    if not isinstance(missions, list) or len(missions) != 15:
        raise CorpusError("Scene/TCAM matrix mission count mismatch")
    for mission_id, mission in enumerate(missions, start=1):
        expected_tables, expected_paths, expected_tcams, expected_branches = EXPECTED[mission_id]
        if not isinstance(mission, dict) or set(mission) != {
            "branches",
            "mission_id",
            "payload_entry",
            "payload_sha256",
            "payload_size",
            "scene_paths",
            "scene_tables",
            "tcam_resources",
        }:
            raise CorpusError("Scene/TCAM matrix mission shape mismatch")
        if (
            any(
                type(mission[key]) is not int
                for key in (
                    "mission_id",
                    "payload_entry",
                    "payload_size",
                    "scene_paths",
                    "scene_tables",
                    "tcam_resources",
                )
            )
            or mission["mission_id"] != mission_id
            or mission["payload_entry"] != mission_id + 8
            or mission["scene_tables"] != expected_tables
            or mission["scene_paths"] != expected_paths
            or mission["tcam_resources"] != expected_tcams
            or mission["branches"] != list(expected_branches)
            or mission["payload_size"] <= 0
            or not isinstance(mission["payload_sha256"], str)
            or SHA256_TEXT.fullmatch(mission["payload_sha256"]) is None
        ):
            raise CorpusError(f"Scene/TCAM matrix Mission {mission_id:02d} mismatch")

    resources = document["tcam_resources"]
    if not isinstance(resources, list) or len(resources) != EXPECTED_TOTALS[2]:
        raise CorpusError("Scene/TCAM matrix TCAM count mismatch")
    previous: tuple[int, str, str] | None = None
    counts = {mission_id: 0 for mission_id in EXPECTED}
    paths: set[tuple[int, str]] = set()
    for resource in resources:
        if not isinstance(resource, dict) or set(resource) != {
            "mission_id",
            "path",
            "sha256",
            "size",
        }:
            raise CorpusError("Scene/TCAM matrix resource shape mismatch")
        mission_id = resource["mission_id"]
        path = resource["path"]
        digest = resource["sha256"]
        key = (mission_id, path, digest)
        if (
            type(mission_id) is not int
            or mission_id not in EXPECTED
            or not isinstance(path, str)
            or not path.startswith("Scene/")
            or not _is_tcam_path(path)
            or "\\" in path
            or any(component in {"", ".", ".."} for component in path.split("/"))
            or (mission_id, path) in paths
            or not isinstance(digest, str)
            or SHA256_TEXT.fullmatch(digest) is None
            or type(resource["size"]) is not int
            or resource["size"] <= 0
            or (previous is not None and key < previous)
        ):
            raise CorpusError("Scene/TCAM matrix resource metadata mismatch")
        paths.add((mission_id, path))
        counts[mission_id] += 1
        previous = key
    if any(counts[mission_id] != EXPECTED[mission_id][2] for mission_id in EXPECTED):
        raise CorpusError("Scene/TCAM matrix per-mission TCAM count mismatch")
    return document


def _read_v2_index(cache: Path) -> tuple[str, list[dict]]:
    try:
        current = (cache / "current").read_bytes()
    except OSError as exc:
        raise CorpusError("cache current record is unreadable") from exc
    if len(current) != CURRENT.size:
        raise CorpusError("cache current record size mismatch")
    magic, version, size, index_digest = CURRENT.unpack(current)
    if magic != b"AC6RCUR\0" or version != 2 or size != CURRENT.size:
        raise CorpusError("RetailContentStore v2 current record required")
    index_sha256 = index_digest.hex()
    index_path = cache / "indices" / f"{index_sha256}.ac6idx"
    try:
        raw = index_path.read_bytes()
    except OSError as exc:
        raise CorpusError("cache v2 index is unreadable") from exc
    expected_size = V2_HEADER.size + 926 * RECORD.size
    if len(raw) != expected_size or hashlib.sha256(raw).digest() != index_digest:
        raise CorpusError("cache v2 index size or SHA-256 mismatch")
    if raw[:8] != b"AC6RIDX\0" or _be32(raw, 8) != 2:
        raise CorpusError("cache index is not RetailContentStore v2")
    try:
        _identity, records = parse_index(raw)
    except CacheAuditError as exc:
        raise CorpusError(f"cache v2 index rejected: {exc}") from exc
    if [record["data_table_entry_index"] for record in records] != list(range(926)):
        raise CorpusError("cache v2 index closure is incomplete")
    return index_sha256, records


def _read_payload(cache: Path, record: dict) -> bytes:
    digest = record["payload_sha256"]
    path = cache / "blobs" / "sha256" / digest[:2] / digest
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise CorpusError(f"payload blob is unreadable for entry {record['data_table_entry_index']}") from exc
    if len(data) != record["payload_size"] or hashlib.sha256(data).hexdigest() != digest:
        raise CorpusError(f"payload blob size or SHA-256 mismatch for entry {record['data_table_entry_index']}")
    return data


def audit(cache: Path) -> dict:
    index_sha256, records = _read_v2_index(cache)
    by_entry = {record["data_table_entry_index"]: record for record in records}
    if sorted(entry for entry in by_entry if 9 <= entry <= 23) != list(MISSION_ENTRIES):
        raise CorpusError("campaign payload entries 9..23 are not exact")

    missions: list[dict] = []
    all_tcams: list[TcamResource] = []
    total_tables = 0
    total_paths = 0
    for mission_id, entry in enumerate(MISSION_ENTRIES, start=1):
        record = by_entry[entry]
        payload = _read_payload(cache, record)
        scan = scan_mission_payload(payload, mission_id)
        validate_expected(mission_id, scan)
        total_tables += scan.scene_tables
        total_paths += scan.scene_paths
        all_tcams.extend(scan.tcam_resources)
        missions.append(
            {
                "branches": list(scan.branches),
                "mission_id": mission_id,
                "payload_entry": entry,
                "payload_sha256": record["payload_sha256"],
                "payload_size": record["payload_size"],
                "scene_paths": scan.scene_paths,
                "scene_tables": scan.scene_tables,
                "tcam_resources": len(scan.tcam_resources),
            }
        )
        del payload

    totals = (total_tables, total_paths, len(all_tcams))
    if totals != EXPECTED_TOTALS:
        raise CorpusError(f"campaign Scene/TCAM totals mismatch: observed={totals} expected={EXPECTED_TOTALS}")
    return validate_matrix_document(
        {
            "cache_index_sha256": index_sha256,
            "missions": missions,
            "policy": {
                "cache_paths_embedded": False,
                "retail_bytes_embedded": False,
            },
            "schema": SCHEMA,
            "schema_version": 1,
            "status": "qualified",
            "tcam_resources": [
                {
                    "mission_id": resource.mission_id,
                    "path": resource.path,
                    "sha256": resource.sha256,
                    "size": resource.size,
                }
                for resource in sorted(
                    all_tcams,
                    key=lambda item: (item.mission_id, item.path, item.sha256),
                )
            ],
            "totals": {
                "mission_payloads": len(missions),
                "scene_paths": total_paths,
                "scene_tables": total_tables,
                "tcam_resources": len(all_tcams),
            },
        }
    )


def atomic_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            json.dump(document, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--cache", type=Path)
    mode.add_argument("--check-matrix", type=Path)
    parser.add_argument("--matrix-out", type=Path)
    args = parser.parse_args()
    try:
        if args.check_matrix is not None:
            if args.matrix_out is not None:
                parser.error("--matrix-out is only valid with --cache")
            matrix = validate_matrix_document(json.loads(args.check_matrix.read_text(encoding="utf-8")))
            totals = matrix["totals"]
            print(
                "scene_tcam_matrix=pass "
                f"missions={totals['mission_payloads']} "
                f"tables={totals['scene_tables']} paths={totals['scene_paths']} "
                f"tcam={totals['tcam_resources']} metadata_only=1"
            )
            return 0
        if args.matrix_out is None:
            parser.error("--matrix-out is required with --cache")
        matrix = audit(args.cache)
        atomic_json(args.matrix_out, matrix)
    except (CorpusError, OSError, ValueError) as exc:
        reason = str(exc).replace(" ", "_").replace("\n", "_")
        print(f"scene_tcam_corpus=fail reason={reason}")
        return 1
    totals = matrix["totals"]
    print(
        "scene_tcam_corpus=pass "
        f"missions={totals['mission_payloads']} "
        f"tables={totals['scene_tables']} paths={totals['scene_paths']} "
        f"tcam={totals['tcam_resources']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
