#!/usr/bin/env python3
"""Fail-closed J0/J1 acceptance audit for the native AC6 Mission 01 slice."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA = "ac6.mission01-native-gate.v2"
V3_SCHEMA = "ac6.mission01-final-gate.v3"
V1_SCHEMA = "ac6.mission01-native-gate.v1"
CANONICAL_XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
CANONICAL_DATA_TBL_SHA256 = "82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5"
SHA256 = re.compile(r"^[0-9a-f]{64}$")
EVIDENCE_KINDS = {
    "derivation",
    "static",
    "microexec",
    "differential",
    "capsule",
    "native-test",
    "native-capture",
    "bridge",
    "retail",
}

J0_REQUIREMENTS = (
    "native_session_loop",
    "world_visible",
    "player_aircraft_visible",
    "gameplay_camera",
    "flight_input",
    "deterministic_replay",
)
J1_REQUIREMENTS = (
    "units_and_waves",
    "targeting",
    "weapons",
    "damage_and_destruction",
    "retail_objectives",
    "essential_hud",
    "scenario_radio_or_subtitles",
    "success_failure_debrief",
    "pause_save_restart",
)
REQUIRES_NATIVE_CAPTURE = {"world_visible", "player_aircraft_visible"}
REQUIRES_NATIVE_TEST = {
    "native_session_loop",
    "gameplay_camera",
    "flight_input",
    "deterministic_replay",
    *J1_REQUIREMENTS,
}
REQUIRES_RETAIL_SEMANTIC_EVIDENCE = {"retail_objectives"}
V2_DOMAINS = (
    "native_combat_mechanics",
    "native_units_and_waves",
    "retail_units_and_waves",
    "native_objective_flow",
    "retail_objectives",
    "essential_hud",
    "scenario_radio_or_subtitles",
    "success_failure_debrief",
)
V2_RETAIL_DOMAINS = {
    "retail_units_and_waves", "retail_objectives", "scenario_radio_or_subtitles"
}
V2_NATIVE_DOMAINS = {
    "native_combat_mechanics",
    "native_units_and_waves",
    "native_objective_flow",
    "essential_hud",
    "success_failure_debrief",
}
V2_RETAIL_GATE_REQUIREMENTS = (
    "J0",
    "native_combat_mechanics",
    "native_units_and_waves",
    "native_objective_flow",
    "essential_hud",
    "success_failure_debrief",
    "retail_units_and_waves",
    "retail_objectives",
    "scenario_radio_or_subtitles",
    "pause_save_restart",
)
V2_RETAIL_EVIDENCE_KINDS = {"static", "microexec", "differential", "retail"}


# A derivation may not cite generated or recompiled sources: those are literal
# cross-match evidence, never the origin of native behaviour.
FORBIDDEN_DERIVATION_MARKERS = (
    "ac6-recomp-reference",
    "/generated/",
    "ppc_recomp",
    "XenonRecomp",
    "xenonrecomp",
)

V3_REQUIRED_KINDS = {"static", "native-test", "derivation"}


class GateError(ValueError):
    pass


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def template() -> dict[str, Any]:
    def requirement(name: str) -> dict[str, Any]:
        return {
            "status": "open",
            "statement": name.replace("_", " "),
            "evidence": [],
        }

    return {
        "schema": SCHEMA,
        "provenance": {
            "repo_commit": "REPLACE_WITH_COMMIT_SHA",
            "xex_sha256": CANONICAL_XEX_SHA256,
            "data_tbl_sha256": CANONICAL_DATA_TBL_SHA256,
            "mission_id": 1,
        },
        "requirements": {
            "J0": {name: requirement(name) for name in J0_REQUIREMENTS},
            "domains": {name: requirement(name) for name in V2_DOMAINS},
            "runtime": {"pause_save_restart": requirement("pause_save_restart")},
        },
        "policy": {
            "bridge_is_supporting_evidence_only": True,
            "native_behavior_required_for_pass": True,
            "retail_assets_remain_external": True,
            "retail_semantics_qualified_required": True,
            "retail_gate_requires": list(V2_RETAIL_GATE_REQUIREMENTS),
        },
    }


def safe_artifact_path(root: Path, relative: str) -> Path:
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts:
        raise GateError(f"unsafe evidence path: {relative}")
    resolved_root = root.resolve()
    resolved = (resolved_root / path).resolve()
    try:
        resolved.relative_to(resolved_root)
    except ValueError as exc:
        raise GateError(f"evidence path escapes artifact root: {relative}") from exc
    return resolved


def validate_v2_policy(document: dict[str, Any]) -> None:
    policy = document.get("policy")
    expected_flags = {
        "bridge_is_supporting_evidence_only",
        "native_behavior_required_for_pass",
        "retail_assets_remain_external",
        "retail_semantics_qualified_required",
    }
    expected_keys = expected_flags | {"retail_gate_requires"}
    if not isinstance(policy, dict) or set(policy) != expected_keys:
        raise GateError("policy coverage")
    for flag in expected_flags:
        if policy.get(flag) is not True:
            raise GateError(f"policy.{flag}")
    requirements = policy.get("retail_gate_requires")
    if not isinstance(requirements, list) or tuple(requirements) != V2_RETAIL_GATE_REQUIREMENTS:
        raise GateError("policy.retail_gate_requires")


def validate_evidence(
    requirement: str,
    evidence: Any,
    artifact_root: Path,
) -> set[str]:
    if not isinstance(evidence, list):
        raise GateError(f"{requirement}.evidence must be a list")
    kinds: set[str] = set()
    identities: set[tuple[str, str]] = set()
    for index, item in enumerate(evidence):
        if not isinstance(item, dict):
            raise GateError(f"{requirement}.evidence[{index}] must be an object")
        kind = item.get("kind")
        if kind not in EVIDENCE_KINDS:
            raise GateError(f"{requirement}.evidence[{index}] has invalid kind")
        relative_path = item.get("path")
        digest = item.get("sha256")
        claim = item.get("claim")
        if not isinstance(relative_path, str) or not relative_path:
            raise GateError(f"{requirement}.evidence[{index}] missing path")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise GateError(f"{requirement}.evidence[{index}] invalid sha256")
        if not isinstance(claim, str) or not claim:
            raise GateError(f"{requirement}.evidence[{index}] missing claim")
        identity = (kind, relative_path)
        if identity in identities:
            raise GateError(f"{requirement} duplicates evidence {kind}:{relative_path}")
        identities.add(identity)
        path = safe_artifact_path(artifact_root, relative_path)
        try:
            actual_size = path.stat().st_size
        except OSError as exc:
            raise GateError(f"{requirement} evidence missing: {relative_path}") from exc
        expected_size = item.get("size")
        if expected_size is not None and (
            type(expected_size) is not int or expected_size < 0 or expected_size != actual_size
        ):
            raise GateError(f"{requirement} evidence size mismatch: {relative_path}")
        actual_digest = sha256_path(path)
        if actual_digest != digest:
            raise GateError(
                f"{requirement} evidence hash mismatch: {relative_path} "
                f"{actual_digest} != {digest}"
            )
        kinds.add(kind)
    return kinds


def _audit_v1(document: dict[str, Any], artifact_root: Path) -> dict[str, Any]:
    if document.get("schema") != V1_SCHEMA:
        raise GateError("schema")
    provenance = document.get("provenance")
    if not isinstance(provenance, dict):
        raise GateError("provenance")
    commit = provenance.get("repo_commit")
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise GateError("provenance.repo_commit")
    if provenance.get("xex_sha256") != CANONICAL_XEX_SHA256:
        raise GateError("provenance.xex_sha256")
    if provenance.get("data_tbl_sha256") != CANONICAL_DATA_TBL_SHA256:
        raise GateError("provenance.data_tbl_sha256")
    if provenance.get("mission_id") != 1:
        raise GateError("provenance.mission_id")
    requirements = document.get("requirements")
    if not isinstance(requirements, dict):
        raise GateError("requirements")
    expected = set(J0_REQUIREMENTS) | set(J1_REQUIREMENTS)
    if set(requirements) != expected:
        missing = sorted(expected - set(requirements))
        extra = sorted(set(requirements) - expected)
        raise GateError(f"requirements coverage missing={missing} extra={extra}")

    statuses: dict[str, str] = {}
    evidence_kinds: dict[str, list[str]] = {}
    for name in (*J0_REQUIREMENTS, *J1_REQUIREMENTS):
        record = requirements[name]
        if not isinstance(record, dict):
            raise GateError(f"{name} must be an object")
        status = record.get("status")
        if status not in {"open", "passed"}:
            raise GateError(f"{name}.status")
        statement = record.get("statement")
        if not isinstance(statement, str) or not statement:
            raise GateError(f"{name}.statement")
        kinds = validate_evidence(name, record.get("evidence"), artifact_root)
        if status == "open" and kinds:
            # Partial evidence belongs in the ledger, but the gate document should
            # not pretend an open requirement has a stable acceptance bundle.
            raise GateError(f"{name} is open but carries acceptance evidence")
        if status == "passed":
            if not kinds:
                raise GateError(f"{name} passed without evidence")
            if kinds <= {"bridge", "retail"}:
                raise GateError(f"{name} passed using oracle-only evidence")
            if name in REQUIRES_NATIVE_CAPTURE and "native-capture" not in kinds:
                raise GateError(f"{name} requires native-capture evidence")
            if name in REQUIRES_NATIVE_TEST and "native-test" not in kinds:
                raise GateError(f"{name} requires native-test evidence")
            if name in REQUIRES_RETAIL_SEMANTIC_EVIDENCE and not kinds.intersection(
                {"static", "microexec", "differential"}
            ):
                raise GateError(f"{name} requires retail semantic evidence")
        statuses[name] = status
        evidence_kinds[name] = sorted(kinds)

    j0_passed = all(statuses[name] == "passed" for name in J0_REQUIREMENTS)
    j1_local_passed = all(statuses[name] == "passed" for name in J1_REQUIREMENTS)
    j1_passed = j0_passed and j1_local_passed
    return {
        "schema": "ac6.mission01-native-gate-audit.v1",
        "repo_commit": commit,
        "J0": {
            "passed": j0_passed,
            "open": [name for name in J0_REQUIREMENTS if statuses[name] != "passed"],
        },
        "J1": {
            "passed": j1_passed,
            "open": [name for name in J1_REQUIREMENTS if statuses[name] != "passed"],
            "blocked_by_J0": not j0_passed,
        },
        "evidence_kinds": evidence_kinds,
        "retail_semantics_qualified": False,
    }


def _audit_v2(document: dict[str, Any], artifact_root: Path) -> dict[str, Any]:
    if document.get("schema") != SCHEMA:
        raise GateError("schema")
    provenance = document.get("provenance")
    if not isinstance(provenance, dict):
        raise GateError("provenance")
    if not re.fullmatch(r"[0-9a-f]{40}", str(provenance.get("repo_commit", ""))):
        raise GateError("provenance.repo_commit")
    if provenance.get("xex_sha256") != CANONICAL_XEX_SHA256:
        raise GateError("provenance.xex_sha256")
    if provenance.get("data_tbl_sha256") != CANONICAL_DATA_TBL_SHA256:
        raise GateError("provenance.data_tbl_sha256")
    if provenance.get("mission_id") != 1:
        raise GateError("provenance.mission_id")
    validate_v2_policy(document)

    requirements = document.get("requirements")
    if not isinstance(requirements, dict) or set(requirements) != {"J0", "domains", "runtime"}:
        raise GateError("requirements coverage")
    j0 = requirements.get("J0")
    domains = requirements.get("domains")
    runtime = requirements.get("runtime")
    if not isinstance(j0, dict) or set(j0) != set(J0_REQUIREMENTS):
        raise GateError("requirements.J0 coverage")
    if not isinstance(domains, dict) or set(domains) != set(V2_DOMAINS):
        raise GateError("requirements.domains coverage")
    if not isinstance(runtime, dict) or set(runtime) != {"pause_save_restart"}:
        raise GateError("requirements.runtime coverage")

    records = (
        [(name, j0[name]) for name in J0_REQUIREMENTS]
        + [(name, domains[name]) for name in V2_DOMAINS]
        + [("pause_save_restart", runtime["pause_save_restart"])]
    )
    statuses: dict[str, str] = {}
    kinds_by_name: dict[str, list[str]] = {}
    for name, record in records:
        if not isinstance(record, dict) or record.get("status") not in {"open", "passed"}:
            raise GateError(f"{name}.status")
        if not isinstance(record.get("statement"), str) or not record["statement"]:
            raise GateError(f"{name}.statement")
        kinds = validate_evidence(name, record.get("evidence"), artifact_root)
        if record["status"] == "open":
            if kinds:
                raise GateError(f"{name} is open but carries acceptance evidence")
            if name in V2_RETAIL_DOMAINS and record.get("retail_semantics_qualified") is True:
                raise GateError(f"{name} is open but marked retail-qualified")
        else:
            if not kinds:
                raise GateError(f"{name} passed without evidence")
            if name in REQUIRES_NATIVE_CAPTURE and "native-capture" not in kinds:
                raise GateError(f"{name} requires native-capture evidence")
            if name in set(J0_REQUIREMENTS) - REQUIRES_NATIVE_CAPTURE and "native-test" not in kinds:
                raise GateError(f"{name} requires native-test evidence")
            if name in V2_NATIVE_DOMAINS and "native-test" not in kinds:
                raise GateError(f"{name} requires native-test evidence")
            if name in {"essential_hud", "success_failure_debrief"} and "native-capture" not in kinds:
                raise GateError(f"{name} requires native-capture evidence")
            if name == "pause_save_restart" and "native-test" not in kinds:
                raise GateError(f"{name} requires native-test evidence")
            if name in V2_RETAIL_DOMAINS:
                if record.get("retail_semantics_qualified") is not True:
                    raise GateError(f"{name} lacks retail semantic qualification")
                if not kinds.intersection(V2_RETAIL_EVIDENCE_KINDS):
                    raise GateError(f"{name} requires retail evidence")
                if "native-test" not in kinds:
                    raise GateError(f"{name} requires native consumption evidence")
        statuses[name] = record["status"]
        kinds_by_name[name] = sorted(kinds)

    j0_open = [name for name in J0_REQUIREMENTS if statuses[name] != "passed"]
    j0_passed = not j0_open
    retail_semantics_qualified = all(
        statuses[name] == "passed"
        and domains[name].get("retail_semantics_qualified") is True
        for name in V2_RETAIL_DOMAINS
    )
    required_domains = V2_RETAIL_DOMAINS | V2_NATIVE_DOMAINS | {"pause_save_restart"}
    retail_open = [
        name
        for name in (*V2_DOMAINS, "pause_save_restart")
        if name in required_domains and statuses[name] != "passed"
    ]
    retail_passed = j0_passed and not retail_open and retail_semantics_qualified
    return {
        "schema": "ac6.mission01-native-gate-audit.v2",
        "repo_commit": provenance["repo_commit"],
        "J0": {"passed": j0_passed, "open": j0_open},
        "retail": {
            "passed": retail_passed,
            "open": retail_open,
            "blocked_by_J0": not j0_passed,
        },
        "evidence_kinds": kinds_by_name,
        "retail_semantics_qualified": retail_semantics_qualified,
    }


def _validate_derivation(name: str, item: dict[str, Any], path: Path,
                         addresses: list[str]) -> None:
    """A derivation must name its origin inside the file it points at.

    Declaring provenance in the contract proves nothing: the contract is the
    thing being audited. So the native source itself has to carry the retail
    address, and it must not be generated code.
    """
    relative = item["path"]
    for marker in FORBIDDEN_DERIVATION_MARKERS:
        if marker in relative:
            raise GateError(
                f"{name} derivation cites generated or recompiled source: {relative}")
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise GateError(f"{name} derivation unreadable: {relative}") from exc
    lowered = text.lower()
    for address in addresses:
        bare = address.lower().removeprefix("0x")
        if bare not in lowered:
            raise GateError(
                f"{name} derivation {relative} never cites retail address {address}")


def _audit_v3(document: dict[str, Any], artifact_root: Path) -> dict[str, Any]:
    if document.get("schema") != V3_SCHEMA:
        raise GateError("schema")
    provenance = document.get("provenance")
    if not isinstance(provenance, dict):
        raise GateError("provenance")
    if not re.fullmatch(r"[0-9a-f]{40}", str(provenance.get("repo_commit", ""))):
        raise GateError("provenance.repo_commit")
    if provenance.get("xex_sha256") != CANONICAL_XEX_SHA256:
        raise GateError("provenance.xex_sha256")
    if provenance.get("mission_id") != 1:
        raise GateError("provenance.mission_id")

    policy = document.get("policy")
    expected = {"derivation_required_for_pass", "generated_recomp_is_cross_match_only",
                "no_forced_objective_completion", "visual_parity_out_of_scope"}
    if not isinstance(policy, dict) or not expected <= set(policy):
        raise GateError("policy coverage")
    for flag in expected:
        if policy.get(flag) is not True:
            raise GateError(f"policy.{flag}")

    behaviours = document.get("requirements", {}).get("behaviours")
    if not isinstance(behaviours, dict) or not behaviours:
        raise GateError("requirements.behaviours")

    statuses: dict[str, str] = {}
    kinds_by_name: dict[str, list[str]] = {}
    for name, record in behaviours.items():
        if not isinstance(record, dict) or record.get("status") not in {"open", "passed"}:
            raise GateError(f"{name}.status")
        if not isinstance(record.get("statement"), str) or not record["statement"]:
            raise GateError(f"{name}.statement")
        addresses = record.get("retail_addresses")
        if not isinstance(addresses, list) or not addresses:
            raise GateError(f"{name}.retail_addresses")

        kinds = validate_evidence(name, record.get("evidence"), artifact_root)
        if record["status"] == "open":
            if kinds:
                raise GateError(f"{name} is open but carries acceptance evidence")
        else:
            missing = V3_REQUIRED_KINDS - kinds
            if missing:
                raise GateError(f"{name} passed without {sorted(missing)}")
            for item in record["evidence"]:
                if item.get("kind") != "derivation":
                    continue
                _validate_derivation(name, item,
                                     safe_artifact_path(artifact_root, item["path"]),
                                     addresses)
        statuses[name] = record["status"]
        kinds_by_name[name] = sorted(kinds)

    open_names = [name for name, status in statuses.items() if status != "passed"]
    return {
        "schema": "ac6.mission01-final-gate-audit.v3",
        "repo_commit": provenance["repo_commit"],
        "JF": {"passed": not open_names, "open": open_names},
        "behaviours": statuses,
        "evidence_kinds": kinds_by_name,
    }


def audit_contract(document: dict[str, Any], artifact_root: Path) -> dict[str, Any]:
    schema = document.get("schema")
    if schema == V1_SCHEMA:
        return _audit_v1(document, artifact_root)
    if schema == V3_SCHEMA:
        return _audit_v3(document, artifact_root)
    return _audit_v2(document, artifact_root)


def fail(message: str) -> int:
    print(f"mission01_native_gate=fail reason={message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contract", type=Path, nargs="?")
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--require", choices=("J0", "J1", "retail", "JF"))
    parser.add_argument("--init", type=Path, help="write a new fail-closed template")
    args = parser.parse_args()
    if args.init is not None:
        if args.contract is not None:
            parser.error("contract and --init are mutually exclusive")
        if args.init.exists():
            return fail(f"refusing to replace existing template: {args.init}")
        args.init.parent.mkdir(parents=True, exist_ok=True)
        args.init.write_text(json.dumps(template(), indent=2) + "\n", encoding="utf-8")
        print(f"mission01_native_gate=initialized path={args.init}")
        return 0
    if args.contract is None:
        parser.error("contract is required unless --init is used")
    try:
        document = json.loads(args.contract.read_text(encoding="utf-8"))
        report = audit_contract(document, args.artifact_root)
    except (OSError, json.JSONDecodeError, GateError) as exc:
        return fail(str(exc))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    if "JF" in report:
        state = "pass" if report["JF"]["passed"] else "open"
        print(f"mission01_final_gate=audit-valid JF={state} "
              f"open={','.join(report['JF']['open']) or 'none'}")
        if args.require == "JF" and not report["JF"]["passed"]:
            return 2
        return 0
    retail_report = report.get("J1", report.get("retail", {"passed": False}))
    print(
        "mission01_native_gate=audit-valid "
        f"J0={'pass' if report['J0']['passed'] else 'open'} "
        f"J1={'pass' if retail_report['passed'] else 'open'}"
    )
    required_key = "J0" if args.require == "J0" else (
        "J1" if args.require == "J1" and "J1" in report else
        ("retail" if args.require == "J1" else None)
    )
    if args.require == "retail":
        required_key = "retail"
    if required_key and not report[required_key]["passed"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
