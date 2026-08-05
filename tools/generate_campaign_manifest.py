#!/usr/bin/env python3
"""Generate the native campaign TSV from qualified catalog routes only."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def fail(message: str) -> int:
    print(f"campaign_manifest=fail reason={message}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("definitions", type=Path,
                        help="JSON gameplay definitions keyed by mission_id")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        catalog = json.loads(args.catalog.read_text())
        definitions = json.loads(args.definitions.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"read:{exc}")
    if catalog.get("schema_version") != 1 or not isinstance(catalog.get("missions"), list):
        return fail("catalog_schema")
    if not isinstance(definitions, dict) or not isinstance(definitions.get("missions"), dict):
        return fail("definitions_schema")
    qualified = [entry for entry in catalog["missions"]
                 if isinstance(entry, dict) and entry.get("status") == "qualified"]
    if not qualified:
        return fail("no_qualified_routes")
    rows = []
    seen = set()
    for entry in sorted(qualified, key=lambda item: item.get("mission_id", 0)):
        mission_id = entry.get("mission_id")
        if not isinstance(mission_id, int) or mission_id <= 0 or mission_id in seen:
            return fail(f"mission_id:{mission_id}")
        seen.add(mission_id)
        route = [entry.get("campaign_selector"), entry.get("dpl_resource_id"),
                 entry.get("data_table_entry_index")]
        if not all(isinstance(value, int) and value > 0 for value in route):
            return fail(f"route:{mission_id}")
        definition = definitions["missions"].get(str(mission_id))
        if not isinstance(definition, dict):
            return fail(f"missing_gameplay_definition:{mission_id}")
        objective_count = definition.get("objective_count")
        prerequisites = definition.get("prerequisites", [])
        if not isinstance(objective_count, int) or not 1 <= objective_count <= 32:
            return fail(f"objective_count:{mission_id}")
        if (not isinstance(prerequisites, list) or
                not all(isinstance(value, int) and value > 0 for value in prerequisites) or
                len(set(prerequisites)) != len(prerequisites) or mission_id in prerequisites):
            return fail(f"prerequisites:{mission_id}")
        prerequisite_text = "-" if not prerequisites else ",".join(map(str, sorted(prerequisites)))
        rows.append("\t".join(map(str, [mission_id, *route, objective_count, prerequisite_text])))
    unknown_definitions = set(definitions["missions"]) - {str(mission_id) for mission_id in seen}
    if unknown_definitions:
        return fail("definition_without_qualified_route:" + ",".join(sorted(unknown_definitions)))
    output = "# generated_from_catalog_sha256=" + hashlib.sha256(args.catalog.read_bytes()).hexdigest() + "\n"
    output += "# native gameplay definitions are separate from retail route qualification\n"
    output += "# mission_id\tselector\tdpl_resource_id\tdata_table_entry\tobjective_count\tprerequisites\n"
    output += "\n".join(rows) + "\n"
    try:
        args.output.write_text(output)
    except OSError as exc:
        return fail(f"write:{exc}")
    print(f"campaign_manifest=pass qualified={len(rows)} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
