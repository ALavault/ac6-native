#!/usr/bin/env python3
"""Qualify the bounded US campaign scenario children, metadata only.

The tool reads the already-qualified DATA.TBL[9..23] ranges from the exact US
ISO, takes child 0 of each root FHM as the retail scenario, and runs the
existing byte-for-byte structural round-trip plus schema walks.  It emits
only hashes, sizes and counters; no scenario bytes, PACs, objective meaning,
or runtime claim is retained.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
from typing import Any


WORKSPACE = Path(__file__).resolve().parents[1]
PRODUCT = WORKSPACE / "recompilation" / "ace-combat-6-retail"
TOOLS = WORKSPACE / "tools"
TARGET = PRODUCT / "targets" / "ntsc-uj.json"
PAYLOAD_MANIFEST_DEFAULT = (
    WORKSPACE / "artifacts" / "retail-us-mission-payloads-static-20260828"
    / "manifest.json"
)
MISSION_FIRST = 1
MISSION_LAST = 15
SECTOR_SIZE = 2048
PAC_NAMES = ("DATA00.PAC", "DATA01.PAC")


class QualificationError(RuntimeError):
    pass


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise QualificationError(f"cannot load helper: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


PAYLOAD = load_module(
    "ac6_us_scenario_payload", WORKSPACE / "scripts" / "qualify_us_mission_payloads.py"
)
ROUNDTRIP = load_module(
    "ac6_us_scenario_roundtrip", TOOLS / "roundtrip_ac6_scenario.py"
)
SCHEMA = load_module(
    "ac6_us_scenario_schema", TOOLS / "validate_ac6_scenario_schema.py"
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise QualificationError(f"JSON object required: {path}")
    return value


def schema_documents() -> dict[str, dict[str, Any]]:
    return {
        path.stem: load_json(path)
        for path in sorted((WORKSPACE / "analysis" / "scenario-schema").glob("*.json"))
    }


def scenario_child(payload: bytes, mission_id: int) -> bytes:
    root = PAYLOAD.FHM.parse_fhm(payload)
    # The scenario child is a node-graph payload whose first word is the
    # retail class tag 0x10, not an FHM container magic.  The enclosing root
    # FHM child index is the qualified selector boundary.
    if not root or root[0].index != 0 or root[0].data[:4] != b"\0\0\0\x10":
        raise QualificationError(f"mission {mission_id} has no scenario child 0")
    return root[0].data


def validator_summary(
    scenario: bytes, schemas: dict[str, dict[str, Any]]
) -> tuple[dict[str, dict[str, Any]], list[str]]:
    parsed = SCHEMA.Payload(scenario)
    summaries: dict[str, dict[str, Any]] = {}
    errors: list[str] = []
    for name, document in schemas.items():
        validator = SCHEMA.resolve_validator(document)
        if validator is None:
            continue
        try:
            result = validator(document, parsed)
        except Exception as error:  # fail closed and retain only the error class
            errors.append(f"{name}:{type(error).__name__}:{error}")
            continue
        summary = {
            key: result[key]
            for key in (
                "inconsistencies",
                "slot0_entries",
                "obj_records_reached",
                "set_nodes",
                "act_nodes",
                "order_records_reached",
                "maneuver_nodes",
                "maneuver_elements",
                "declared_count",
                "table_children",
                "elements_present",
                "elements_absent",
            )
            if key in result
        }
        if result.get("inconsistencies", 0):
            errors.append(f"{name}:inconsistencies={result['inconsistencies']}")
        summaries[name] = summary
    return summaries, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument("--data-tbl", type=Path, default=PAYLOAD.DATA_TBL_DEFAULT)
    parser.add_argument(
        "--payload-manifest", type=Path, default=PAYLOAD_MANIFEST_DEFAULT
    )
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    target = load_json(TARGET)
    iso = args.iso.resolve()
    data_tbl = args.data_tbl.resolve()
    payload_manifest_path = args.payload_manifest.resolve()
    output = args.output.resolve()
    if output.exists():
        raise QualificationError(f"refusing to overwrite: {output}")
    expected_iso = target.get("iso", {})
    if not iso.is_file() or iso.stat().st_size != expected_iso.get("size"):
        raise QualificationError("US ISO size does not match target definition")
    iso_digest = sha256_file(iso)
    if iso_digest != expected_iso.get("sha256"):
        raise QualificationError(f"US ISO SHA-256 mismatch: {iso_digest}")
    if not data_tbl.is_file() or not payload_manifest_path.is_file():
        raise QualificationError("DATA.TBL or payload manifest is missing")
    payload_manifest = load_json(payload_manifest_path)
    payload_records = payload_manifest.get("records")
    if (
        payload_manifest.get("schema") != "ac6.retail-us-mission-payload-static.v1"
        or payload_manifest.get("status") != "qualified-structural-only"
        or payload_manifest.get("source_iso", {}).get("sha256") != iso_digest
        or not isinstance(payload_records, list)
        or len(payload_records) != 15
    ):
        raise QualificationError("payload manifest identity/status mismatch")

    _, _, entries = PAYLOAD.parse_data_tbl(data_tbl)
    selected = [entries[index + 8] for index in range(MISSION_FIRST, MISSION_LAST + 1)]
    if [int(item["index"]) for item in selected] != list(range(9, 24)):
        raise QualificationError("mission DATA.TBL selection is not 9..23")
    schemas = schema_documents()
    image_size = iso.stat().st_size
    records: list[dict[str, Any]] = []
    total_scenario_bytes = 0
    total_nodes = 0
    total_tables = 0
    total_obj_records = 0
    total_order_records = 0
    with iso.open("rb") as stream:
        volumes: dict[str, tuple[int, int]] = {}
        for name in PAC_NAMES:
            volume, entry = PAYLOAD.XDVDFS.resolve_entry(stream, image_size, name)
            volumes[name] = (volume.game_offset + entry.sector * SECTOR_SIZE, entry.length)
        for mission_id, item in enumerate(selected, start=MISSION_FIRST):
            archive = str(item["archive"])
            pac_base, _ = volumes[archive]
            stream.seek(pac_base + int(item["offset"]))
            stored = stream.read(int(item["stored_size"]))
            payload = PAYLOAD.CODEC.decompress_entry(
                stored, int(item["codec_index"]), int(item["expanded_size"])
            )
            expected_payload = payload_records[mission_id - 1]
            scenario = scenario_child(payload, mission_id)
            scenario_digest = hashlib.sha256(scenario).hexdigest()
            if (
                not isinstance(expected_payload, dict)
                or expected_payload.get("expanded_sha256")
                != hashlib.sha256(payload).hexdigest()
            ):
                raise QualificationError(f"payload identity mismatch: mission {mission_id}")
            walk = ROUNDTRIP.Walk(ROUNDTRIP.Payload(scenario))
            rebuilt, written = ROUNDTRIP.reemit(ROUNDTRIP.Payload(scenario), walk)
            unwritten_nonzero = sum(
                1 for index, claimed in enumerate(written) if not claimed and scenario[index]
            )
            if rebuilt != scenario or unwritten_nonzero:
                raise QualificationError(f"scenario round-trip mismatch: mission {mission_id}")
            validators, validator_errors = validator_summary(scenario, schemas)
            if validator_errors:
                raise QualificationError(
                    f"scenario schema validation failed for mission {mission_id}: "
                    + "; ".join(validator_errors)
                )
            obj = validators.get("ObjBin", {})
            order = validators.get("OrderBin", {})
            record = {
                "mission_id": mission_id,
                "campaign_selector": mission_id,
                "dpl_resource_id": mission_id + 8,
                "data_table_entry_index": int(item["index"]),
                "payload_sha256": hashlib.sha256(payload).hexdigest(),
                "scenario_size": len(scenario),
                "scenario_sha256": scenario_digest,
                "root_children": len(PAYLOAD.FHM.parse_fhm(payload) or ()),
                "roundtrip": {
                    "identical": True,
                    "nodes": len(walk.nodes),
                    "tables": len(walk.tables),
                    "data_blocks": len(walk.data),
                    "unwritten_nonzero": 0,
                },
                "validators": validators,
            }
            records.append(record)
            total_scenario_bytes += len(scenario)
            total_nodes += len(walk.nodes)
            total_tables += len(walk.tables)
            total_obj_records += int(obj.get("obj_records_reached", 0))
            total_order_records += int(order.get("order_records_reached", 0))

    manifest = {
        "schema": "ac6.retail-us-scenario-static.v1",
        "status": "qualified-structural-only",
        "target": "ntsc-uj",
        "xex_sha256": target["xex"]["sha256"],
        "source_iso": {"path": str(iso), "size": image_size, "sha256": iso_digest},
        "data_tbl": {
            "path": str(data_tbl),
            "size": data_tbl.stat().st_size,
            "sha256": sha256_file(data_tbl),
        },
        "payload_manifest": {
            "path": str(payload_manifest_path),
            "sha256": sha256_file(payload_manifest_path),
        },
        "mission_range": [MISSION_FIRST, MISSION_LAST],
        "missions": records,
        "totals": {
            "mission_payloads": len(records),
            "scenario_bytes": total_scenario_bytes,
            "nodes": total_nodes,
            "tables": total_tables,
            "obj_records": total_obj_records,
            "order_records": total_order_records,
            "roundtrip_pass": len(records),
            "schema_validation_pass": len(records),
        },
        "policy": {
            "bounded_iso_reads": True,
            "complete_pac_copied": False,
            "payloads_retained": False,
            "objective_semantics_inferred": False,
            "runtime_or_renderer_claimed": False,
        },
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"status": manifest["status"], **manifest["totals"], "output": str(output)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, QualificationError, json.JSONDecodeError) as error:
        print(f"qualify_us_scenario_payloads=fail error={error}", file=sys.stderr)
        raise SystemExit(2)
