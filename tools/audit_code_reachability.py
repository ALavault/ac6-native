#!/usr/bin/env python3
"""Fail-closed audit for the AC6 code reachability inventory."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

SHA256 = re.compile(r"^[0-9a-f]{64}$")
ROOT_ROLES = {
    "campaign_selection",
    "campaign_loading",
    "mission_hsm",
    "mission_update",
    "event_dispatch",
    "unit_factory",
    "radio_dispatch",
}


def fail(reason: str) -> int:
    print(f"code_inventory=fail reason={reason}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("inventory", type=Path)
    parser.add_argument("--xex", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.inventory.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"read:{exc}")
    if document.get("schema_version") != 1:
        return fail("schema_version")
    provenance = document.get("provenance")
    if not isinstance(provenance, dict) or provenance.get("ghidra_project") != "ace-combat-6" or \
            provenance.get("target_id") != "PAL-default-xex" or \
            not SHA256.fullmatch(provenance.get("xex_sha256", "")):
        return fail("provenance")
    if args.xex and hashlib.sha256(args.xex.read_bytes()).hexdigest() != provenance["xex_sha256"]:
        return fail("xex_sha256_mismatch")
    roots = document.get("roots")
    if not isinstance(roots, list) or {root.get("role") for root in roots if isinstance(root, dict)} != ROOT_ROLES:
        return fail("root_roles")
    entries = document.get("entries")
    if not isinstance(entries, list) or not entries:
        return fail("entries")
    entry_ids = set()
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str) or not entry["id"]:
            return fail("entry_id")
        if entry["id"] in entry_ids:
            return fail(f"duplicate_entry:{entry['id']}")
        entry_ids.add(entry["id"])
        if not isinstance(entry.get("address"), (str, type(None))) or not isinstance(entry.get("module"), str) or not entry["module"]:
            return fail(f"entry_location:{entry['id']}")
        if not all(isinstance(entry.get(key), list) for key in ("callers", "callees", "missions")):
            return fail(f"entry_graph:{entry['id']}")
        if not all(isinstance(mission, int) and 1 <= mission <= 15 for mission in entry["missions"]):
            return fail(f"entry_missions:{entry['id']}")
        if entry.get("proof") not in {"native_test", "ghidra_qualified", "unknown"}:
            return fail(f"entry_proof:{entry['id']}")
        if entry.get("proof") == "unknown" and (entry.get("address") is not None or not entry.get("gaps")):
            return fail(f"unknown_entry_gap:{entry['id']}")
    covered = 0
    unknown = 0
    for root in roots:
        if not isinstance(root, dict) or root.get("role") not in ROOT_ROLES:
            return fail("root_shape")
        status = root.get("status")
        root_entries = root.get("entry_ids")
        retail_status = root.get("retail_status")
        if status not in {"covered", "unknown"} or retail_status not in {"covered", "partial", "unknown"} or not isinstance(root_entries, list):
            return fail(f"root_status:{root.get('role')}")
        if any(entry_id not in entry_ids for entry_id in root_entries):
            return fail(f"root_entry_reference:{root['role']}")
        if status == "covered" and not root_entries:
            return fail(f"covered_without_entry:{root['role']}")
        if status == "unknown" and not root.get("gaps"):
            return fail(f"unknown_without_gap:{root['role']}")
        if retail_status in {"partial", "unknown"} and not root.get("retail_gaps"):
            return fail(f"retail_{retail_status}_without_gap:{root['role']}")
        covered += status == "covered"
        unknown += retail_status == "unknown"
    partial = sum(root.get("retail_status") == "partial" for root in roots)
    print(f"code_inventory=pass roots={len(roots)} native_covered={covered} retail_partial={partial} retail_unknown={unknown} entries={len(entries)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
