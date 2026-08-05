#!/usr/bin/env python3
"""Fail-closed audit for the durable AC6 payload dependency inventory."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

SHA256 = re.compile(r"^[0-9a-f]{64}$")
SCOPE = list(range(9, 24))


def fail(message: str) -> int:
    print(f"payload_dependency_inventory=fail reason={message}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("inventory", type=Path)
    parser.add_argument("--catalog", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.inventory.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"read:{exc}")
    if document.get("schema_version") != 1 or document.get("status") != "bounded":
        return fail("schema_or_status")
    if document.get("scope_data_table_entries") != SCOPE:
        return fail("scope")
    provenance = document.get("provenance")
    if not isinstance(provenance, dict) or not SHA256.fullmatch(provenance.get("xex_sha256", "")) or not SHA256.fullmatch(provenance.get("data_tbl_sha256", "")):
        return fail("provenance")

    summary = document.get("summary")
    resources = document.get("resources")
    missions = document.get("missions")
    shared = document.get("shared_resources")
    if not isinstance(summary, dict) or not isinstance(resources, list) or not isinstance(missions, list) or not isinstance(shared, list):
        return fail("top_level_shapes")
    if summary.get("mission_count") != 15 or len(missions) != 15:
        return fail("mission_count")
    if sorted(row.get("data_table_entry_index") for row in missions) != SCOPE:
        return fail("mission_scope")
    if summary.get("unique_resource_hashes") != len(resources):
        return fail("unique_resource_count")
    by_hash = {}
    for resource in resources:
        if not isinstance(resource, dict) or not SHA256.fullmatch(resource.get("sha256", "")):
            return fail("resource_hash")
        sha = resource["sha256"]
        if sha in by_hash:
            return fail("duplicate_resource_hash")
        by_hash[sha] = resource
        if not isinstance(resource.get("type"), str) or not isinstance(resource.get("magic_hex"), str):
            return fail("resource_identity")
        if type(resource.get("size")) is not int or resource["size"] < 0:
            return fail("resource_size")
        occurrences = resource.get("occurrences")
        entries = resource.get("data_table_entries")
        missions_for_resource = resource.get("missions")
        if not isinstance(occurrences, list) or resource.get("occurrence_count") != len(occurrences):
            return fail("resource_occurrences")
        if not isinstance(entries, list) or not entries or not all(entry in SCOPE for entry in entries):
            return fail("resource_entries")
        if not isinstance(missions_for_resource, list) or not missions_for_resource:
            return fail("resource_missions")
        if "data" in resource or "payload" in resource:
            return fail("resource_contains_blob")
        occurrence_entries = sorted({row.get("data_table_entry_index") for row in occurrences})
        if occurrence_entries != entries:
            return fail("resource_occurrence_scope")
        for occurrence in occurrences:
            if not isinstance(occurrence, dict) or not SHA256.fullmatch(occurrence.get("parent_sha256", "")):
                return fail("occurrence_parent")
            if occurrence.get("data_table_entry_index") not in SCOPE or not isinstance(occurrence.get("path"), str):
                return fail("occurrence_identity")
    expected_shared = [resource for resource in resources if len(resource["data_table_entries"]) > 1]
    if summary.get("shared_resource_hashes") != len(expected_shared) or len(shared) != len(expected_shared):
        return fail("shared_resource_count")
    if summary.get("empty_resource_hashes") != sum(resource["size"] == 0 for resource in expected_shared):
        return fail("empty_resource_count")
    if {resource["sha256"] for resource in shared} != {resource["sha256"] for resource in expected_shared}:
        return fail("shared_resource_identity")
    if sum(row.get("node_count", -1) for row in missions) != summary.get("recursive_nodes"):
        return fail("recursive_node_count")

    resource_types = {sha: resource["type"] for sha, resource in by_hash.items()}
    formats = document.get("observed_formats")
    if not isinstance(formats, list):
        return fail("observed_formats")
    for format_row in formats:
        if not isinstance(format_row, dict) or not isinstance(format_row.get("type"), str):
            return fail("format_row")
        format_name = format_row["type"]
        matching = [resource for resource in resources if resource["type"] == format_name]
        expected_status = "observed_bounded" if matching else "not_observed_as_standalone_child"
        if format_row.get("status") != expected_status or format_row.get("resource_count") != len(matching):
            return fail(f"format_summary:{format_name}")
        if format_row.get("occurrence_count") != sum(resource["occurrence_count"] for resource in matching):
            return fail(f"format_occurrences:{format_name}")
        if sorted(format_row.get("resource_hashes", [])) != sorted(resource["sha256"] for resource in matching):
            return fail(f"format_hashes:{format_name}")

    for edge in document.get("observed_edges", []):
        if not isinstance(edge, dict) or not isinstance(edge.get("resource_hashes"), list):
            return fail("edge_shape")
        if not all(sha in resource_types for sha in edge["resource_hashes"]):
            return fail("edge_resource_hash")
        if not isinstance(edge.get("parent_type"), str) or not isinstance(edge.get("child_type"), str):
            return fail("edge_types")

    if args.catalog is not None:
        try:
            catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
            link = catalog["payload_dependency_inventory"]
            digest = hashlib.sha256(args.inventory.read_bytes()).hexdigest()
            if link.get("artifact_sha256") != digest or link.get("artifact_size") != args.inventory.stat().st_size:
                return fail("catalog_artifact_link")
            if link.get("recursive_nodes") != summary["recursive_nodes"] or link.get("unique_resource_hashes") != summary["unique_resource_hashes"]:
                return fail("catalog_summary_link")
        except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
            return fail(f"catalog:{exc}")
    print(f"payload_dependency_inventory=pass missions=15 nodes={summary['recursive_nodes']} unique={summary['unique_resource_hashes']} shared={summary['shared_resource_hashes']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
