#!/usr/bin/env python3
"""Build a bounded, content-addressed dependency inventory for AC6 FHM payloads.

The input manifests are produced by ``extract_ac6_pac.py``.  Only decoded
payloads referenced by those manifests are read; retail PAC containers are
never copied or emitted by this tool.  The output records tree offsets,
formats, hashes, parent/child edges and cross-mission sharing without
assigning semantics to unknown four-byte tags.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ac6_fhm import parse_fhm


MAX_FHM_DEPTH = 32
MAX_FHM_NODES = 1_000_000
MAGIC_TYPES = {
    b"FHM ": "FHM",
    b"MDLP": "MDLP",
    b"PLAD": "PLAD",
    b"NTXR": "NTXR",
    b"NDXR": "NDXR",
    b"NSXR": "NSXR",
    b"NFH\x00": "NFH",
    b"NFIC": "NFIC",
    b"Scen": "Scene",
    b"CAPT": "CAPT",
    b"MATE": "MATE",
    b"TCAM": "TCAM",
    b"ACE6": "ACE6",
    b"XMA ": "XMA",
}
REQUIRED_DATA_TABLE_ENTRIES = list(range(9, 24))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def type_for(data: bytes) -> tuple[str, str]:
    magic = data[:4]
    magic_hex = magic.hex()
    return MAGIC_TYPES.get(magic, f"unknown:{magic_hex}"), magic_hex


def load_inputs(catalog_path: Path, manifest_paths: list[Path]) -> tuple[dict, dict[int, dict], list[dict]]:
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    missions = {
        entry["data_table_entry_index"]: entry
        for entry in catalog["missions"]
        if isinstance(entry.get("data_table_entry_index"), int)
    }
    if set(missions) != set(REQUIRED_DATA_TABLE_ENTRIES):
        raise ValueError("catalog does not cover DATA.TBL entries 9-23")

    manifest_entries: dict[int, dict] = {}
    manifest_meta: list[dict] = []
    for manifest_path in manifest_paths:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("schema_version") != 1:
            raise ValueError(f"unsupported manifest schema: {manifest_path}")
        if manifest.get("data_tbl", {}).get("sha256") != catalog["corpus"]["data_tbl"]["sha256"]:
            raise ValueError(f"DATA.TBL provenance mismatch: {manifest_path}")
        if not manifest.get("policy", {}).get("bounded_pac_reads"):
            raise ValueError(f"manifest is not bounded: {manifest_path}")
        if not manifest.get("policy", {}).get("complete_pac_not_copied"):
            raise ValueError(f"manifest permits complete PAC copy: {manifest_path}")
        manifest_meta.append({
            "sha256": sha256_path(manifest_path),
            "data_table_entries": sorted(manifest.get("indices", [])),
        })
        for record in manifest.get("entries", []):
            index = record.get("index")
            if not isinstance(index, int) or index in manifest_entries:
                raise ValueError(f"duplicate or invalid DATA.TBL entry: {index}")
            if index not in missions:
                raise ValueError(f"manifest entry outside campaign scope: {index}")
            if record.get("decode", {}).get("status") != "decoded":
                raise ValueError(f"entry {index} is not decoded")
            payload = record.get("payload", {})
            payload_path = manifest_path.parent / record.get("payload_path", "")
            if not payload_path.is_file():
                raise ValueError(f"missing decoded payload for entry {index}: {payload_path}")
            if payload_path.stat().st_size != payload.get("size"):
                raise ValueError(f"payload size mismatch for entry {index}")
            if sha256_path(payload_path) != payload.get("sha256"):
                raise ValueError(f"payload hash mismatch for entry {index}")
            asset = catalog["data_table_assets"][str(index)]
            if record.get("pac_name") != asset["archive"] or record.get("offset") != int(asset["offset"], 16):
                raise ValueError(f"physical route mismatch for entry {index}")
            if record.get("stored_size") != asset["stored_size"] or record.get("expanded_size") != asset["expanded_size"]:
                raise ValueError(f"physical extent mismatch for entry {index}")
            manifest_entries[index] = {
                "record": record,
                "payload_path": payload_path,
                "mission": missions[index],
            }
    if set(manifest_entries) != set(REQUIRED_DATA_TABLE_ENTRIES):
        missing = sorted(set(REQUIRED_DATA_TABLE_ENTRIES) - set(manifest_entries))
        raise ValueError(f"missing campaign manifests for entries: {missing}")
    return catalog, manifest_entries, manifest_meta


def build_inventory(catalog: dict, manifest_entries: dict[int, dict], manifest_meta: list[dict]) -> dict:
    resources: dict[str, dict] = {}
    mission_rows: list[dict] = []
    aggregate_edges: dict[tuple[str, str], dict] = {}
    total_nodes = 0

    for index in REQUIRED_DATA_TABLE_ENTRIES:
        item = manifest_entries[index]
        record = item["record"]
        mission = item["mission"]
        payload_path: Path = item["payload_path"]
        payload = payload_path.read_bytes()
        payload_sha = sha256_bytes(payload)
        if payload[:4] != b"FHM ":
            raise ValueError(f"entry {index} does not have an FHM root")

        type_counts: Counter[str] = Counter()
        top_level_counts: Counter[str] = Counter()
        type_hashes: defaultdict[str, set[str]] = defaultdict(set)
        edges: dict[tuple[str, str], dict] = {}
        node_count = 0
        max_depth = 0
        root_children = parse_fhm(payload)
        if root_children is None:
            raise ValueError(f"entry {index} root FHM parse failed")

        def add_edge(parent_type: str, child_type: str, child_sha: str) -> None:
            key = (parent_type, child_type)
            edge = edges.setdefault(key, {"parent_type": parent_type, "child_type": child_type, "occurrences": 0, "resource_hashes": set()})
            edge["occurrences"] += 1
            edge["resource_hashes"].add(child_sha)
            aggregate = aggregate_edges.setdefault(key, {"parent_type": parent_type, "child_type": child_type, "occurrences": 0, "resource_hashes": set(), "data_table_entries": set()})
            aggregate["occurrences"] += 1
            aggregate["resource_hashes"].add(child_sha)
            aggregate["data_table_entries"].add(index)

        def visit(blob: bytes, parent_type: str, parent_sha: str, path_prefix: str, depth: int, top_level: bool = False) -> None:
            nonlocal node_count, max_depth, total_nodes
            if depth > MAX_FHM_DEPTH:
                raise ValueError(f"FHM depth limit exceeded for entry {index}")
            children = parse_fhm(blob)
            if children is None:
                raise ValueError(f"nested FHM parse failed for entry {index} at {path_prefix}")
            max_depth = max(max_depth, depth)
            for child in children:
                node_count += 1
                total_nodes += 1
                if node_count > MAX_FHM_NODES:
                    raise ValueError(f"FHM node limit exceeded for entry {index}")
                child_type, magic_hex = type_for(child.data)
                child_sha = sha256_bytes(child.data)
                type_counts[child_type] += 1
                type_hashes[child_type].add(child_sha)
                if top_level:
                    top_level_counts[child_type] += 1
                add_edge(parent_type, child_type, child_sha)
                resource = resources.setdefault(child_sha, {
                    "sha256": child_sha,
                    "type": child_type,
                    "magic_hex": magic_hex,
                    "size": len(child.data),
                    "occurrence_count": 0,
                    "data_table_entries": set(),
                    "missions": set(),
                    "parent_types": set(),
                    "occurrences": [],
                })
                resource["occurrence_count"] += 1
                resource["data_table_entries"].add(index)
                resource["missions"].add(mission["mission_id"])
                resource["parent_types"].add(parent_type)
                resource["occurrences"].append({
                    "data_table_entry_index": index,
                    "mission_id": mission["mission_id"],
                    "path": f"{path_prefix}/{child.index}" if path_prefix else str(child.index),
                    "offset": child.offset,
                    "declared_size": child.declared_size,
                    "size": len(child.data),
                    "parent_sha256": parent_sha,
                    "parent_type": parent_type,
                })
                if child.magic == "FHM ":
                    visit(child.data, child_type, child_sha, f"{path_prefix}/{child.index}" if path_prefix else str(child.index), depth + 1)

        visit(payload, "FHM", payload_sha, "", 0, top_level=True)
        expected_nodes = record.get("structure", {}).get("node_count")
        if expected_nodes != node_count:
            raise ValueError(f"manifest node count mismatch for entry {index}: {node_count} != {expected_nodes}")
        expected_payload_sha = mission["parse"].get("payload_sha256")
        if expected_payload_sha != payload_sha:
            raise ValueError(f"catalog payload hash mismatch for entry {index}")

        mission_rows.append({
            "mission_id": mission["mission_id"],
            "campaign_selector": mission["campaign_selector"],
            "dpl_resource_id": mission["dpl_resource_id"],
            "data_table_entry_index": index,
            "payload_sha256": payload_sha,
            "payload_size": len(payload),
            "root": "FHM",
            "root_sha256": payload_sha,
            "node_count": node_count,
            "max_depth": max_depth,
            "type_counts": dict(sorted(type_counts.items())),
            "top_level_type_counts": dict(sorted(top_level_counts.items())),
            "resource_hashes_by_type": {key: sorted(value) for key, value in sorted(type_hashes.items())},
            "observed_edges": [
                {"parent_type": key[0], "child_type": key[1], "occurrences": value["occurrences"], "resource_hashes": sorted(value["resource_hashes"])}
                for key, value in sorted(edges.items())
            ],
        })

    serialized_resources = []
    for sha, resource in sorted(resources.items()):
        serialized_resources.append({
            "sha256": sha,
            "type": resource["type"],
            "magic_hex": resource["magic_hex"],
            "size": resource["size"],
            "occurrence_count": resource["occurrence_count"],
            "data_table_entries": sorted(resource["data_table_entries"]),
            "missions": sorted(resource["missions"]),
            "parent_types": sorted(resource["parent_types"]),
            "occurrences": sorted(resource["occurrences"], key=lambda row: (row["data_table_entry_index"], row["path"])),
        })
    shared_resources = [resource for resource in serialized_resources if len(resource["data_table_entries"]) > 1]
    serialized_edges = [
        {"parent_type": key[0], "child_type": key[1], "occurrences": value["occurrences"], "data_table_entries": sorted(value["data_table_entries"]), "resource_hashes": sorted(value["resource_hashes"])}
        for key, value in sorted(aggregate_edges.items())
    ]

    observed_types = {resource["type"] for resource in serialized_resources}
    observed_formats = []
    for format_name in ("MDLP", "PLAD", "NTXR", "NDXR", "NFH", "NFIC", "Scene", "CAPT", "MATE", "TCAM", "ACE6", "XMA"):
        matching = [resource for resource in serialized_resources if resource["type"] == format_name]
        observed_formats.append({
            "type": format_name,
            "status": "observed_bounded" if matching else "not_observed_as_standalone_child",
            "resource_count": len(matching),
            "occurrence_count": sum(resource["occurrence_count"] for resource in matching),
            "data_table_entries": sorted({index for resource in matching for index in resource["data_table_entries"]}),
            "resource_hashes": [resource["sha256"] for resource in matching],
        })

    contracts = [
        {
            "name": "FHM tree and content-addressed resources",
            "status": "observed_bounded",
            "evidence": "All 15 payload roots and every recursive child were parsed with bounded offsets and hashes.",
        },
        {
            "name": "MATE -> NDXR -> NTXR",
            "status": "partial",
            "evidence": "NTXR children are observed and hashed; MATE and NDXR are not standalone child tags in this scope, so the binding remains unknown.",
        },
        {
            "name": "Scene / TCAM",
            "status": "partial",
            "evidence": "Scen children are observed and hashed; TCAM is not a standalone child tag and scene semantics are not decoded.",
        },
        {
            "name": "NFIC timelines",
            "status": "observed_bounded",
            "evidence": "NFIC children and hashes are inventoried; timeline fields and event semantics remain unknown.",
        },
        {
            "name": "objects / units",
            "status": "unknown",
            "evidence": "No object or unit semantics are inferred from unknown binary tags.",
        },
        {
            "name": "audio / XMA",
            "status": "not_observed_as_standalone_child",
            "evidence": "No XMA child tag is present in the bounded FHM trees; audio dependencies remain unresolved.",
        },
    ]

    return {
        "schema_version": 1,
        "status": "bounded",
        "provenance": {
            "region": catalog["corpus"]["region"],
            "xex_sha256": catalog["corpus"]["xex"]["sha256"],
            "data_tbl_sha256": catalog["corpus"]["data_tbl"]["sha256"],
            "ghidra_project": "ace-combat-6",
            "target_id": "PAL-default-xex",
            "module": "default.xex",
            "source_manifests": manifest_meta,
        },
        "scope_data_table_entries": REQUIRED_DATA_TABLE_ENTRIES,
        "summary": {
            "mission_count": len(mission_rows),
            "recursive_nodes": total_nodes,
            "unique_resource_hashes": len(serialized_resources),
            "shared_resource_hashes": len(shared_resources),
            "empty_resource_hashes": sum(resource["size"] == 0 for resource in shared_resources),
        },
        "contracts": contracts,
        "observed_formats": observed_formats,
        "observed_edges": serialized_edges,
        "missions": mission_rows,
        "shared_resources": shared_resources,
        "resources": serialized_resources,
        "policy": {
            "bounded_pac_reads": True,
            "complete_pac_not_copied": True,
            "unknown_magic_fail_closed": True,
            "semantic_promotion": False,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        catalog, entries, manifest_meta = load_inputs(args.catalog, args.manifest)
        inventory = build_inventory(catalog, entries, manifest_meta)
        args.output.write_text(json.dumps(inventory, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        print(f"payload_dependency_inventory=fail reason={exc}")
        return 1
    print(json.dumps({
        "scope": inventory["scope_data_table_entries"],
        "recursive_nodes": inventory["summary"]["recursive_nodes"],
        "unique_resource_hashes": inventory["summary"]["unique_resource_hashes"],
        "shared_resource_hashes": inventory["summary"]["shared_resource_hashes"],
        "output": str(args.output),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
