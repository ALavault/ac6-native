#!/usr/bin/env python3
"""Validate the fail-closed NTSC-U/J 15-mission release manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
WORKSPACE = PRODUCT.parents[1]
SHA256 = re.compile(r"^[0-9a-f]{64}$")


class CampaignValidationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CampaignValidationError(message)


def load_json(path: Path) -> dict[str, object]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CampaignValidationError(f"cannot read {path}: {error}") from error
    require(isinstance(document, dict), f"{path} is not a JSON object")
    return document


def workspace_path(value: object, label: str) -> Path:
    require(isinstance(value, str) and value != "", f"{label} path is missing")
    relative = Path(value)
    require(not relative.is_absolute() and ".." not in relative.parts,
            f"{label} path is not workspace-relative")
    resolved = (WORKSPACE / relative).resolve()
    try:
        resolved.relative_to(WORKSPACE.resolve())
    except ValueError as error:
        raise CampaignValidationError(f"{label} escapes the workspace") from error
    require(resolved.is_file(), f"{label} does not exist: {relative}")
    return resolved


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_identity(
    document: dict[str, object], target: dict[str, object], runtime: str = "rexglue-oracle"
) -> None:
    require(document.get("schema") == "ac6.retail-campaign-manifest.v1",
            "campaign schema mismatch")
    require(document.get("target") == "ntsc-uj" and document.get("region") == "NTSC-U/J",
            "campaign target identity mismatch")
    identity = document.get("identity")
    require(isinstance(identity, dict), "campaign identity is missing")
    target_xex = target.get("xex")
    target_iso = target.get("iso")
    target_ghidra = target.get("ghidra")
    require(isinstance(target_xex, dict) and isinstance(target_iso, dict)
            and isinstance(target_ghidra, dict), "target definition identity is incomplete")
    require(identity.get("xex_sha256") == target_xex.get("sha256"),
            "campaign XEX identity mismatch")
    require(identity.get("iso_sha256") == target_iso.get("sha256"),
            "campaign ISO identity mismatch")
    require(identity.get("ghidra_project") == target_ghidra.get("project")
            and identity.get("program") == target_ghidra.get("program")
            and identity.get("language") == target_ghidra.get("language"),
            "campaign Ghidra identity mismatch")
    require(SHA256.fullmatch(str(document.get("binary_sha256", ""))) is not None,
            "campaign binary identity is invalid")

    renderer = document.get("renderer")
    authority = "native Xenos/Vulkan" if runtime == "native" else "ReXGlue Xenos/Vulkan"
    require(renderer == {
        "authority": authority,
        "vulkan": True,
        "d3d12": False,
        "resolution": "1280x720",
        "scale": 1,
        "fps": 30,
        "enhancements": False,
    }, "campaign renderer contract mismatch")


def validate_static_evidence(document: dict[str, object]) -> None:
    static = document.get("static_evidence")
    require(isinstance(static, dict), "static evidence is missing")
    identity = document.get("identity")
    require(isinstance(identity, dict), "campaign identity is missing")
    expected_xex = identity.get("xex_sha256")
    expected_iso = identity.get("iso_sha256")
    selector = static.get("selector_to_dpl")
    require(isinstance(selector, dict) and selector.get("status") == "qualified",
            "selector-to-DPL evidence is not qualified")
    require(selector.get("function_address") == "0x821B6EE8"
            and selector.get("table_address") == "0x820657B0"
            and selector.get("mission_count") == 15
            and selector.get("mapping_rule") ==
            "dpl_resource_id = campaign_selector + 8 for selectors 1..15",
            "selector-to-DPL contract mismatch")
    selector_log = workspace_path(selector.get("evidence"), "selector evidence")
    selector_text = selector_log.read_text(encoding="utf-8", errors="replace")
    require("sha256=6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc"
            in selector_text, "selector evidence has the wrong XEX")
    require("FUNCTION 821b6ee8" in selector_text and "UNK_820657b0" in selector_text,
            "selector evidence does not contain the qualified consumer/table")

    route = static.get("dpl_to_data_table")
    require(isinstance(route, dict), "DPL-to-DATA.TBL evidence is missing")
    require(route.get("status") in {"needs-canonical-queue-boundary", "qualified"},
            "unknown DPL-to-DATA.TBL status")
    require(route.get("request_function_address") == "0x821D1190"
            and route.get("queue_call_target") == "0x821CD168"
            and route.get("loader_function_address") == "0x821CC288"
            and route.get("direct_id_exclusive") == "0x39D",
            "DPL-to-DATA.TBL contract mismatch")
    route_log = workspace_path(route.get("evidence"), "DPL route evidence")
    route_text = route_log.read_text(encoding="utf-8", errors="replace")
    require("FUNCTION 821d1190" in route_text and "uVar4 < 0x39d" in route_text
            and "func_0x821cd168" in route_text and "FUNCTION 821cc288" in route_text,
            "DPL route evidence is incomplete")
    if route.get("status") == "needs-canonical-queue-boundary":
        require(route.get("qualification_gaps") == [
            "canonical_function_boundary_at_0x821CD168",
            "qualified_us_data_tbl_identity",
        ], "DPL route gaps are not explicit")
    else:
        require("qualification_gaps" not in route,
                "qualified DPL route still carries qualification gaps")
        queue = route.get("queue_boundary")
        require(queue == {
            "kind": "direct-call-leaf-between-pdata-functions",
            "entry": "0x821CD168",
            "end_exclusive": "0x821CD2E8",
            "call_site": "0x821D121C",
            "return_addresses": ["0x821CD1BC", "0x821CD29C", "0x821CD2E0"],
            "previous_pdata_entry": "0x821CD0D0",
            "previous_pdata_extent": "0x94",
            "next_pdata_entry": "0x821CD2E8",
            "evidence": "artifacts/retail-us-campaign-leaf-static-20260827/leaf.log",
            "pdata_evidence": "artifacts/retail-us-campaign-pdata-static-20260827/pdata.log",
        }, "qualified queue leaf boundary mismatch")
        leaf_text = workspace_path(queue["evidence"], "queue leaf evidence").read_text(
            encoding="utf-8", errors="replace"
        )
        pdata_text = workspace_path(queue["pdata_evidence"], "queue pdata evidence").read_text(
            encoding="utf-8", errors="replace"
        )
        for fragment in (
            "821d121c -> 0x821cd168",
            "821cd1bc blr",
            "821cd29c blr",
            "821cd2e0 blr",
            "821cd2e4 <not-disassembled>",
        ):
            require(fragment in leaf_text, f"queue leaf evidence lacks {fragment}")
        require("sha256=6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc"
                in pdata_text, "queue pdata evidence has the wrong XEX")
        require("target=0x821CD168 record=8207f4e0 entry=0x821CD0D0 delta=0x98"
                in pdata_text and
                "target=0x821CD2E8 record=8207f4e8 entry=0x821CD2E8 delta=0x0"
                in pdata_text, "queue pdata evidence does not bound the direct-call leaf")

        loader = route.get("loader_contract")
        require(loader == {
            "path_address": "0x82067D30",
            "format_flag_address": "0x8293B938",
            "format_flag_value": 2,
            "format_flag_store": "0x821D6248",
            "loader_call_site": "0x821D624C",
            "entry_count_address": "0x8293B950",
            "record_base_offset": 8,
            "record_size": 16,
            "evidence": "artifacts/retail-us-campaign-loader-static-20260827/loader.log",
        }, "qualified DATA.TBL loader contract mismatch")
        loader_text = workspace_path(loader["evidence"], "DATA.TBL loader evidence").read_text(
            encoding="utf-8", errors="replace"
        )
        for fragment in (
            "sha256=6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc",
            "821d6244 li r11,0x2",
            "821d6248 stb r11,-0x46c8(r10)",
            "821d624c bl 0x821cc288",
            "821cc338 lwz r11,0x0(r31)",
            "821cc33c stw r11,-0x46b0(r10)",
            "73 69 6d 3a 44 41 54 41 2e 54 42 4c 00",
        ):
            require(fragment in loader_text, f"DATA.TBL loader evidence lacks {fragment}")

        table = route.get("data_table")
        require(isinstance(table, dict), "qualified DPL route lacks DATA.TBL metadata")
        require(table.get("sha256") ==
                "bad3a157eb75c839d9d6187f69ac061dee6d075d9cd61e0aa73b533473863b2f"
                and table.get("size") == 14824 and table.get("entry_count") == 926
                and table.get("pack_count") == 2 and table.get("record_size") == 16,
                "qualified US DATA.TBL contract mismatch")
        table_path = workspace_path(table.get("file"), "qualified US DATA.TBL")
        require(table_path.stat().st_size == table["size"]
                and sha256(table_path) == table["sha256"],
                "qualified US DATA.TBL file mismatch")
        table_bytes = table_path.read_bytes()
        entry_count, pack_count = struct.unpack_from(">II", table_bytes)
        require(entry_count == table["entry_count"] and pack_count == table["pack_count"]
                and len(table_bytes) == 8 + entry_count * table["record_size"],
                "qualified US DATA.TBL shape mismatch")
        extraction = load_json(workspace_path(
            table.get("extraction_receipt"), "qualified US DATA.TBL extraction receipt"
        ))
        source_iso = extraction.get("source_iso")
        extracted = extraction.get("file")
        require(extraction.get("schema") == "ac6.xdvdfs-bounded-extraction.v1"
                and extraction.get("status") == "qualified"
                and extraction.get("target") == "ntsc-uj"
                and isinstance(source_iso, dict)
                and source_iso.get("sha256") ==
                "204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c"
                and source_iso.get("size") == 7835492352
                and isinstance(extracted, dict)
                and extracted.get("internal_path") == "DATA.TBL"
                and extracted.get("size") == table["size"]
                and extracted.get("sha256") == table["sha256"]
                and extracted.get("output") == table["file"],
                "qualified US DATA.TBL extraction receipt mismatch")
        require(route.get("mapping_rule") ==
                "data_table_entry_index = dpl_resource_id for direct IDs below 0x39D",
                "qualified DPL-to-DATA.TBL mapping rule mismatch")

    payload_evidence = static.get("mission_payloads")
    require(isinstance(payload_evidence, dict)
            and payload_evidence.get("status") == "qualified-structural-only"
            and payload_evidence.get("mission_range") == [1, 15]
            and payload_evidence.get("policy") ==
            "bounded US ISO slices; no PAC copy; no objective/runtime semantics",
            "US mission payload static evidence is not qualified")
    payload_manifest = load_json(workspace_path(
        payload_evidence.get("evidence"), "US mission payload manifest"
    ))
    require(payload_manifest.get("schema") ==
            "ac6.retail-us-mission-payload-static.v1"
            and payload_manifest.get("status") == "qualified-structural-only"
            and payload_manifest.get("target") == "ntsc-uj"
            and payload_manifest.get("xex_sha256") == expected_xex,
            "US mission payload manifest identity mismatch")
    source_iso = payload_manifest.get("source_iso")
    require(isinstance(source_iso, dict) and source_iso.get("sha256") == expected_iso,
            "US mission payload manifest ISO mismatch")
    payload_records = payload_manifest.get("records")
    require(isinstance(payload_records, list) and len(payload_records) == 15,
            "US mission payload manifest cardinality mismatch")
    for mission_id, record in enumerate(payload_records, start=1):
        require(isinstance(record, dict)
                and record.get("mission_id") == mission_id
                and record.get("campaign_selector") == mission_id
                and record.get("dpl_resource_id") == mission_id + 8
                and record.get("data_table_entry_index") == mission_id + 8,
                f"US mission payload mapping mismatch: {mission_id}")
        structure = record.get("structure")
        require(isinstance(structure, dict)
                and structure.get("root") == "FHM"
                and structure.get("parse_failures") == 0,
                f"US mission payload structure is not qualified: {mission_id}")
    require(payload_manifest.get("policy") == {
        "bounded_iso_reads": True,
        "complete_pac_copied": False,
        "payloads_retained": False,
        "objective_semantics_inferred": False,
        "renderer_or_runtime_claimed": False,
    }, "US mission payload policy mismatch")

    scene_evidence = static.get("scene_tcam")
    require(isinstance(scene_evidence, dict)
            and scene_evidence.get("status") == "qualified-structural-only"
            and scene_evidence.get("mission_range") == [1, 15]
            and scene_evidence.get("totals") == {
                "scene_tables": 176, "scene_paths": 2950,
                "tcam_resources": 88,
            }, "US Scene/TCAM static evidence is not qualified")
    scene_manifest = load_json(workspace_path(
        scene_evidence.get("evidence"), "US Scene/TCAM manifest"
    ))
    require(scene_manifest.get("schema") == "ac6.retail-us-scene-tcam-static.v1"
            and scene_manifest.get("status") == "qualified-structural-only"
            and scene_manifest.get("target") == "ntsc-uj"
            and scene_manifest.get("xex_sha256") == expected_xex,
            "US Scene/TCAM manifest identity mismatch")
    scene_iso = scene_manifest.get("source_iso")
    require(isinstance(scene_iso, dict) and scene_iso.get("sha256") == expected_iso,
            "US Scene/TCAM manifest ISO mismatch")
    require(scene_manifest.get("mission_range") == [1, 15]
            and scene_manifest.get("totals") == {
                "mission_payloads": 15, "scene_tables": 176,
                "scene_paths": 2950, "tcam_resources": 88,
            }, "US Scene/TCAM totals mismatch")
    scene_missions = scene_manifest.get("missions")
    require(isinstance(scene_missions, list) and len(scene_missions) == 15,
            "US Scene/TCAM mission cardinality mismatch")
    for mission_id, record in enumerate(scene_missions, start=1):
        require(isinstance(record, dict)
                and record.get("mission_id") == mission_id
                and record.get("data_table_entry_index") == mission_id + 8,
                f"US Scene/TCAM mapping mismatch: {mission_id}")
    require(isinstance(scene_manifest.get("tcam_resources"), list)
            and len(scene_manifest["tcam_resources"]) == 88,
            "US Scene/TCAM resource cardinality mismatch")
    require(scene_manifest.get("policy") == {
        "bounded_iso_reads": True,
        "complete_pac_copied": False,
        "objective_semantics_inferred": False,
        "payloads_retained": False,
        "runtime_or_renderer_claimed": False,
    }, "US Scene/TCAM policy mismatch")

    scenario_evidence = static.get("scenario_payloads")
    require(isinstance(scenario_evidence, dict)
            and scenario_evidence.get("status") == "qualified-structural-only"
            and scenario_evidence.get("mission_range") == [1, 15]
            and scenario_evidence.get("totals") == {
                "roundtrip_pass": 15, "schema_validation_pass": 15,
                "obj_records": 6784, "order_records": 43287,
            }, "US scenario static evidence is not qualified")
    scenario_manifest = load_json(workspace_path(
        scenario_evidence.get("evidence"), "US scenario manifest"
    ))
    require(scenario_manifest.get("schema") == "ac6.retail-us-scenario-static.v1"
            and scenario_manifest.get("status") == "qualified-structural-only"
            and scenario_manifest.get("target") == "ntsc-uj"
            and scenario_manifest.get("xex_sha256") == expected_xex,
            "US scenario manifest identity mismatch")
    scenario_iso = scenario_manifest.get("source_iso")
    require(isinstance(scenario_iso, dict)
            and scenario_iso.get("sha256") == expected_iso,
            "US scenario manifest ISO mismatch")
    require(scenario_manifest.get("mission_range") == [1, 15]
            and scenario_manifest.get("totals") == {
                "mission_payloads": 15, "scenario_bytes": 42038864,
                "nodes": 758806, "tables": 351366,
                "obj_records": 6784, "order_records": 43287,
                "roundtrip_pass": 15, "schema_validation_pass": 15,
            }, "US scenario totals mismatch")
    scenario_missions = scenario_manifest.get("missions")
    require(isinstance(scenario_missions, list) and len(scenario_missions) == 15,
            "US scenario mission cardinality mismatch")
    for mission_id, record in enumerate(scenario_missions, start=1):
        require(isinstance(record, dict)
                and record.get("mission_id") == mission_id
                and record.get("data_table_entry_index") == mission_id + 8,
                f"US scenario mapping mismatch: {mission_id}")
        roundtrip = record.get("roundtrip")
        validators = record.get("validators")
        require(isinstance(roundtrip, dict)
                and roundtrip.get("identical") is True
                and roundtrip.get("unwritten_nonzero") == 0
                and isinstance(validators, dict)
                and all(
                    isinstance(result, dict) and result.get("inconsistencies") == 0
                    for result in validators.values()
                ), f"US scenario validation mismatch: {mission_id}")
    require(scenario_manifest.get("policy") == {
        "bounded_iso_reads": True,
        "complete_pac_copied": False,
        "objective_semantics_inferred": False,
        "payloads_retained": False,
        "runtime_or_renderer_claimed": False,
    }, "US scenario policy mismatch")


def validate_gameplay_receipts(
    mission: dict[str, object], identity: dict[str, object], binary_sha256: str,
    runtime: str = "rexglue-oracle",
) -> dict[str, object]:
    mission_id = mission["mission_id"]
    runtime = mission["runtime"]
    assert isinstance(runtime, dict)
    route_path = workspace_path(runtime.get("route"), f"mission {mission_id} route")
    gate = load_json(workspace_path(
        runtime.get("gate_receipt"), f"mission {mission_id} gameplay receipt"))
    audit = load_json(workspace_path(
        runtime.get("audit_receipt"), f"mission {mission_id} gameplay audit"))
    require(gate.get("schema") == "ac6.retail-gameplay-gate.v3"
            and gate.get("status") == "capture-ready", f"mission {mission_id} gate is not capture-ready")
    require(audit.get("schema") == "ac6.retail-gameplay-audit.v3"
            and audit.get("status") == "pass", f"mission {mission_id} audit is not pass")
    authority = "native Xenos/Vulkan" if runtime == "native" else "ReXGlue Xenos/Vulkan"
    for label, receipt in (("gate", gate), ("audit", audit)):
        require(receipt.get("target") == "ntsc-uj", f"mission {mission_id} {label} target mismatch")
        require(receipt.get("mission_id") == mission_id
                and receipt.get("execution_mode") == "strict-replay",
                f"mission {mission_id} {label} is not strict mission replay")
        manifest = receipt.get("manifest")
        require(isinstance(manifest, dict), f"mission {mission_id} {label} manifest missing")
        xex = manifest.get("xex")
        iso = manifest.get("iso")
        graphics = manifest.get("graphics")
        require(isinstance(xex, dict) and xex.get("sha256") == identity.get("xex_sha256"),
                f"mission {mission_id} {label} XEX mismatch")
        require(isinstance(iso, dict) and iso.get("sha256") == identity.get("iso_sha256"),
                f"mission {mission_id} {label} ISO mismatch")
        require(isinstance(graphics, dict) and graphics.get("vulkan") is True
                and graphics.get("d3d12") is False
                and graphics.get("authority") == authority,
                f"mission {mission_id} {label} renderer mismatch")
        binary = receipt.get("binary")
        require(isinstance(binary, dict) and binary.get("sha256") == binary_sha256,
                f"mission {mission_id} {label} binary mismatch")
        route = receipt.get("route")
        require(isinstance(route, dict) and route.get("executed_steps") == route.get("steps")
                and route.get("sha256") == sha256(route_path),
                f"mission {mission_id} {label} route mismatch")
        require(receipt.get("clean_shutdown") is True and receipt.get("game_status") == 0
                and receipt.get("xvfb_status") == 0 and receipt.get("fatal_matches") == [],
                f"mission {mission_id} {label} did not end cleanly")
        replay = receipt.get("input_replay")
        require(isinstance(replay, dict) and replay.get("loaded") is True
                and replay.get("finalized") is True and SHA256.fullmatch(
                    str(replay.get("sha256", ""))) is not None,
                f"mission {mission_id} {label} replay evidence mismatch")
    metrics = audit.get("world_center_metrics")
    require(isinstance(metrics, dict) and metrics.get("mean", 0) > 0.05
            and metrics.get("stddev", 0) > 0.05
            and metrics.get("nonblack_fraction", 0) > 0.25,
            f"mission {mission_id} world is not visibly qualified")
    controls = audit.get("control_changed_pixels")
    require(isinstance(controls, dict)
            and set(controls) == {
                "flight-pitch", "flight-roll", "flight-yaw",
                "flight-throttle", "flight-brake",
            }
            and all(isinstance(value, int) and value > 5000
                    for value in controls.values()),
            f"mission {mission_id} lacks all five observable controls")
    invariants = audit.get("automated_invariants")
    require(invariants == {
        "renderer_world_visible": True,
        "all_five_controls_visible": True,
        "strict_replay_complete": True,
    }, f"mission {mission_id} automated gameplay invariants mismatch")
    return gate


def validate_save(document: dict[str, object], identity: dict[str, object]) -> None:
    save = document.get("save_round_trip")
    require(isinstance(save, dict), "save round-trip contract is missing")
    require(save.get("status") in {"needs-final-binary-rerun", "pass"},
            "unknown save round-trip status")
    receipt = load_json(workspace_path(save.get("evidence"), "save round-trip evidence"))
    require(receipt.get("schema") == "ac6.retail-save-experiment.v1"
            and receipt.get("status") == "pass", "save evidence is not pass")
    load = receipt.get("load")
    require(isinstance(load, dict) and load.get("status") == "pass"
            and load.get("clean_shutdown") is True and load.get("game_status") == 0
            and load.get("xvfb_status") == 0 and load.get("fatal_matches") == [],
            "save load evidence is not clean")
    iso = load.get("iso")
    binary = load.get("binary")
    require(isinstance(iso, dict) and iso.get("sha256") == identity.get("iso_sha256"),
            "save evidence ISO mismatch")
    require(isinstance(binary, dict)
            and binary.get("sha256") == save.get("evidence_binary_sha256"),
            "save evidence binary mismatch")
    if save.get("status") == "pass":
        require(binary.get("sha256") == document.get("binary_sha256"),
                "save round-trip was not run on the release binary")
        create = receipt.get("create")
        require(isinstance(create, dict) and create.get("status") == "pass"
                and create.get("clean_shutdown") is True and create.get("game_status") == 0
                and create.get("xvfb_status") == 0 and create.get("fatal_matches") == [],
                "release save creation evidence is not clean")


def validate_campaign_run(document: dict[str, object]) -> str:
    campaign_run = document.get("campaign_run")
    require(isinstance(campaign_run, dict), "campaign run contract is missing")
    status = campaign_run.get("status")
    require(status in {"unverified", "pass"}, "campaign run status is invalid")
    if status != "pass":
        require(campaign_run.get("evidence") is None,
                "unverified campaign run promotes evidence")
        return str(status)
    receipt = load_json(workspace_path(
        campaign_run.get("evidence"), "campaign run evidence"
    ))
    binary = receipt.get("binary")
    require(receipt.get("schema") == "ac6.retail-campaign-run.v1"
            and receipt.get("status") == "pass"
            and receipt.get("target") == "ntsc-uj"
            and receipt.get("mission_count") == 15
            and receipt.get("fresh_process_per_mission") is True
            and receipt.get("mission01_cold_cache") is True
            and receipt.get("cache_policy") ==
            "self-built-from-empty-mission01-then-carried-forward"
            and receipt.get("storage_policy") == "sealed-predecessor-output-only"
            and isinstance(binary, dict)
            and binary.get("sha256") == document.get("binary_sha256"),
            "campaign run aggregate contract mismatch")
    plan = receipt.get("plan")
    require(isinstance(plan, dict), "campaign run plan identity is missing")
    plan_path = workspace_path(plan.get("path"), "campaign run plan")
    require(plan.get("sha256") == sha256(plan_path),
            "campaign run plan identity mismatch")
    runs = receipt.get("missions")
    missions = document.get("missions")
    require(isinstance(runs, list) and len(runs) == 15
            and isinstance(missions, list), "campaign run mission set mismatch")
    for mission_id, (run, mission) in enumerate(zip(runs, missions), 1):
        require(isinstance(run, dict) and isinstance(mission, dict)
                and run.get("mission_id") == mission_id,
                f"campaign run mission {mission_id} identity mismatch")
        runtime = mission.get("runtime")
        receipts = run.get("receipts")
        require(isinstance(runtime, dict) and isinstance(receipts, dict),
                f"campaign run mission {mission_id} receipts missing")
        for name, runtime_key in {
            "gate": "gate_receipt", "audit": "audit_receipt",
            "debrief": "debrief_receipt", "visual": "visual_receipt",
        }.items():
            record = receipts.get(name)
            require(isinstance(record, dict)
                    and record.get("path") == runtime.get(runtime_key),
                    f"campaign run mission {mission_id} {name} path mismatch")
            path = workspace_path(record.get("path"),
                                  f"campaign run mission {mission_id} {name}")
            require(record.get("sha256") == sha256(path),
                    f"campaign run mission {mission_id} {name} identity mismatch")
    return "pass"


def validate_campaign(
    document: dict[str, object], target: dict[str, object], require_release: bool = False,
    runtime: str = "rexglue-oracle",
) -> dict[str, object]:
    if runtime not in {"rexglue-oracle", "native"}:
        raise CampaignValidationError(f"unknown runtime profile: {runtime}")
    validate_identity(document, target, runtime)
    validate_static_evidence(document)
    identity = document["identity"]
    assert isinstance(identity, dict)
    binary_sha256 = str(document["binary_sha256"])
    missions = document.get("missions")
    require(isinstance(missions, list), "campaign missions are missing")
    require([entry.get("mission_id") for entry in missions if isinstance(entry, dict)]
            == list(range(1, 16)), "campaign must contain missions 1..15 exactly once and in order")

    static_qualified = gameplay_pass = debrief_pass = visual_pass = controls = 0
    previous_storage_manifest: str | None = None
    predecessor_passed = False
    for mission_id, mission in enumerate(missions, 1):
        require(isinstance(mission, dict), f"mission {mission_id} is not an object")
        require(mission.get("campaign_selector") == mission_id
                and mission.get("dpl_resource_id") == mission_id + 8,
                f"mission {mission_id} selector-to-DPL mapping mismatch")
        static_status = mission.get("static_route_status")
        require(static_status in {"selector-qualified", "qualified"},
                f"mission {mission_id} static status is invalid")
        if static_status == "qualified":
            require(mission.get("data_table_entry_index") == mission_id + 8,
                    f"mission {mission_id} DATA.TBL index mismatch")
            static_qualified += 1
        else:
            require(mission.get("data_table_entry_index") is None,
                    f"mission {mission_id} promotes an unqualified DATA.TBL index")
        runtime = mission.get("runtime")
        require(isinstance(runtime, dict), f"mission {mission_id} runtime contract is missing")
        require(runtime.get("gameplay_status") in {"unverified", "pass"}
                and runtime.get("debrief_status") in {"unverified", "pass"}
                and runtime.get("visual_status") in {"unverified", "open", "pass"}
                and isinstance(runtime.get("control_observed"), bool),
                f"mission {mission_id} runtime status is invalid")
        if runtime.get("gameplay_status") == "pass":
            gate = validate_gameplay_receipts(mission, identity, binary_sha256, runtime)
            cache = gate.get("cache")
            storage = gate.get("storage")
            require(isinstance(cache, dict) and isinstance(storage, dict),
                    f"mission {mission_id} chain state is missing")
            output_storage = storage.get("output")
            require(isinstance(output_storage, dict) and SHA256.fullmatch(
                str(output_storage.get("manifest_sha256", ""))) is not None,
                f"mission {mission_id} storage output identity is invalid")
            if mission_id == 1:
                require(cache.get("cold_start") is True and cache.get("seed") is None
                        and storage.get("seed") is None
                        and storage.get("seed_manifest_sha256") is None,
                        "mission 1 is not a cold autonomous start")
            else:
                require(predecessor_passed and previous_storage_manifest is not None,
                        f"mission {mission_id} lacks a passing predecessor")
                require(cache.get("cold_start") is False and cache.get("seed") is not None
                        and storage.get("seed") is not None
                        and storage.get("seed_manifest_sha256") == previous_storage_manifest,
                        f"mission {mission_id} does not consume its predecessor state")
            previous_storage_manifest = str(output_storage["manifest_sha256"])
            predecessor_passed = True
            gameplay_pass += 1
        else:
            require(runtime.get("route") is None and runtime.get("gate_receipt") is None
                    and runtime.get("audit_receipt") is None,
                    f"mission {mission_id} has unpromoted gameplay evidence")
            predecessor_passed = False
        if runtime.get("debrief_status") == "pass":
            receipt = load_json(workspace_path(
                runtime.get("debrief_receipt"), f"mission {mission_id} debrief receipt"))
            require(receipt.get("schema") == "ac6.retail-mission-debrief.v2"
                    and receipt.get("status") == "pass"
                    and receipt.get("target") == "ntsc-uj"
                    and receipt.get("mission_id") == mission_id
                    and receipt.get("execution_mode") == "strict-replay",
                    f"mission {mission_id} debrief receipt mismatch")
            require(isinstance(receipt.get("binary"), dict)
                    and receipt["binary"].get("sha256") == binary_sha256,
                    f"mission {mission_id} debrief binary mismatch")
            progression = receipt.get("progression")
            require(isinstance(progression, dict)
                    and progression.get("debrief_capture") is True
                    and progression.get("post_mission_capture") is True
                    and progression.get("intermission_entered") is True
                    and progression.get("save_completed_after_intermission") is True
                    and bool(progression.get("level_set_lines")),
                    f"mission {mission_id} debrief progression is incomplete")
            debrief_pass += 1
        else:
            require(runtime.get("debrief_receipt") is None,
                    f"mission {mission_id} has unpromoted debrief evidence")
        if runtime.get("visual_status") == "pass":
            receipt = load_json(workspace_path(
                runtime.get("visual_receipt"), f"mission {mission_id} visual receipt"))
            require(receipt.get("schema") == "ac6.retail-mission-visual-audit.v2"
                    and receipt.get("status") == "pass"
                    and receipt.get("target") == "ntsc-uj"
                    and receipt.get("mission_id") == mission_id
                    and receipt.get("execution_mode") == "strict-replay"
                    and receipt.get("authority") ==
                    ("native Xenos/Vulkan renderer invariants" if runtime == "native"
                     else "ReXGlue Xenos/Vulkan renderer invariants")
                    and receipt.get("pixel_parity_claimed") is False,
                    f"mission {mission_id} visual receipt mismatch")
            require(isinstance(receipt.get("binary"), dict)
                    and receipt["binary"].get("sha256") == binary_sha256,
                    f"mission {mission_id} visual binary mismatch")
            visual_pass += 1
        else:
            require(runtime.get("visual_receipt") is None,
                    f"mission {mission_id} has unpromoted visual evidence")
        if runtime.get("control_observed") is True:
            require(runtime.get("gameplay_status") == "pass",
                    f"mission {mission_id} promotes controls without gameplay")
            controls += 1

    validate_save(document, identity)
    campaign_run_status = validate_campaign_run(document)
    visual_boundary = document.get("visual_boundary")
    require(isinstance(visual_boundary, dict)
            and visual_boundary.get("status") in {"open", "pass"},
            "visual boundary status is invalid")
    workspace_path(visual_boundary.get("evidence"), "visual boundary evidence")
    require(document.get("release_contract") == {
        "mission_count": 15,
        "static_route_status": "qualified",
        "gameplay_status": "pass",
        "debrief_status": "pass",
        "visual_status": "pass",
        "control_observed": True,
        "save_round_trip_status": "pass",
        "visual_boundary_status": "pass",
        "dpl_to_data_table_status": "qualified",
        "campaign_run_status": "pass",
    }, "release contract mismatch")

    route_status = document["static_evidence"]["dpl_to_data_table"]["status"]
    save_status = document["save_round_trip"]["status"]
    release_ready = (
        static_qualified == gameplay_pass == debrief_pass == visual_pass == controls == 15
        and route_status == "qualified" and save_status == "pass"
        and visual_boundary.get("status") == "pass" and campaign_run_status == "pass"
    )
    if require_release:
        require(release_ready, "release contract is not satisfied")
    return {
        "missions": 15,
        "selector_qualified": 15,
        "static_qualified": static_qualified,
        "gameplay_pass": gameplay_pass,
        "debrief_pass": debrief_pass,
        "visual_pass": visual_pass,
        "controls": controls,
        "campaign_run": campaign_run_status,
        "release_ready": release_ready,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj",))
    parser.add_argument("--runtime", choices=("rexglue-oracle", "native"),
                        default="rexglue-oracle")
    parser.add_argument("--require-release", action="store_true")
    arguments = parser.parse_args()
    manifest = load_json(PRODUCT / "targets" / f"{arguments.target}-campaign.json")
    target = load_json(PRODUCT / "targets" / f"{arguments.target}.json")
    summary = validate_campaign(manifest, target, arguments.require_release, arguments.runtime)
    fields = " ".join(f"{key}={str(value).lower() if isinstance(value, bool) else value}"
                      for key, value in summary.items())
    print(f"campaign_manifest=pass target={arguments.target} {fields}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CampaignValidationError as error:
        print(f"campaign_manifest=fail reason={error}")
        raise SystemExit(2)
