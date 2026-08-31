#!/usr/bin/env python3
"""Produce automated v3 gameplay, visual and debrief receipts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
GAMEPLAY_CAPTURES = (
    "gameplay-hud",
    "flight-pitch",
    "flight-roll",
    "flight-yaw",
    "flight-throttle",
    "flight-brake",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def capture_name(entry: dict[str, object]) -> str:
    value = entry.get("path")
    if not isinstance(value, str):
        return ""
    return value.split("-", 2)[-1].removesuffix(".png")


def require(condition: bool, parser: argparse.ArgumentParser, message: str) -> None:
    if not condition:
        parser.error(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", default="ntsc-uj", choices=("ntsc-uj",))
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument(
        "--output", type=Path,
        help="receipt directory; defaults to the capture directory",
    )
    arguments = parser.parse_args()
    root = PRODUCT / "build" / arguments.target
    capture_root = arguments.capture.resolve()
    result_path = capture_root / "RESULT.json"
    manifest_path = root / "manifest.json"
    static_path = root / "static-validation.json"
    require(result_path.is_file() and manifest_path.is_file() and static_path.is_file(),
            parser, "capture, manifest and static validation are required")
    result = json.loads(result_path.read_text(encoding="utf-8"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    static = json.loads(static_path.read_text(encoding="utf-8"))
    require(result.get("status") == "capture-ready", parser,
            "capture is not ready for automated audit")
    require(result.get("schema") == "ac6.retail-gameplay-gate.v3", parser,
            "only an ac6.retail-gameplay-gate.v3 receipt may be audited")
    require(result.get("target") == arguments.target and result.get("manifest") == manifest,
            parser, "capture does not match the exact target manifest")
    require(result.get("binary") == static.get("binary") and static.get("status") == "pass",
            parser, "capture does not match the statically validated binary")
    require(result.get("execution_mode") == "strict-replay", parser,
            "release audit requires strict controller replay")
    mission_id = result.get("mission_id")
    require(isinstance(mission_id, int) and 1 <= mission_id <= 15, parser,
            "mission identity is invalid")

    captures = result.get("captures")
    require(isinstance(captures, list), parser, "capture inventory is missing")
    named: dict[str, dict[str, object]] = {}
    for entry in captures:
        require(isinstance(entry, dict), parser, "capture inventory entry is invalid")
        path = capture_root / str(entry.get("path", ""))
        require(path.is_file() and path.stat().st_size == entry.get("bytes")
                and sha256(path) == entry.get("sha256"), parser,
                f"capture identity mismatch: {path}")
        named[capture_name(entry)] = entry
    for name in (*GAMEPLAY_CAPTURES, "debrief", "post-mission"):
        require(name in named, parser, f"required capture is missing: {name}")

    metrics = result.get("world_center_metrics")
    require(isinstance(metrics, dict)
            and metrics.get("mean", 0) > 0.05
            and metrics.get("stddev", 0) > 0.05
            and metrics.get("nonblack_fraction", 0) > 0.25
            and metrics.get("hud_green_fraction", 0) > 0.0005,
            parser, "world/HUD renderer invariants failed")
    controls = result.get("control_changed_pixels")
    required_controls = {name for name in GAMEPLAY_CAPTURES if name != "gameplay-hud"}
    require(isinstance(controls, dict)
            and set(controls) == required_controls
            and all(isinstance(value, int) and value > 5000 for value in controls.values()),
            parser, "all five control observables must exceed 5000 changed pixels")
    replay = result.get("input_replay")
    require(isinstance(replay, dict) and replay.get("loaded") is True
            and replay.get("finalized") is True, parser,
            "strict replay completion evidence is missing")
    progression = result.get("progression")
    require(isinstance(progression, dict)
            and progression.get("debrief_capture") is True
            and progression.get("post_mission_capture") is True
            and progression.get("intermission_entered") is True
            and progression.get("save_completed_after_intermission") is True
            and isinstance(progression.get("level_set_lines"), list)
            and bool(progression.get("level_set_lines")),
            parser, "debrief/save/progression evidence is incomplete")
    require(result.get("clean_shutdown") is True and result.get("game_status") == 0
            and result.get("xvfb_status") == 0 and result.get("fatal_matches") == [],
            parser, "runtime did not end cleanly")

    output = arguments.output.resolve() if arguments.output else capture_root
    output.mkdir(parents=True, exist_ok=True)
    paths = {
        "audit": output / "AUDIT.json",
        "debrief": output / "DEBRIEF.json",
        "visual": output / "VISUAL.json",
    }
    require(not any(path.exists() for path in paths.values()), parser,
            "one or more receipt outputs already exist")
    common = {
        "status": "pass",
        "target": arguments.target,
        "mission_id": mission_id,
        "manifest": manifest,
        "binary": result["binary"],
        "route": result["route"],
        "capture_result": str(result_path),
        "execution_mode": "strict-replay",
        "input_replay": replay,
        "clean_shutdown": True,
        "game_status": 0,
        "xvfb_status": 0,
        "fatal_matches": [],
    }
    audit = {
        **common,
        "schema": "ac6.retail-gameplay-audit.v3",
        "world_center_metrics": metrics,
        "control_changed_pixels": controls,
        "automated_invariants": {
            "renderer_world_visible": True,
            "all_five_controls_visible": True,
            "strict_replay_complete": True,
        },
    }
    debrief = {
        **common,
        "schema": "ac6.retail-mission-debrief.v2",
        "progression": progression,
        "storage": result.get("storage"),
    }
    visual = {
        **common,
        "schema": "ac6.retail-mission-visual-audit.v2",
        "authority": "ReXGlue Xenos/Vulkan renderer invariants",
        "pixel_parity_claimed": False,
        "world_center_metrics": metrics,
        "control_changed_pixels": controls,
        "capture_identities": {name: named[name] for name in GAMEPLAY_CAPTURES},
    }
    for key, document in (("audit", audit), ("debrief", debrief), ("visual", visual)):
        paths[key].write_text(
            json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(json.dumps({key: str(path) for key, path in paths.items()}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
