#!/usr/bin/env python3
"""Fail-closed structural audit for the global offline mission ladder."""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MATRIX = ROOT / "analysis/mission-capability-matrix.tsv"
TEMPLATE = ROOT / "analysis/templates/mission-gate-template.json"
LADDER = ROOT / "GLOBAL_OFFLINE_LADDER.md"
GATES = ("JF", "JV", "JP", "JG")
STATES = {"open", "passed"}
XEX = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"


def audit() -> None:
    document = json.loads(TEMPLATE.read_text(encoding="utf-8"))
    if document.get("schema") != "ac6.offline-mission-gate.v1":
        raise ValueError("template schema")
    if tuple(document.get("gates", {}).keys()) != GATES:
        raise ValueError("template gate coverage/order")
    provenance = document.get("provenance", {})
    if provenance.get("ghidra_project") != "ace-combat-6":
        raise ValueError("canonical Ghidra project")
    if provenance.get("xex_sha256") != XEX:
        raise ValueError("canonical XEX identity")
    if document.get("policy", {}).get("all_gates_required_for_support") is not True:
        raise ValueError("support policy")

    with MATRIX.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    expected = [f"M{number:02d}" for number in range(1, 16)]
    if [row.get("mission") for row in rows] != expected:
        raise ValueError("matrix must contain M01..M15 exactly once in order")
    for row in rows:
        if any(row.get(gate) not in STATES for gate in GATES):
            raise ValueError(f"invalid gate state: {row.get('mission')}")
        should_support = all(row[gate] == "passed" for gate in GATES)
        if (row.get("supported") == "yes") != should_support:
            raise ValueError(f"support disagrees with gates: {row.get('mission')}")
    ladder = LADDER.read_text(encoding="utf-8")
    for checkpoint in range(8):
        if f"{checkpoint} —" not in ladder:
            raise ValueError(f"missing checkpoint {checkpoint}")
    print("global_ladder=pass missions=15 gates=4 checkpoints=8")


if __name__ == "__main__":
    try:
        audit()
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"global_ladder=fail error={error}", file=sys.stderr)
        raise SystemExit(1)
