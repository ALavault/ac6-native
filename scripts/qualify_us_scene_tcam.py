#!/usr/bin/env python3
"""Qualify bounded US Scene/TCAM structure for campaign payloads 1..15.

Only the compressed DATA.TBL ranges already qualified by
``qualify_us_mission_payloads.py`` are read from the exact US ISO.  Expanded
payloads are scanned in memory with the existing strict Scene/TCAM parser and
discarded after each mission.  No PAC is copied and no runtime or objective
semantics are inferred.
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
    "ac6_us_scene_payload", WORKSPACE / "scripts" / "qualify_us_mission_payloads.py"
)

# The scanner imports its existing ``ac6_fhm`` and cache helpers by module
# name; expose the workspace tools directory exactly as its normal CLI does.
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
SCENE = load_module(
    "ac6_us_scene_tcam_scanner", TOOLS / "audit_ac6_scene_tcam_corpus.py"
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
    if not data_tbl.is_file():
        raise QualificationError(f"DATA.TBL is missing: {data_tbl}")
    if not payload_manifest_path.is_file():
        raise QualificationError(f"payload manifest is missing: {payload_manifest_path}")
    payload_manifest = load_json(payload_manifest_path)
    if (
        payload_manifest.get("schema") != "ac6.retail-us-mission-payload-static.v1"
        or payload_manifest.get("status") != "qualified-structural-only"
        or payload_manifest.get("target") != "ntsc-uj"
        or payload_manifest.get("source_iso", {}).get("sha256") != iso_digest
    ):
        raise QualificationError("payload manifest identity/status mismatch")
    payload_records = payload_manifest.get("records")
    if not isinstance(payload_records, list) or len(payload_records) != 15:
        raise QualificationError("payload manifest does not contain 15 records")

    _, _, entries = PAYLOAD.parse_data_tbl(data_tbl)
    selected = [entries[index + 8] for index in range(MISSION_FIRST, MISSION_LAST + 1)]
    if [int(item["index"]) for item in selected] != list(range(9, 24)):
        raise QualificationError("mission DATA.TBL selection is not 9..23")

    image_size = iso.stat().st_size
    with iso.open("rb") as stream:
        volumes: dict[str, tuple[int, int]] = {}
        for name in PAC_NAMES:
            volume, entry = PAYLOAD.XDVDFS.resolve_entry(stream, image_size, name)
            volumes[name] = (volume.game_offset + entry.sector * SECTOR_SIZE, entry.length)

        records: list[dict[str, Any]] = []
        for mission_id, item in enumerate(selected, start=MISSION_FIRST):
            expected_payload = payload_records[mission_id - 1]
            if not isinstance(expected_payload, dict):
                raise QualificationError(f"payload record is not an object: {mission_id}")
            archive = str(item["archive"])
            pac_base, pac_size = volumes[archive]
            offset = int(item["offset"])
            stored_size = int(item["stored_size"])
            expanded_size = int(item["expanded_size"])
            if offset + stored_size > pac_size:
                raise QualificationError(f"mission slice exceeds {archive}: {mission_id}")
            stored = PAYLOAD.read_iso_range(
                stream, pac_base + offset, stored_size, image_size
            )
            payload = PAYLOAD.CODEC.decompress_entry(
                stored, int(item["codec_index"]), expanded_size
            )
            expanded_digest = hashlib.sha256(payload).hexdigest()
            if (
                expected_payload.get("data_table_entry_index") != int(item["index"])
                or expected_payload.get("expanded_size") != expanded_size
                or expected_payload.get("expanded_sha256") != expanded_digest
            ):
                raise QualificationError(f"payload identity mismatch: mission {mission_id}")
            try:
                scan = SCENE.scan_mission_payload(payload, mission_id)
                SCENE.validate_expected(mission_id, scan)
            except (ValueError, RuntimeError) as error:
                raise QualificationError(
                    f"Scene/TCAM scan failed for mission {mission_id}: {error}"
                ) from error
            tcam_resources = [
                {
                    "path": resource.path,
                    "size": resource.size,
                    "sha256": resource.sha256,
                }
                for resource in scan.tcam_resources
            ]
            records.append(
                {
                    "mission_id": mission_id,
                    "campaign_selector": mission_id,
                    "dpl_resource_id": mission_id + 8,
                    "data_table_entry_index": int(item["index"]),
                    "payload_size": expanded_size,
                    "payload_sha256": expanded_digest,
                    "scene_tables": scan.scene_tables,
                    "scene_paths": scan.scene_paths,
                    "tcam_resources": len(tcam_resources),
                    "branches": list(scan.branches),
                    "resources": tcam_resources,
                }
            )

    totals = {
        "mission_payloads": len(records),
        "scene_tables": sum(record["scene_tables"] for record in records),
        "scene_paths": sum(record["scene_paths"] for record in records),
        "tcam_resources": sum(record["tcam_resources"] for record in records),
    }
    all_resources = [
        {"mission_id": record["mission_id"], **resource}
        for record in records
        for resource in record["resources"]
    ]
    for record in records:
        del record["resources"]
    manifest = {
        "schema": "ac6.retail-us-scene-tcam-static.v1",
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
        "tcam_resources": all_resources,
        "totals": totals,
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
    print(json.dumps({"status": manifest["status"], **totals, "output": str(output)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, QualificationError, json.JSONDecodeError) as error:
        print(f"qualify_us_scene_tcam=fail error={error}", file=sys.stderr)
        raise SystemExit(2)
