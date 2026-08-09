#!/usr/bin/env python3
"""Audit a native AC6 retail cache and emit the durable 15-mission matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import tempfile
from pathlib import Path


CURRENT = struct.Struct(">8sII32s")
HEADER = struct.Struct(">8s6I32s32s32s32sQQQQ")
RECORD = struct.Struct(">IIBBHQQQQ32s32s")
IDENTITY = {
    "xex": (7483392, "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"),
    "data_tbl": (14824, "82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5"),
    "data00": (2267086848, "c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816"),
    "data01": (664141824, "eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4"),
}
MAXIMUM_STORED_SIZE = 256 * 1024 * 1024
MAXIMUM_EXPANDED_SIZE = 512 * 1024 * 1024
MAXIMUM_TOTAL_EXPANDED_SIZE = 2 * 1024 * 1024 * 1024


class AuditError(ValueError):
    pass


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_current(cache: Path) -> str:
    try:
        raw = (cache / "current").read_bytes()
    except OSError as exc:
        raise AuditError(f"current record unreadable: {exc}") from exc
    if len(raw) != CURRENT.size:
        raise AuditError("current record size mismatch")
    magic, version, size, digest = CURRENT.unpack(raw)
    if magic != b"AC6RCUR\0" or version != 1 or size != CURRENT.size:
        raise AuditError("current record incompatible")
    return digest.hex()


def parse_index(raw: bytes) -> tuple[dict, list[dict]]:
    if len(raw) < HEADER.size:
        raise AuditError("index is truncated")
    unpacked = HEADER.unpack_from(raw)
    magic, version, header_size, record_size, count, table_count, pack_count = unpacked[:7]
    digests = unpacked[7:11]
    sizes = unpacked[11:15]
    if (
        magic != b"AC6RIDX\0"
        or version != 1
        or header_size != HEADER.size
        or record_size != RECORD.size
        or count != 15
        or table_count != 926
        or pack_count != 2
        or len(raw) != HEADER.size + count * RECORD.size
    ):
        raise AuditError("index header or extent mismatch")
    labels = ("xex", "data_tbl", "data00", "data01")
    identity = {}
    for label, digest, size in zip(labels, digests, sizes, strict=True):
        expected_size, expected_digest = IDENTITY[label]
        if size != expected_size or digest.hex() != expected_digest:
            raise AuditError(f"index {label} identity mismatch")
        identity[label] = {"size": size, "sha256": digest.hex()}

    records = []
    previous = -1
    source_ranges: set[tuple[int, int, int]] = set()
    total_expanded = 0
    for ordinal in range(count):
        row = RECORD.unpack_from(raw, HEADER.size + ordinal * RECORD.size)
        index, group, archive, codec, reserved = row[:5]
        offset, stored_size, expanded_size, payload_size = row[5:9]
        stored_sha256, payload_sha256 = row[9:11]
        archive_size = sizes[2 + archive] if archive in (0, 1) else 0
        source_range = (archive, offset, stored_size)
        expected_archive = 1 if group & 0x01000000 else 0
        expected_codec = 2 if group & 0x00020000 else 1
        if (
            reserved != 0
            or archive not in (0, 1)
            or codec not in (1, 2)
            or index >= table_count
            or index <= previous
            or stored_size <= 0
            or expanded_size <= 0
            or payload_size <= 0
            or stored_size > MAXIMUM_STORED_SIZE
            or expanded_size > MAXIMUM_EXPANDED_SIZE
            or expanded_size != payload_size
            or total_expanded + payload_size > MAXIMUM_TOTAL_EXPANDED_SIZE
            or offset + stored_size > archive_size
            or archive != expected_archive
            or codec != expected_codec
            or (codec == 2 and stored_size != expanded_size)
            or source_range in source_ranges
        ):
            raise AuditError(f"invalid index record at ordinal {ordinal}")
        previous = index
        source_ranges.add(source_range)
        total_expanded += payload_size
        records.append(
            {
                "data_table_entry_index": index,
                "group": group,
                "archive": "DATA00.PAC" if archive == 0 else "DATA01.PAC",
                "codec": (
                    "mode1_pi_xor_raw_deflate" if codec == 1 else "mode1_pi_xor_raw"
                ),
                "source_offset": offset,
                "stored_size": stored_size,
                "expanded_size": expanded_size,
                "payload_size": payload_size,
                "stored_sha256": stored_sha256.hex(),
                "payload_sha256": payload_sha256.hex(),
            }
        )
    if [record["data_table_entry_index"] for record in records] != list(range(9, 24)):
        raise AuditError("index does not cover exactly DATA.TBL entries 9..23")
    return identity, records


def audit_blobs(cache: Path, records: list[dict]) -> None:
    for record in records:
        digest = record["payload_sha256"]
        path = cache / "blobs" / "sha256" / digest[:2] / digest
        try:
            size = path.stat().st_size
        except OSError as exc:
            raise AuditError(
                f"payload blob missing for entry {record['data_table_entry_index']}: {exc}"
            ) from exc
        if size != record["payload_size"] or sha256_path(path) != digest:
            raise AuditError(
                f"payload blob mismatch for entry {record['data_table_entry_index']}"
            )


def cross_check_catalog(catalog: dict, records: list[dict]) -> dict[int, dict]:
    missions = catalog.get("missions")
    if not isinstance(missions, list) or len(missions) != 15:
        raise AuditError("campaign catalog does not contain 15 missions")
    by_entry = {mission.get("data_table_entry_index"): mission for mission in missions}
    if sorted(by_entry) != list(range(9, 24)):
        raise AuditError("campaign catalog routes do not cover entries 9..23")
    assets = catalog.get("data_table_assets", {})
    for record in records:
        index = record["data_table_entry_index"]
        mission = by_entry[index]
        parse = mission.get("parse", {})
        asset = assets.get(str(index), {})
        expected = {
            "archive": asset.get("archive"),
            "source_offset": int(asset.get("offset", "-1"), 16),
            "stored_size": parse.get("stored_size"),
            "expanded_size": parse.get("expanded_size"),
            "payload_size": parse.get("expanded_size"),
            "stored_sha256": parse.get("stored_sha256"),
            "payload_sha256": parse.get("payload_sha256"),
            "codec": parse.get("codec"),
        }
        for field, value in expected.items():
            if record[field] != value:
                raise AuditError(f"catalog mismatch for entry {index}: {field}")
    return by_entry


def load_inventory(path: Path) -> tuple[dict[int, dict], list[dict]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    missions = document.get("missions")
    contracts = document.get("contracts")
    if not isinstance(missions, list) or len(missions) != 15 or not isinstance(contracts, list):
        raise AuditError("payload dependency inventory shape mismatch")
    by_entry = {mission.get("data_table_entry_index"): mission for mission in missions}
    if sorted(by_entry) != list(range(9, 24)):
        raise AuditError("payload dependency inventory scope mismatch")
    return by_entry, contracts


def build_matrix(
    index_sha256: str,
    identity: dict,
    records: list[dict],
    catalog: dict,
    catalog_path: Path,
    inventory_path: Path,
) -> dict:
    by_catalog = cross_check_catalog(catalog, records)
    by_inventory, contracts = load_inventory(inventory_path)
    unresolved = [
        contract["name"]
        for contract in contracts
        if contract.get("status") != "observed_bounded"
    ]
    missions = []
    for record in records:
        entry = record["data_table_entry_index"]
        catalog_mission = by_catalog[entry]
        inventory = by_inventory[entry]
        type_counts = inventory.get("type_counts", {})
        formats = {
            name: count
            for name, count in sorted(type_counts.items())
            if not name.startswith("unknown:")
        }
        hashes_by_type = inventory.get("resource_hashes_by_type", {})
        resource_hashes = {
            name: len(hashes)
            for name, hashes in sorted(hashes_by_type.items())
            if isinstance(hashes, list)
        }
        missions.append(
            {
                "mission_id": catalog_mission["mission_id"],
                "playable_supported": catalog_mission["mission_id"] == 1,
                "catalog_status": catalog_mission["status"],
                "campaign_selector": catalog_mission["campaign_selector"],
                "dpl_resource_id": catalog_mission["dpl_resource_id"],
                **record,
                "recursive_nodes": inventory["node_count"],
                "maximum_fhm_depth": inventory["max_depth"],
                "formats_encountered": formats,
                "content_addressed_resources_by_format": resource_hashes,
                "catalog_qualification_gaps": sorted(
                    catalog_mission.get("qualification_gaps", [])
                ),
                "remaining_boundaries": sorted(
                    (
                        set(catalog_mission.get("qualification_gaps", []))
                        - {"payload_dependency_inventory"}
                    )
                    | set(unresolved)
                ),
            }
        )
    return {
        "schema_version": 1,
        "status": "campaign_payloads_imported",
        "cache_index_sha256": index_sha256,
        "source_identity": identity,
        "sources": {
            "campaign_catalog": {
                "path": str(catalog_path).replace("\\", "/"),
                "sha256": sha256_path(catalog_path),
                "size": catalog_path.stat().st_size,
            },
            "payload_dependency_inventory": {
                "path": str(inventory_path).replace("\\", "/"),
                "sha256": sha256_path(inventory_path),
                "size": inventory_path.stat().st_size,
            },
        },
        "summary": {
            "mission_payloads": len(missions),
            "playable_missions_claimed": 1,
            "content_blobs": len(records),
            "payload_bytes": sum(record["payload_size"] for record in records),
            "stored_bytes_read": sum(record["stored_size"] for record in records),
            "unresolved_common_contracts": unresolved,
        },
        "missions": missions,
        "policy": {
            "retail_bytes_embedded": False,
            "cache_paths_embedded": False,
            "second_mission_playability_claimed": False,
            "unknown_bindings_fail_closed": True,
        },
    }


def atomic_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(document, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def audit(cache: Path, catalog_path: Path, inventory_path: Path) -> dict:
    index_sha256 = read_current(cache)
    index_path = cache / "indices" / f"{index_sha256}.ac6idx"
    try:
        raw = index_path.read_bytes()
    except OSError as exc:
        raise AuditError(f"index unreadable: {exc}") from exc
    if hashlib.sha256(raw).hexdigest() != index_sha256:
        raise AuditError("index SHA-256 mismatch")
    identity, records = parse_index(raw)
    audit_blobs(cache, records)
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    return build_matrix(
        index_sha256, identity, records, catalog, catalog_path, inventory_path
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cache", type=Path)
    parser.add_argument(
        "--catalog", type=Path, default=Path("reports/ac6-pal-campaign-catalog.json")
    )
    parser.add_argument(
        "--inventory",
        type=Path,
        default=Path("reports/ac6-pal-payload-dependency-inventory.json"),
    )
    parser.add_argument("--matrix-out", type=Path)
    args = parser.parse_args()
    try:
        matrix = audit(args.cache, args.catalog, args.inventory)
    except (AuditError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"retail_cache=fail reason={str(exc).replace(' ', '_')}")
        return 1
    if args.matrix_out is not None:
        atomic_json(args.matrix_out, matrix)
    summary = matrix["summary"]
    print(
        "retail_cache=pass "
        f"missions={summary['mission_payloads']} blobs={summary['content_blobs']} "
        f"bytes={summary['payload_bytes']} index={matrix['cache_index_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
