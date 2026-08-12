#!/usr/bin/env python3
"""Fail-closed structural audit for the global offline mission ladder."""

from __future__ import annotations

import csv
import hashlib
import json
import re
import sys
from pathlib import Path

from audit_ac6_global_checkpoint2 import audit_document as audit_checkpoint2


ROOT = Path(__file__).resolve().parents[1]
MATRIX = ROOT / "analysis/mission-capability-matrix.tsv"
TEMPLATE = ROOT / "analysis/templates/mission-gate-template.json"
LADDER = ROOT / "GLOBAL_OFFLINE_LADDER.md"
SPINE = ROOT / "analysis/mission01-execution-spine.json"
REXGLUE_TRUST = ROOT / "analysis/rexglue-semantic-trust-v1.json"
CHECKPOINT2 = ROOT / "analysis/contracts/global-checkpoint-2-v1.json"
GATES = ("JF", "JV", "JP", "JG")
STATES = {"open", "passed"}
XEX = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
NTSC_UJ_XEX = "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc"
NTSC_UJ_XXH3 = "892639b654015428"
SPINE_PHASES = (
    "M01-A-load", "M01-B-controlled-sortie", "M01-C-first-objective",
    "M01-D-debrief", "M01-E-replay", "M01-F-parity",
)
REXGLUE_TRUST_STATES = {
    "provisional-rexglue", "retail-qualified", "divergent",
}
REXGLUE_DIVERGENCES = {
    "cpu-dcbst-emitted-as-noop",
    "cpu-reciprocal-estimates-exact-host-math",
    "cpu-reservation-without-address-or-granule",
    "cpu-barriers-emitted-as-noop",
    "cpu-vmsum4fp128-summation-order",
    "cpu-hle-u64-arguments-truncated",
    "cpu-hle-mixed-fp-integer-slotting",
    "cpu-vscr-sat-not-updated",
    "cpu-fpscr-status-incomplete",
    "cpu-pal-oracle-fma-unfused",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def project_path(relative: object) -> Path:
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise ValueError("spine evidence path")
    candidate = (ROOT / relative).resolve()
    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError as error:
        raise ValueError("spine evidence escapes project") from error
    return candidate


def audit_rexglue_trust(path: Path) -> None:
    trust = json.loads(path.read_text(encoding="utf-8"))
    if trust.get("schema") != "ac6.rexglue-semantic-trust.v1":
        raise ValueError("RexGlue semantic trust schema")
    target = trust.get("target", {})
    if (target.get("platform") != "Xbox 360 PAL" or
            target.get("module") != "default.xex" or
            target.get("xex_sha256") != XEX):
        raise ValueError("RexGlue semantic trust target")
    if set(trust.get("states", [])) != REXGLUE_TRUST_STATES:
        raise ValueError("RexGlue semantic trust states")
    policy = trust.get("policy", {})
    required_true = (
        "provisional_allows_native_implementation",
        "provisional_allows_integration_tests",
        "provisional_allows_diagnostic_replay",
        "known_divergence_is_never_provisional",
        "provisional_is_revision_scoped",
        "revision_changes_require_semantic_diff",
        "shared_reader_changes_require_15_mission_corpus",
        "promotion_requires_pal_identity",
        "promotion_requires_bounded_retail_evidence",
        "promotion_requires_native_regression_test",
    )
    required_false = (
        "provisional_allows_lane_closure",
        "provisional_allows_publication",
        "product_may_link_or_embed_oracle",
    )
    if any(policy.get(key) is not True for key in required_true):
        raise ValueError("RexGlue semantic trust positive policy")
    if any(policy.get(key) is not False for key in required_false):
        raise ValueError("RexGlue semantic trust negative policy")
    if policy.get("default_for_reached_implemented_rexglue_semantics") != \
            "provisional-rexglue":
        raise ValueError("RexGlue semantic trust default")
    pal_oracle = trust.get("source_pins", {}).get("pal_oracle", {})
    manifest_path = project_path(pal_oracle.get("manifest"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if (pal_oracle.get("ac6_recomp_commit") != manifest.get("oracle", {}).get("commit") or
            pal_oracle.get("rexglue_tree") != manifest.get("sdk", {}).get("tree_sha1") or
            pal_oracle.get("runtime_config_sha256") !=
            manifest.get("configuration", {}).get("sha256") or
            pal_oracle.get("oracle_binary_sha256") !=
            manifest.get("runtime_build", {}).get("binary_sha256")):
        raise ValueError("RexGlue PAL revision identity")
    modern = trust.get("source_pins", {}).get("modern_us_reference", {})
    modern_identity_path = project_path(modern.get("identity"))
    modern_identity = json.loads(modern_identity_path.read_text(encoding="utf-8"))
    modern_target = modern_identity.get("target", {})
    modern_oracle = modern_identity.get("oracle_reference", {})
    modern_codegen = modern_identity.get("codegen", {})
    modern_linux = modern_identity.get("linux_retest", {})
    modern_xenia = modern_identity.get("xenia_identity_cross_check", {})
    if (modern_identity.get("schema") != "ac6.recomp-oracle-identity.v1" or
            modern_identity.get("source_media", {}).get("retail_bytes_committed") is not False or
            modern_target.get("target_id") != "ac6-ntsc-uj-default-xex" or
            modern_target.get("module") != "default.xex" or
            modern_target.get("sha256") != NTSC_UJ_XEX or
            modern_target.get("module_xxh3") != NTSC_UJ_XXH3 or
            modern_target.get("title_id") != "4E4D07D1" or
            modern_target.get("media_id") != "531C30BE" or
            modern_target.get("xex_version") != "v0.0.0.8" or
            modern_target.get("base_version") != "v0.0.0.8"):
        raise ValueError("RexGlue NTSC-U/J target identity")
    if (modern.get("ac6_recomp_commit") != modern_oracle.get("commit") or
            modern.get("rexglue_tree") != modern_oracle.get("rexglue_tree") or
            modern.get("runtime_config_sha256") !=
            modern_oracle.get("runtime_config_sha256") or
            modern.get("target_region") != "NTSC-U/J family" or
            modern.get("module") != modern_target.get("module") or
            modern.get("xex_sha256") != modern_target.get("sha256") or
            modern.get("module_xxh3") != modern_target.get("module_xxh3") or
            modern.get("title_id") != modern_target.get("title_id") or
            modern.get("media_id") != modern_target.get("media_id") or
            modern.get("xex_version") != modern_target.get("xex_version") or
            modern.get("base_version") != modern_target.get("base_version") or
            modern.get("codegen_tree_sha256") !=
            modern_codegen.get("generated_tree_sha256")):
        raise ValueError("RexGlue NTSC-U/J revision identity")
    marker = modern_identity.get("controller_seams", {}).get("frame_input_stage", {})
    xam_thunk = modern_identity.get("controller_seams", {}).get("xam_input_get_state_thunk", {})
    if (modern_codegen.get("status") != "generated-with-audit-only-link-order-patch" or
            modern_codegen.get("identical_runs") != 2 or
            modern_codegen.get("generated_output_committed") is not False or
            modern_codegen.get("generated_output_is_product") is not False or
            modern_linux.get("full_runtime_built") is not False or
            modern_linux.get("runtime_launched") is not False or
            modern_xenia.get("gameplay_evidence") is not False or
            modern_xenia.get("parity_evidence") is not False or
            marker.get("address") != "0x821CA940" or
            marker.get("length") != 328 or
            marker.get("code_sha256") !=
            "a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d" or
            marker.get("runtime_cadence_qualified") is not False or
            xam_thunk.get("address") != "0x82390CE0" or
            xam_thunk.get("code_sha256") !=
            "9fcb5cb10e9f71b1b6eaf6f8c1854155506dee6702d9b6423888f72622d032bb" or
            modern.get("codegen_status") != "provisional-rexglue" or
            modern.get("linux_runtime_built") is not False or
            modern.get("pal_gate_evidence") is not False):
        raise ValueError("RexGlue NTSC-U/J provisional boundary")
    divergences = trust.get("known_divergences")
    if not isinstance(divergences, list) or {
            item.get("id") for item in divergences if isinstance(item, dict)
    } != REXGLUE_DIVERGENCES:
        raise ValueError("RexGlue known divergence coverage")
    for item in divergences:
        if (item.get("status") != "divergent" or item.get("gate_evidence") is not False or
                not item.get("semantics") or not item.get("evidence")):
            raise ValueError(f"RexGlue divergence contract: {item.get('id')}")
    revision_scope = trust.get("revision_scope", {})
    if (revision_scope.get("unreviewed_revision_inherits") != "nothing" or
            revision_scope.get("semantic_diff_required_before_reuse") is not True or
            set(revision_scope.get("required_identity_fields", [])) != {
                "ac6_recomp_commit", "rexglue_tree", "runtime_config_sha256",
                "oracle_binary_sha256",
            }):
        raise ValueError("RexGlue revision scope")
    promotion = trust.get("promotion", {})
    if (promotion.get("from") != "provisional-rexglue" or
            promotion.get("to") != "retail-qualified" or
            not promotion.get("required")):
        raise ValueError("RexGlue semantic promotion contract")


def audit_spine(m01_row: dict[str, str]) -> None:
    spine = json.loads(SPINE.read_text(encoding="utf-8"))
    if spine.get("schema") != "ac6.mission01-execution-spine.v1" or spine.get("mission") != "M01":
        raise ValueError("Mission 01 spine identity")
    target = spine.get("target", {})
    if (target.get("platform") != "Xbox 360 PAL" or target.get("module") != "default.xex" or
            target.get("xex_sha256") != XEX or target.get("simulation_hz") != 60):
        raise ValueError("Mission 01 spine target")
    policy = spine.get("focus_policy", {})
    for key in ("defer_other_missions", "shared_reader_changes_require_15_mission_corpus",
                "new_report_requires_durable_test_contract_or_invariant",
                "product_must_not_depend_on_oracle", "rexglue_provisional_bringup_allowed",
                "known_divergences_fail_closed"):
        if policy.get(key) is not True:
            raise ValueError(f"Mission 01 spine policy: {key}")
    if policy.get("provisional_evidence_closes_gates") is not False:
        raise ValueError("Mission 01 provisional gate policy")

    oracle = spine.get("oracle", {})
    oracle_manifest_path = project_path(oracle.get("manifest"))
    oracle_manifest = json.loads(oracle_manifest_path.read_text(encoding="utf-8"))
    if (oracle_manifest.get("oracle", {}).get("commit") !=
            "dcd41b7457fcac8242f8ef40de83d1719390d5af" or
            oracle_manifest.get("target", {}).get("sha256") != XEX):
        raise ValueError("Mission 01 spine oracle identity")
    trust_path = project_path(oracle.get("semantic_trust_registry"))
    if trust_path != REXGLUE_TRUST.resolve():
        raise ValueError("Mission 01 RexGlue semantic trust path")
    audit_rexglue_trust(trust_path)
    frontier = oracle.get("current_runtime_frontier", {})
    if (not re.fullmatch(r"0x[0-9A-F]{8}", str(frontier.get("source"))) or
            not re.fullmatch(r"0x[0-9A-F]{8}", str(frontier.get("target"))) or
            frontier.get("gate_evidence") is not False):
        raise ValueError("Mission 01 spine runtime frontier")

    lanes = spine.get("lanes")
    if not isinstance(lanes, list) or [lane.get("id") for lane in lanes] != [
            "simulation", "renderer", "platform"]:
        raise ValueError("Mission 01 spine lanes")
    if any(not isinstance(lane.get("exit"), str) or not lane["exit"] for lane in lanes):
        raise ValueError("Mission 01 spine lane exit")

    phases = spine.get("phases")
    if not isinstance(phases, list) or tuple(phase.get("id") for phase in phases) != SPINE_PHASES:
        raise ValueError("Mission 01 spine phase order")
    status_by_id: dict[str, str] = {}
    passed_gates: set[str] = set()
    for index, phase in enumerate(phases):
        phase_id = phase["id"]
        status = phase.get("status")
        expected_requires = [] if index == 0 else [SPINE_PHASES[index - 1]]
        if status not in STATES or phase.get("requires") != expected_requires:
            raise ValueError(f"Mission 01 spine phase contract: {phase_id}")
        gates = phase.get("product_gates")
        if (not isinstance(gates, list) or not gates or len(gates) != len(set(gates)) or
                any(gate not in GATES for gate in gates)):
            raise ValueError(f"Mission 01 spine product gates: {phase_id}")
        if not isinstance(phase.get("closure"), str) or not phase["closure"]:
            raise ValueError(f"Mission 01 spine closure: {phase_id}")
        evidence = phase.get("evidence")
        if not isinstance(evidence, list):
            raise ValueError(f"Mission 01 spine evidence: {phase_id}")
        if status == "passed":
            if not evidence or any(status_by_id[required] != "passed" for required in expected_requires):
                raise ValueError(f"Mission 01 spine premature pass: {phase_id}")
            if (phase.get("oracle_capture_required") is True and
                    oracle_manifest.get("capture_status") !=
                    oracle.get("required_capture_status_for_runtime_gates")):
                raise ValueError(f"Mission 01 spine unqualified oracle pass: {phase_id}")
            passed_gates.update(gates)
        else:
            blockers = phase.get("blockers")
            if not isinstance(blockers, list) or not blockers or any(
                    not isinstance(blocker, str) or not blocker for blocker in blockers):
                raise ValueError(f"Mission 01 spine open blockers: {phase_id}")
        for item in evidence:
            if not isinstance(item, dict) or set(item) != {"path", "sha256", "size"}:
                raise ValueError(f"Mission 01 spine evidence shape: {phase_id}")
            path = project_path(item["path"])
            if (not path.is_file() or path.stat().st_size != item["size"] or
                    sha256(path) != item["sha256"]):
                raise ValueError(f"Mission 01 spine evidence identity: {phase_id}")
        status_by_id[phase_id] = status

    matrix_passed = {gate for gate in GATES if m01_row[gate] == "passed"}
    if not matrix_passed.issubset(passed_gates):
        raise ValueError("Mission 01 matrix outruns execution spine")
    all_phases_passed = all(status_by_id[phase] == "passed" for phase in SPINE_PHASES)
    if (m01_row.get("supported") == "yes") != all_phases_passed:
        raise ValueError("Mission 01 support disagrees with execution spine")


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
    audit_spine(rows[0])
    audit_checkpoint2(json.loads(CHECKPOINT2.read_text(encoding="utf-8")))
    ladder = LADDER.read_text(encoding="utf-8")
    for checkpoint in range(8):
        if f"{checkpoint} —" not in ladder:
            raise ValueError(f"missing checkpoint {checkpoint}")
    print("global_ladder=pass missions=15 gates=4 checkpoints=8 m01_spine=6 "
          "rexglue_trust=pass")


if __name__ == "__main__":
    try:
        audit()
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"global_ladder=fail error={error}", file=sys.stderr)
        raise SystemExit(1) from error
