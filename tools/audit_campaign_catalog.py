#!/usr/bin/env python3
"""Fail-closed audit for the machine-readable 15-mission campaign catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
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
        if status == "unqualified" and parse["status"] != "not_attempted":
            return fail(f"unqualified_parse:{entry['mission_id']}")
    for label, path, key in (("xex", args.xex, "xex"), ("data_tbl", args.data_tbl, "data_tbl")):
        if path is not None:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != corpus[key]["sha256"]:
                return fail(f"{label}_sha256_mismatch")
    qualified = sum(entry["status"] == "qualified" for entry in missions)
    partial = sum(entry["status"] == "partial" for entry in missions)
    unknown = sum(entry["status"] == "unqualified" for entry in missions)
    print(f"campaign_catalog=pass missions=15 qualified={qualified} partial={partial} unqualified={unknown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
