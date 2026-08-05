#!/usr/bin/env python3
"""Fail-closed audit for the machine-readable 15-mission campaign catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

SHA256 = re.compile(r"^[0-9a-f]{64}$")


def fail(message: str) -> int:
    print(f"campaign_catalog=fail reason={message}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("--xex", type=Path)
    parser.add_argument("--data-tbl", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.catalog.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"read:{exc}")
    if document.get("schema_version") != 1:
        return fail("schema_version")
    corpus = document.get("corpus")
    if not isinstance(corpus, dict) or corpus.get("region") != "PAL":
        return fail("corpus")
    for key in ("xex", "data_tbl"):
        record = corpus.get(key)
        if not isinstance(record, dict) or not SHA256.fullmatch(record.get("sha256", "")):
            return fail(f"corpus_{key}_sha256")
        if not isinstance(record.get("path"), str) or not record["path"]:
            return fail(f"corpus_{key}_path")
    selector_dpl = document.get("selector_dpl_evidence")
    expected_mapping = {str(selector): selector + 8 for selector in range(1, 16)}
    if not isinstance(selector_dpl, dict) or selector_dpl.get("status") != "qualified":
        return fail("selector_dpl_evidence")
    if selector_dpl.get("mode") != 1:
        return fail("selector_dpl_mode")
    if selector_dpl.get("function_address") != "0x821B6E58":
        return fail("selector_dpl_function")
    if selector_dpl.get("table_address") != "0x82065840":
        return fail("selector_dpl_table")
    if selector_dpl.get("module") != "default.xex":
        return fail("selector_dpl_module")
    if selector_dpl.get("ghidra_project") != "ace-combat-6":
        return fail("selector_dpl_project")
    if selector_dpl.get("target_id") != "PAL-default-xex":
        return fail("selector_dpl_target")
    if selector_dpl.get("xex_sha256") != corpus["xex"]["sha256"]:
        return fail("selector_dpl_xex_sha256")
    if selector_dpl.get("mapping") != expected_mapping:
        return fail("selector_dpl_mapping")
    if not isinstance(selector_dpl.get("unknown_after_dpl"), str) or not selector_dpl["unknown_after_dpl"]:
        return fail("selector_dpl_boundary")
    dpl_data_table = document.get("dpl_data_table_evidence")
    if not isinstance(dpl_data_table, dict) or dpl_data_table.get("status") != "qualified":
        return fail("dpl_data_table_evidence")
    if dpl_data_table.get("request_function_address") != "0x821D1128":
        return fail("dpl_data_table_request_function")
    if dpl_data_table.get("queue_function_address") != "0x821CD130":
        return fail("dpl_data_table_queue_function")
    if dpl_data_table.get("loader_function_address") != "0x821CC250":
        return fail("dpl_data_table_loader_function")
    if dpl_data_table.get("module") != "default.xex":
        return fail("dpl_data_table_module")
    if dpl_data_table.get("ghidra_project") != "ace-combat-6":
        return fail("dpl_data_table_project")
    if dpl_data_table.get("target_id") != "PAL-default-xex":
        return fail("dpl_data_table_target")
    if dpl_data_table.get("xex_sha256") != corpus["xex"]["sha256"]:
        return fail("dpl_data_table_xex_sha256")
    if dpl_data_table.get("data_tbl_sha256") != corpus["data_tbl"]["sha256"]:
        return fail("dpl_data_table_data_tbl_sha256")
    if dpl_data_table.get("entry_count") != 926:
        return fail("dpl_data_table_entry_count")
    if dpl_data_table.get("direct_id_exclusive") != "0x39D":
        return fail("dpl_data_table_boundary")
    if dpl_data_table.get("mapping_rule") != "dpl_resource_id == data_table_entry_index for 0 <= id < 0x39D":
        return fail("dpl_data_table_mapping_rule")
    if dpl_data_table.get("unknown_at_or_above") != "0x39D":
        return fail("dpl_data_table_unknown_boundary")
    data_table_assets = document.get("data_table_assets")
    expected_asset_keys = {str(index) for index in range(9, 24)}
    if not isinstance(data_table_assets, dict) or set(data_table_assets) != expected_asset_keys:
        return fail("data_table_assets_coverage")
    for key, asset in data_table_assets.items():
        if not isinstance(asset, dict) or asset.get("archive") not in {"DATA00.PAC", "DATA01.PAC"}:
            return fail(f"data_table_asset:{key}")
        if type(asset.get("bank_index")) is not int or asset["bank_index"] < 0:
            return fail(f"data_table_asset_bank:{key}")
        if type(asset.get("storage_class")) is not int or asset["storage_class"] < 0:
            return fail(f"data_table_asset_storage:{key}")
        if not isinstance(asset.get("offset"), str) or not re.fullmatch(r"0x[0-9A-Fa-f]+", asset["offset"]):
            return fail(f"data_table_asset_offset:{key}")
        for field in ("stored_size", "expanded_size"):
            if type(asset.get(field)) is not int or asset[field] <= 0:
                return fail(f"data_table_asset_{field}:{key}")
    dependency_hashes = document.get("payload_dependency_hashes")
    if not isinstance(dependency_hashes, dict) or dependency_hashes.get("status") != "bounded":
        return fail("payload_dependency_hashes")
    if dependency_hashes.get("scope_data_table_entries") != [11, 12, 13, 14, 15, 16, 17, 18, 19]:
        return fail("payload_dependency_scope")
    for field in ("recursive_nodes", "unique_node_hashes", "shared_hash_group_count", "nonempty_shared_hash_group_count", "empty_shared_hash_group_count"):
        if type(dependency_hashes.get(field)) is not int or dependency_hashes[field] <= 0:
            return fail(f"payload_dependency_{field}")
    groups = dependency_hashes.get("groups")
    if not isinstance(groups, list) or len(groups) != dependency_hashes["nonempty_shared_hash_group_count"]:
        return fail("payload_dependency_groups")
    seen_hashes: set[str] = set()
    for group in groups:
        if not isinstance(group, dict) or not SHA256.fullmatch(group.get("sha256", "")):
            return fail("payload_dependency_group_hash")
        if group["sha256"] in seen_hashes:
            return fail("payload_dependency_duplicate_hash")
        seen_hashes.add(group["sha256"])
        if not isinstance(group.get("magic_hex"), str) or not re.fullmatch(r"[0-9a-fA-F]{8}", group["magic_hex"]):
            return fail("payload_dependency_group_magic")
        if type(group.get("size")) is not int or group["size"] <= 0:
            return fail("payload_dependency_group_size")
        entries = group.get("entries")
        if not isinstance(entries, list) or len(entries) < 2 or not all(entry in dependency_hashes["scope_data_table_entries"] for entry in entries):
            return fail("payload_dependency_group_entries")
    if dependency_hashes["shared_hash_group_count"] != len(groups) + dependency_hashes["empty_shared_hash_group_count"]:
        return fail("payload_dependency_group_count")
    missions = document.get("missions")
    if not isinstance(missions, list) or len(missions) != 15:
        return fail("mission_count")
    ids = [entry.get("mission_id") for entry in missions if isinstance(entry, dict)]
    if sorted(ids) != list(range(1, 16)):
        return fail("mission_ids")
    for entry in missions:
        status = entry.get("status")
        if status not in {"qualified", "partial", "unqualified"}:
            return fail(f"status:{entry.get('mission_id')}")
        provenance = entry.get("provenance")
        if not isinstance(provenance, dict) or provenance.get("xex_sha256") != corpus["xex"]["sha256"] or provenance.get("data_tbl_sha256") != corpus["data_tbl"]["sha256"]:
            return fail(f"provenance:{entry.get('mission_id')}")
        selector = entry.get("campaign_selector")
        dpl_resource_id = entry.get("dpl_resource_id")
        if selector is not None or dpl_resource_id is not None:
            if type(selector) is not int or type(dpl_resource_id) is not int:
                return fail(f"selector_dpl_types:{entry['mission_id']}")
            if expected_mapping.get(str(selector)) != dpl_resource_id:
                return fail(f"selector_dpl_mapping:{entry['mission_id']}")
        data_table_entry_index = entry.get("data_table_entry_index")
        if type(dpl_resource_id) is int and 0 <= dpl_resource_id < 0x39D:
            if type(data_table_entry_index) is not int or data_table_entry_index != dpl_resource_id:
                return fail(f"dpl_data_table_mapping:{entry['mission_id']}")
        if type(data_table_entry_index) is int:
            asset = data_table_assets.get(str(data_table_entry_index))
            if not isinstance(asset, dict):
                return fail(f"data_table_asset_route:{entry['mission_id']}")
        route_fields = ("campaign_selector", "dpl_resource_id", "data_table_entry_index")
        route_complete = all(isinstance(entry.get(field), int) and entry[field] > 0 for field in route_fields)
        if status == "qualified" and not route_complete:
            return fail(f"qualified_route:{entry['mission_id']}")
        if status == "unqualified":
            gaps = entry.get("qualification_gaps")
            if not isinstance(gaps, list) or not gaps or not all(isinstance(gap, str) and gap for gap in gaps):
                return fail(f"unqualified_gaps:{entry['mission_id']}")
        parse = entry.get("parse")
        if not isinstance(parse, dict) or parse.get("status") not in {"decoded", "bounded", "not_attempted"}:
            return fail(f"parse:{entry['mission_id']}")
        if type(data_table_entry_index) is int:
            asset = data_table_assets[str(data_table_entry_index)]
            if parse.get("status") == "decoded" and (
                parse.get("stored_size") != asset["stored_size"]
                or parse.get("expanded_size") != asset["expanded_size"]
            ):
                return fail(f"decoded_asset_extent:{entry['mission_id']}")
        if parse.get("status") == "decoded":
            for field in ("stored_sha256", "payload_sha256"):
                if not SHA256.fullmatch(parse.get(field, "")):
                    return fail(f"decoded_{field}:{entry['mission_id']}")
            for field in ("stored_size", "expanded_size"):
                if type(parse.get(field)) is not int or parse[field] <= 0:
                    return fail(f"decoded_{field}:{entry['mission_id']}")
            if parse.get("codec") != "mode1_pi_xor_raw_deflate":
                return fail(f"decoded_codec:{entry['mission_id']}")
            structure = parse.get("structure")
            if not isinstance(structure, dict) or structure.get("root") != "FHM":
                return fail(f"decoded_structure:{entry['mission_id']}")
            for field in ("top_level_child_count", "recursive_manifest_rows", "nested_fhm_count"):
                if type(structure.get(field)) is not int or structure[field] < 0:
                    return fail(f"decoded_structure_{field}:{entry['mission_id']}")
            if structure.get("parse_failures") != 0:
                return fail(f"decoded_structure_failures:{entry['mission_id']}")
        if status == "unqualified" and parse["status"] != "not_attempted":
            return fail(f"unqualified_parse:{entry['mission_id']}")
    catalog_mapping = {
        str(entry["campaign_selector"]): entry["dpl_resource_id"]
        for entry in missions
        if type(entry.get("campaign_selector")) is int and type(entry.get("dpl_resource_id")) is int
    }
    if catalog_mapping != expected_mapping:
        return fail("selector_dpl_catalog_coverage")
    for label, path, key in (("xex", args.xex, "xex"), ("data_tbl", args.data_tbl, "data_tbl")):
        if path is not None:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != corpus[key]["sha256"]:
                return fail(f"{label}_sha256_mismatch")
            if label == "data_tbl":
                raw = path.read_bytes()
                if len(raw) < 8:
                    return fail("data_tbl_header")
                entry_count, _pack_count = struct.unpack_from(">II", raw, 0)
                if len(raw) != 8 + entry_count * 16 or entry_count < 24:
                    return fail("data_tbl_shape")
                for index in range(9, 24):
                    group, offset, stored_size, expanded_size = struct.unpack_from(">4I", raw, 8 + index * 16)
                    asset = data_table_assets[str(index)]
                    archive = "DATA01.PAC" if group & 0x01000000 else "DATA00.PAC"
                    if archive != asset["archive"] or offset != int(asset["offset"], 16) or stored_size != asset["stored_size"] or expanded_size != asset["expanded_size"]:
                        return fail(f"data_tbl_asset_mismatch:{index}")
    qualified = sum(entry["status"] == "qualified" for entry in missions)
    partial = sum(entry["status"] == "partial" for entry in missions)
    unknown = sum(entry["status"] == "unqualified" for entry in missions)
    print(f"campaign_catalog=pass missions=15 qualified={qualified} partial={partial} unqualified={unknown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
