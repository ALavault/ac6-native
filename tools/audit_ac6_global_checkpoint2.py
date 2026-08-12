#!/usr/bin/env python3
"""Fail-closed audit for the global AC6 structural checkpoint."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

from audit_ac6_oracle_reproducibility import validate_document


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "analysis/contracts/global-checkpoint-2-v1.json"
SCHEMA = "ac6.global-checkpoint-2.v1"
XEX_SHA256 = (
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
)
LANES = (
    "vmx-vmx128",
    "scene-tcam",
    "mdlp-mate",
    "ndxr-ntxr-xenos",
    "objectives-campaign",
    "xma-asf",
)
STATES = {"open", "passed"}


class Checkpoint2Error(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise Checkpoint2Error(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def project_path(relative: object) -> Path:
    require(
        isinstance(relative, str)
        and relative != ""
        and not Path(relative).is_absolute(),
        "checkpoint evidence path",
    )
    candidate = (ROOT / relative).resolve()
    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError as error:
        raise Checkpoint2Error("checkpoint evidence escapes project") from error
    return candidate


def audit_evidence(record: object, label: str) -> Path:
    require(
        isinstance(record, dict)
        and set(record) == {"path", "size", "sha256"},
        f"{label} evidence shape",
    )
    path = project_path(record["path"])
    require(
        path.is_file()
        and path.stat().st_size == record["size"]
        and sha256(path) == record["sha256"],
        f"{label} evidence identity",
    )
    return path


def audit_document(document: dict) -> None:
    require(
        document.get("schema") == SCHEMA
        and document.get("checkpoint") == 2
        and document.get("state") in STATES,
        "checkpoint identity",
    )
    target = document.get("target", {})
    require(
        target
        == {
            "platform": "Xbox 360 PAL",
            "module": "default.xex",
            "xex_sha256": XEX_SHA256,
            "ghidra_project": "ace-combat-6",
        },
        "checkpoint target",
    )
    prerequisites = document.get("prerequisites")
    require(
        isinstance(prerequisites, list)
        and [item.get("checkpoint") for item in prerequisites] == [0, 1]
        and all(item.get("state") == "passed" for item in prerequisites),
        "checkpoint prerequisites",
    )
    for item in prerequisites:
        audit_evidence(item.get("evidence"), f"checkpoint {item['checkpoint']}")

    reproducibility = document.get("reproducibility", {})
    require(reproducibility.get("state") == "passed", "reproducibility state")
    reproduction_path = audit_evidence(
        reproducibility.get("evidence"), "reproducibility"
    )
    validate_document(
        json.loads(reproduction_path.read_text(encoding="utf-8")), ROOT
    )

    expected_missions = [f"M{number:02d}" for number in range(1, 16)]
    require(
        document.get("corpus_missions") == expected_missions,
        "checkpoint mission corpus",
    )
    lanes = document.get("lanes")
    require(
        isinstance(lanes, list)
        and tuple(lane.get("id") for lane in lanes) == LANES,
        "checkpoint lanes",
    )
    for lane in lanes:
        lane_id = lane["id"]
        state = lane.get("state")
        require(state in STATES, f"{lane_id} state")
        require(
            isinstance(lane.get("closure"), str) and lane["closure"],
            f"{lane_id} closure",
        )
        blockers = lane.get("blockers")
        if state == "open":
            require(
                isinstance(blockers, list)
                and blockers
                and all(isinstance(item, str) and item for item in blockers),
                f"{lane_id} blockers",
            )
        else:
            require(blockers == [], f"{lane_id} passed blockers")
        evidence = lane.get("evidence")
        require(
            isinstance(evidence, list) and evidence,
            f"{lane_id} evidence",
        )
        for record in evidence:
            audit_evidence(record, lane_id)

    all_passed = all(lane["state"] == "passed" for lane in lanes)
    require(
        (document["state"] == "passed") == all_passed,
        "checkpoint state outruns lanes",
    )
    policy = document.get("policy", {})
    required_policy = {
        "all_lanes_required_for_pass",
        "shared_reader_change_requires_all_15_missions",
        "unknown_behavior_fails_closed",
        "oracle_is_not_product",
        "generated_code_is_not_product",
        "mission_progress_cannot_close_checkpoint",
    }
    require(
        set(policy) == required_policy and all(policy.values()),
        "checkpoint policy",
    )
    commands = document.get("validation_commands")
    require(
        isinstance(commands, list)
        and len(commands) == len(set(commands))
        and {
            "python3 tools/audit_ac6_global_ladder.py",
            "python3 tools/audit_ac6_global_checkpoint2.py",
            "python3 tools/audit_ac6_oracle_reproducibility.py",
            "python3 tools/audit_ac6_camera_selector_microexec.py",
            "git diff --check",
        }.issubset(commands),
        "checkpoint validation commands",
    )


def audit() -> None:
    document = json.loads(CONTRACT.read_text(encoding="utf-8"))
    audit_document(document)
    passed = sum(lane["state"] == "passed" for lane in document["lanes"])
    print(
        f"global_checkpoint2=pass state={document['state']} "
        f"lanes={passed}/{len(LANES)} missions=15"
    )


if __name__ == "__main__":
    try:
        audit()
    except (OSError, KeyError, json.JSONDecodeError, ValueError) as error:
        print(f"global_checkpoint2=fail error={error}", file=sys.stderr)
        raise SystemExit(1) from error
