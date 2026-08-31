#!/usr/bin/env python3
"""Run the strict fresh-process NTSC-U/J Mission 01..15 chain."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
WORKSPACE = PRODUCT.parents[1]
RUN_GATE = PRODUCT / "tools/run_gate.py"
AUDIT_CAPTURE = PRODUCT / "tools/audit_capture.py"
PLAN_SCHEMA = "ac6.retail-campaign-run-plan.v1"
RESULT_SCHEMA = "ac6.retail-campaign-run.v1"


class CampaignRunError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CampaignRunError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, object]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CampaignRunError(f"cannot read {path}: {error}") from error
    require(isinstance(document, dict), f"{path} is not a JSON object")
    return document


def workspace_file(value: object, label: str) -> Path:
    require(isinstance(value, str) and value != "", f"{label} is missing")
    relative = Path(value)
    require(not relative.is_absolute() and ".." not in relative.parts,
            f"{label} is not workspace-relative")
    resolved = (WORKSPACE / relative).resolve()
    try:
        resolved.relative_to(WORKSPACE.resolve())
    except ValueError as error:
        raise CampaignRunError(f"{label} escapes the workspace") from error
    require(resolved.is_file() and not resolved.is_symlink(),
            f"{label} is not a regular file: {relative}")
    return resolved


def validate_plan(
    plan: dict[str, object], *, target: str, binary_sha256: str,
) -> list[dict[str, object]]:
    require(plan.get("schema") == PLAN_SCHEMA, "campaign run plan schema mismatch")
    require(plan.get("target") == target, "campaign run plan target mismatch")
    require(plan.get("binary_sha256") == binary_sha256,
            "campaign run plan binary mismatch")
    missions = plan.get("missions")
    require(isinstance(missions, list) and len(missions) == 15,
            "campaign run plan must contain 15 missions")
    require(
        [entry.get("mission_id") for entry in missions if isinstance(entry, dict)]
        == list(range(1, 16)),
        "campaign run plan missions must be exactly 1..15 in order",
    )
    for mission_id, entry in enumerate(missions, 1):
        require(isinstance(entry, dict), f"mission {mission_id} plan entry is invalid")
        route = workspace_file(entry.get("route"), f"mission {mission_id} route")
        replay = workspace_file(entry.get("input_replay"), f"mission {mission_id} replay")
        require(route.parent == (PRODUCT / "routes").resolve(),
                f"mission {mission_id} route is outside routes/")
        require(entry.get("route_sha256") == sha256(route),
                f"mission {mission_id} route identity mismatch")
        require(entry.get("input_replay_sha256") == sha256(replay),
                f"mission {mission_id} replay identity mismatch")
    return missions


def run_logged(command: list[str], log: Path) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    log.write_text(completed.stdout, encoding="utf-8")
    return completed


def relative_to_workspace(path: Path) -> str:
    return str(path.resolve().relative_to(WORKSPACE.resolve()))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj",))
    parser.add_argument("--plan", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--display", default="auto")
    parser.add_argument("--duration", default=1800, type=int)
    arguments = parser.parse_args()
    if not 60 <= arguments.duration <= 3600:
        parser.error("duration must be between 60 and 3600 seconds per mission")
    plan_path = arguments.plan.resolve()
    output = arguments.output.resolve()
    try:
        plan_path.relative_to(WORKSPACE.resolve())
        output.relative_to(WORKSPACE.resolve())
    except ValueError:
        parser.error("plan and output must stay inside the AC6 workspace")
    if not plan_path.is_file() or plan_path.is_symlink():
        parser.error("plan must be a regular file")
    if output.exists():
        parser.error("output must not exist")
    static_path = PRODUCT / "build" / arguments.target / "static-validation.json"
    if not static_path.is_file():
        parser.error("static validation is required")
    static = load_json(static_path)
    binary = static.get("binary")
    if static.get("status") != "pass" or not isinstance(binary, dict):
        parser.error("static validation is not pass")
    binary_path = Path(str(binary.get("path", "")))
    if not binary_path.is_file() or sha256(binary_path) != binary.get("sha256"):
        parser.error("statically validated binary identity mismatch")
    plan = load_json(plan_path)
    try:
        missions = validate_plan(
            plan, target=arguments.target, binary_sha256=str(binary["sha256"]),
        )
    except CampaignRunError as error:
        parser.error(str(error))

    output.mkdir(parents=True)
    started = time.time()
    mission_results: list[dict[str, object]] = []
    status = "pass"
    failure = ""
    previous_output: Path | None = None
    for mission_id, entry in enumerate(missions, 1):
        route = workspace_file(entry["route"], f"mission {mission_id} route")
        replay = workspace_file(entry["input_replay"], f"mission {mission_id} replay")
        mission_output = output / f"mission-{mission_id:02d}"
        command = [
            sys.executable, str(RUN_GATE), "--target", arguments.target,
            "--mission-id", str(mission_id), "--route", str(route),
            "--input-replay", str(replay), "--output", str(mission_output),
            "--display", arguments.display, "--duration", str(arguments.duration),
        ]
        if previous_output is not None:
            command.extend([
                "--storage-seed", str(previous_output / "storage-root"),
                "--cache-seed", str(previous_output),
            ])
        gate = run_logged(command, output / f"mission-{mission_id:02d}-gate.log")
        if gate.returncode != 0:
            status = "fail"
            failure = f"mission {mission_id} gameplay gate exited {gate.returncode}"
            break
        audit = run_logged([
            sys.executable, str(AUDIT_CAPTURE), "--target", arguments.target,
            "--capture", str(mission_output),
        ], output / f"mission-{mission_id:02d}-audit.log")
        if audit.returncode != 0:
            status = "fail"
            failure = f"mission {mission_id} automated audit exited {audit.returncode}"
            break
        receipts = {
            name: mission_output / filename for name, filename in {
                "gate": "RESULT.json", "audit": "AUDIT.json",
                "debrief": "DEBRIEF.json", "visual": "VISUAL.json",
            }.items()
        }
        if not all(path.is_file() for path in receipts.values()):
            status = "fail"
            failure = f"mission {mission_id} receipt set is incomplete"
            break
        mission_results.append({
            "mission_id": mission_id,
            "route": relative_to_workspace(route),
            "input_replay": relative_to_workspace(replay),
            "receipts": {
                name: {
                    "path": relative_to_workspace(path),
                    "sha256": sha256(path),
                }
                for name, path in receipts.items()
            },
        })
        previous_output = mission_output

    result = {
        "schema": RESULT_SCHEMA,
        "status": status,
        "target": arguments.target,
        "binary": binary,
        "plan": {
            "path": relative_to_workspace(plan_path),
            "sha256": sha256(plan_path),
        },
        "mission_count": len(mission_results),
        "missions": mission_results,
        "fresh_process_per_mission": True,
        "mission01_cold_cache": True,
        "cache_policy": "self-built-from-empty-mission01-then-carried-forward",
        "storage_policy": "sealed-predecessor-output-only",
        "duration_seconds": time.time() - started,
        "error": failure,
    }
    (output / "RESULT.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(output / "RESULT.json")
    return 0 if status == "pass" and len(mission_results) == 15 else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CampaignRunError as error:
        print(f"run_campaign: {error}", file=sys.stderr)
        raise SystemExit(2)
