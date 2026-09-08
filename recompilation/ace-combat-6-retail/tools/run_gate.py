#!/usr/bin/env python3
"""Run one identity-sealed NTSC-U/J retail mission gameplay gate."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import time
from argparse import Namespace
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
WORKSPACE = PRODUCT.parents[1]
# The original 96-step route remains archived by its prior receipts. The
# qualified release route uses the statically justified longstart handoff and
# the v2 audit shape; it remains diagnostic until adaptive cinematic exit and
# a matching gameplay receipt are promoted.
ROUTE = PRODUCT / "routes/mission01-qualified-96-longstart-v2-candidate.steps"
ROUTE_SHA256 = "44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9"
# Keep the older longstart recipe available for the optional launch/render
# probes whose transformations are defined against its historical capture
# labels. The original 96-step route is also accepted for read-only stock
# input diagnostics; the default product gate always uses ROUTE above.
DIAGNOSTIC_ROUTE = PRODUCT / "routes/mission01-qualified-96-longstart-candidate.steps"
DIAGNOSTIC_ROUTE_SHA256 = "6ef77bef4cfd2f59aa19e317c9e44d539fed14c6fe7c11a0171b235f514dac4b"
ORIGINAL_DIAGNOSTIC_ROUTE = PRODUCT / "routes/mission01-qualified-96.steps"
ORIGINAL_DIAGNOSTIC_ROUTE_SHA256 = "771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043"
RUNNER_PATH = WORKSPACE / "tools/ac6-oracle-run.py"
AUDIO_FATAL = re.compile(
    r"SdlAudioDriver init failed|XAudioRegisterRenderDriverClient failed",
    re.IGNORECASE,
)
ROUTE_SYNC_MARKERS = (
    b"type28=",
    b"selector44=",
    b"state40=",
    b"[ac6-campaign-transition]",
    b"[ac6-visual-phase]",
    b"[ac6-current-level]",
    b"[ac6-current-level-set]",
    b"[ac6-save-manager]",
    b"[ac6-post-mission]",
)

GAMEPLAY_SCHEMA = "ac6.retail-gameplay-gate.v3"
REPLAY_SCHEMA = "ac6.controller-input-replay.v4"
GAMEPLAY_CAPTURES = (
    "gameplay-hud",
    "flight-pitch",
    "flight-roll",
    "flight-yaw",
    "flight-throttle",
    "flight-brake",
)
FULL_MISSION_CAPTURES = (*GAMEPLAY_CAPTURES, "debrief", "post-mission")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def is_sealed_route(path: Path, steps: list[tuple[str, str, str]]) -> bool:
    """Accept the current v2 route and the archived probe recipe only."""
    if len(steps) != 96:
        return False
    qualified = {
        ROUTE.resolve(): ROUTE_SHA256,
        DIAGNOSTIC_ROUTE.resolve(): DIAGNOSTIC_ROUTE_SHA256,
        ORIGINAL_DIAGNOSTIC_ROUTE.resolve(): ORIGINAL_DIAGNOSTIC_ROUTE_SHA256,
    }
    expected = qualified.get(path.resolve())
    return expected is not None and sha256(path) == expected


def load_runner():
    sys.path.insert(0, str(WORKSPACE / "tools"))
    spec = importlib.util.spec_from_file_location("ac6_oracle_route_runner", RUNNER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("route runner cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def image_metric(path: Path, expression: str, crop: str | None = None) -> float:
    command = ["convert", str(path)]
    if crop:
        command.extend(["-gravity", "center", "-crop", crop, "+repage"])
    command.extend(["-format", expression, "info:"])
    value = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    ).stdout
    return float(value)


def changed_pixels(left: Path, right: Path) -> int:
    completed = subprocess.run(
        ["compare", "-metric", "AE", str(left), str(right), "null:"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if completed.returncode not in (0, 1):
        raise RuntimeError(completed.stderr.strip() or "image comparison failed")
    match = re.match(r"\s*(\d+)", completed.stderr)
    if match is None:
        raise RuntimeError(completed.stderr.strip() or "image comparison metric missing")
    return int(match.group(1))


def gameplay_image_metrics(path: Path) -> dict[str, float]:
    """Measure only the center world region, excluding most HUD chrome."""
    return {
        "mean": image_metric(path, "%[fx:mean]", "50%x50%+0+0"),
        "stddev": image_metric(path, "%[fx:standard_deviation]", "50%x50%+0+0"),
        "nonblack_fraction": nonblack_fraction(path, "50%x50%+0+0"),
        "hud_green_fraction": hud_green_fraction(path),
    }


def nonblack_fraction(path: Path, crop: str | None = None) -> float:
    command = ["convert", str(path)]
    if crop:
        command.extend(["-gravity", "center", "-crop", crop, "+repage"])
    command.extend(["-colorspace", "gray", "-threshold", "1", "-format", "%[fx:mean]", "info:"])
    value = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    ).stdout
    return float(value)


def hud_green_fraction(path: Path) -> float:
    """Measure the qualified green HUD/radar layer over the complete frame."""
    value = subprocess.run(
        [
            "convert", str(path), "-colorspace", "sRGB", "-fx",
            "g>r*1.4&&g>b*1.4&&g>0.08?1:0", "-format", "%[fx:mean]", "info:",
        ],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout
    return float(value)


def validate_gameplay_visuals(
    captures: dict[str, Path], visual_phases: list[str]
) -> tuple[dict[str, float], dict[str, int], list[str]]:
    errors: list[str] = []
    missing = [name for name in GAMEPLAY_CAPTURES if name not in captures]
    if missing:
        return {}, {}, ["missing gameplay captures: " + ", ".join(missing)]
    if not any("cinematic=0" in phase and "world=1" in phase and
               "hud=1" in phase and "stable=30" in phase
               for phase in visual_phases):
        errors.append("gameplay phase never became stable for 30 frames")
    baseline = captures["gameplay-hud"]
    metrics = gameplay_image_metrics(baseline)
    if metrics["mean"] <= 0.05:
        errors.append("gameplay center mean is not above 0.05")
    if metrics["stddev"] <= 0.05:
        errors.append("gameplay center standard deviation is not above 0.05")
    if metrics["nonblack_fraction"] <= 0.25:
        errors.append("gameplay center does not contain over 25% non-black pixels")
    if metrics["hud_green_fraction"] <= 0.0005:
        errors.append("qualified green HUD layer is not visible over the world")
    effects = {
        name: changed_pixels(baseline, captures[name])
        for name in GAMEPLAY_CAPTURES[1:]
    }
    weak = [name for name, count in effects.items() if count <= 5000]
    if weak:
        errors.append("controls change at most 5000 pixels: " + ", ".join(weak))
    return metrics, effects, errors


def validate_full_mission_route(steps: list[tuple[str, str, str]]) -> None:
    """Fail closed on routes used to record or replay a release movie."""
    capture_names = {
        argument for operation, argument, _ in steps if operation == "capture"
    }
    missing = [name for name in FULL_MISSION_CAPTURES if name not in capture_names]
    if not 1 <= len(steps) <= 512 or missing:
        detail = ", ".join(missing) if missing else "none"
        raise RuntimeError(
            "full-mission route must contain 1..512 steps and all release "
            f"captures; missing: {detail}"
        )


def select_display(requested: str, first: int = 120, last: int = 199) -> str:
    if requested != "auto":
        return requested
    for number in range(first, last + 1):
        if not Path(f"/tmp/.X11-unix/X{number}").exists() and not Path(
            f"/tmp/.X{number}-lock"
        ).exists():
            return f":{number}"
    raise RuntimeError("no private X display is available")


def missing_route_sync_markers(binary: Path) -> list[str]:
    """Return guest predicates the sealed route cannot observe in this host."""
    contents = binary.read_bytes()
    return [marker.decode("ascii") for marker in ROUTE_SYNC_MARKERS if marker not in contents]


def validate_static_receipt(
    target: str, manifest: dict[str, object], static: dict[str, object]
) -> None:
    if (
        static.get("status") != "pass"
        or static.get("target") != target
        or static.get("manifest") != manifest
    ):
        raise RuntimeError("static validation does not match the exact target/manifest")


def validate_gameplay_receipt_schema(receipt: dict[str, object]) -> None:
    if receipt.get("schema") != GAMEPLAY_SCHEMA:
        raise RuntimeError("legacy or unknown gameplay receipt schema")


def replace_first_key_after_capture(
    steps: list[tuple[str, str, str]], capture_label: str, key: str, hold: str
) -> list[tuple[str, str, str]]:
    """Change one bounded key edge without relying on stale route indices."""
    capture_index = next(
        (
            index
            for index, step in enumerate(steps)
            if step[0] == "capture" and step[1] == capture_label
        ),
        None,
    )
    if capture_index is None:
        raise RuntimeError(f"route capture not found: {capture_label}")
    key_index = next(
        (
            index
            for index in range(capture_index + 1, len(steps))
            if steps[index][0] == "key" and steps[index][1] == key
        ),
        None,
    )
    if key_index is None:
        raise RuntimeError(
            f"route key not found after {capture_label}: {key}"
        )
    steps[key_index] = ("key", key, hold)
    return steps


def validate_cache_seed(cache_seed: Path) -> None:
    """Require both host cache trees before spending a diagnostic run."""
    if not cache_seed.is_dir() or cache_seed.is_symlink():
        raise RuntimeError("cache seed must be a regular directory")
    for path in cache_seed.rglob("*"):
        if path.is_symlink():
            raise RuntimeError("cache seed contains a symlink")
    for name in ("cache", "cache-root"):
        source = cache_seed / name
        if not source.is_dir() or source.is_symlink():
            raise RuntimeError(f"cache seed must contain regular {name}/")


def validate_tree(root: Path, label: str) -> None:
    """Reject links and special files before copying sealed campaign state."""
    if not root.is_dir() or root.is_symlink():
        raise RuntimeError(f"{label} must be a regular directory")
    for path in root.rglob("*"):
        if path.is_symlink():
            raise RuntimeError(f"{label} contains a symlink")
        if not path.is_dir() and not path.is_file():
            raise RuntimeError(f"{label} contains a special file")


def tree_manifest_sha256(root: Path) -> str:
    """Hash a portable file manifest; an empty tree matches SHA-256(empty)."""
    validate_tree(root, "state tree")
    digest = hashlib.sha256()
    for path in sorted((entry for entry in root.rglob("*") if entry.is_file()),
                       key=lambda entry: entry.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        row = f"{relative}\0{path.stat().st_size}\0{sha256(path)}\n"
        digest.update(row.encode("utf-8"))
    return digest.hexdigest()


def tree_summary(root: Path) -> dict[str, object]:
    files = [path for path in root.rglob("*") if path.is_file()]
    return {
        "manifest_sha256": tree_manifest_sha256(root),
        "files": len(files),
        "bytes": sum(path.stat().st_size for path in files),
    }


def load_replay_header(path: Path) -> tuple[dict[str, object], bytes]:
    if not path.is_file() or path.is_symlink():
        raise RuntimeError("input replay must be a regular file")
    if path.stat().st_size > 256 * 1024 * 1024:
        raise RuntimeError("input replay exceeds the 256 MiB bound")
    with path.open("rb") as stream:
        line = stream.readline(64 * 1024 + 1)
    if len(line) > 64 * 1024 or not line.endswith(b"\n"):
        raise RuntimeError("input replay header is missing or oversized")
    try:
        header = json.loads(line)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"input replay header is invalid: {error}") from error
    if not isinstance(header, dict) or header.get("schema") != REPLAY_SCHEMA:
        raise RuntimeError("input replay schema mismatch")
    return header, line


def validate_replay_header(
    header: dict[str, object], *, binary: Path, config: Path, route: Path,
    source_xex: Path, storage_root: Path,
) -> None:
    producer = header.get("producer")
    target = header.get("target")
    session = header.get("session")
    if not isinstance(producer, dict) or not isinstance(target, dict) \
            or not isinstance(session, dict):
        raise RuntimeError("input replay identity is incomplete")
    if producer.get("binary_sha256") != sha256(binary):
        raise RuntimeError("input replay binary identity mismatch")
    config_sha256 = sha256(config)
    if (producer.get("build_sha256") != config_sha256
            or session.get("runtime_config_sha256") != config_sha256
            or session.get("behavior_config_sha256") != config_sha256):
        raise RuntimeError("input replay configuration identity mismatch")
    if (target.get("target_id") != "ac6-ntsc-uj-default-xex"
            or target.get("xex_sha256") != sha256(source_xex)):
        raise RuntimeError("input replay target identity mismatch")
    if session.get("route_sha256") != sha256(route):
        raise RuntimeError("input replay route identity mismatch")
    if session.get("profile_save_manifest_sha256") != tree_manifest_sha256(storage_root):
        raise RuntimeError("input replay save-state identity mismatch")


def stage_replay_header(output: Path, header_line: bytes) -> Path:
    path = output / "input-replay.header.jsonl"
    path.write_bytes(header_line)
    path.chmod(0o444)
    return path


def write_stock_input_record_header(
    output: Path, binary: Path, config: Path, route: Path, source_xex: Path,
    storage_root: Path,
) -> Path:
    """Write the exact-US v4 header used by the read-only controller recorder."""
    digest = sha256(binary)
    header = {
        "kind": "header",
        "producer": {
            "binary_sha256": digest,
            "build_sha256": sha256(config),
            "implementation_commit": "ab90b54713e5889f33eee1cc8681dae89fe83d1e",
            "lane": "ac6-recomp",
            "platform": "linux-x86_64-vulkan",
        },
        "schema": "ac6.controller-input-replay.v4",
        "segment": {
            "kind": "full_recording",
            "parent_marker_count": None,
            "parent_payload_sha256": None,
            "parent_replay_sha256": None,
            "parent_start_marker": None,
        },
        "session": {
            "behavior_config_sha256": sha256(config),
            "content_manifest_sha256": sha256(source_xex),
            "profile_save_manifest_sha256": tree_manifest_sha256(storage_root),
            "route_sha256": sha256(route),
            "runtime_config_sha256": sha256(config),
            "segment_origin": (
                "clean_boot" if tree_manifest_sha256(storage_root) ==
                hashlib.sha256(b"").hexdigest() else "sealed_retail_save"
            ),
        },
        "sync": {
            "cadence": {
                "census": None,
                "integrity_level": None,
                "native_clock": None,
                "native_hz": None,
                "projection": "unique_successful_user0_poll",
                "resampling": "refuse",
                "source_hz": None,
                "status": "unqualified",
            },
            "lane_local_diagnostics": ["thread_id", "state_ptr"],
            "marker_contract": {
                "address": "821CA940",
                "code": {
                    "image_rva": "001CA940",
                    "length": 328,
                    "sha256": "a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d",
                },
                "phase": "before_input",
                "role": "ac6_frame_input_stage",
            },
            "portable_guards": [
                "marker_index", "poll_in_marker", "caller_lr", "user_index",
                "flags", "state_ptr_null",
            ],
            "primary": "poll_index",
            "telemetry": ["guest_tick", "present_index"],
        },
        "target": {
            "base_version": "v0.0.0.8",
            "entry_point": "821F5ED0",
            "media_id": "531C30BE",
            "module": "default.xex",
            "module_xxh3": "892639B654015428",
            "region_mask": "0000FDFF",
            "target_id": "ac6-ntsc-uj-default-xex",
            "title_id": "4E4D07D1",
            "xex_sha256": "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc",
            "xex_version": "v0.0.0.8",
        },
    }
    path = output / "stock-input-record.header.jsonl"
    path.write_text(
        json.dumps(header, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    path.chmod(0o444)
    return path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", default="ntsc-uj", choices=("ntsc-uj", "pal"))
    parser.add_argument("--mission-id", default=1, type=int, choices=range(1, 16))
    parser.add_argument(
        "--route", type=Path,
        help="release route below routes/; Mission 01 defaults to the qualified route",
    )
    parser.add_argument(
        "--input-replay", type=Path,
        help="strict ac6.controller-input-replay.v4 recording for this exact route",
    )
    parser.add_argument(
        "--storage-seed", type=Path,
        help="sealed storage-root produced by the preceding mission",
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--display", default="auto")
    parser.add_argument("--duration", default=900, type=int)
    parser.add_argument(
        "--cache-seed", type=Path,
        help="diagnostic-only host cache root containing cache/ and cache-root/",
    )
    parser.add_argument(
        "--mission-renderdoc-frame",
        choices=("cinematic-d5b4", "gameplay-hud"),
        help="diagnostic only: capture one named Mission 01 frame with RenderDoc",
    )
    parser.add_argument(
        "--diagnostic-route", type=Path,
        help="bounded visual-only route below routes/; does not produce a gameplay receipt",
    )
    parser.add_argument(
        "--hide-diagnostics", action="store_true",
        help="close the host graphics diagnostics window before a diagnostic route",
    )
    parser.add_argument(
        "--mission-launch-hold", action="store_true",
        help="diagnose Mission 01 without confirmations after the launch input",
    )
    parser.add_argument(
        "--mission-launch-long-press", action="store_true",
        help="diagnose Mission 01 with only its launch A held for 0.6 seconds",
    )
    parser.add_argument(
        "--mission-deploy-long-press", action="store_true",
        help="diagnose the sealed Mission 01 deploy A held for 0.6 seconds",
    )
    parser.add_argument(
        "--mission-sortie-long-press", action="store_true",
        help="diagnose the sealed deploy A and cinematic Escape held for 0.6 seconds",
    )
    parser.add_argument(
        "--mission-hangar-confirm", action="store_true",
        help="confirm A once from the first Mission 01 hangar screen",
    )
    parser.add_argument(
        "--mission-map-confirm", action="store_true",
        help="confirm A once from the tactical map reached after the hangar",
    )
    parser.add_argument(
        "--mission-cinematic-handoff", action="store_true",
        help="exercise the qualified A/Start handoff from launch cinematic to HUD",
    )
    parser.add_argument(
        "--mission-render-summary", action="store_true",
        help="capture the bounded per-frame draw/clear/resolve summary at the HUD",
    )
    parser.add_argument(
        "--mission-resolve-content", action="store_true",
        help="enable bounded final-resolve readback samples for a render-summary route",
    )
    parser.add_argument(
        "--mission-postprocess-order", action="store_true",
        help="diagnose the ordered full-screen chain on the sealed 96-step route",
    )
    parser.add_argument(
        "--mission-compose-fallback", action="store_true",
        help="diagnose presenting the last AC6 final-composition fetch instead of the sparse swap target",
    )
    parser.add_argument(
        "--mission-fsi", action="store_true",
        help="diagnose the Vulkan fragment-shader-interlock render-target path",
    )
    parser.add_argument(
        "--mission-host-render-targets", action="store_true",
        help="diagnostic only: use Vulkan host render targets instead of FSI",
    )
    parser.add_argument(
        "--mission-d5b4-final-white", action="store_true",
        help="replace only the qualified D5B4 final pixel output during the Mission 01 route",
    )
    parser.add_argument(
        "--mission-d5b4-depth-bypass", action="store_true",
        help="bypass depth/stencil only for D5B4 while retaining the final-white diagnostic",
    )
    parser.add_argument(
        "--mission-d5b4-cull-bypass", action="store_true",
        help="disable culling only for D5B4 while retaining final-white and depth bypass",
    )
    parser.add_argument(
        "--mission-stock-deswizzle", action="store_true",
        help="diagnostic only: disable the AC6 manual de-swizzle fix for one visual comparison",
    )
    parser.add_argument(
        "--mission-stock-water-gradients", action="store_true",
        help="diagnostic only: disable the AC6 hoisted water gradients for one visual comparison",
    )
    parser.add_argument(
        "--mission-stock-input-log", action="store_true",
        help="diagnostic only: log the stock XamInputGetState packet at the qualified seam",
    )
    parser.add_argument(
        "--mission-stock-input-record", action="store_true",
        help="diagnostic only: record raw stock XamInputGetState states at the qualified seam",
    )
    arguments = parser.parse_args()
    if arguments.input_replay is not None and arguments.mission_stock_input_record:
        parser.error("--input-replay and --mission-stock-input-record are mutually exclusive")
    if arguments.route is not None and arguments.diagnostic_route is not None:
        parser.error("--route and --diagnostic-route are mutually exclusive")
    if arguments.mission_renderdoc_frame and arguments.diagnostic_route is None:
        parser.error("--mission-renderdoc-frame requires --diagnostic-route")
    if arguments.mission_d5b4_final_white and not arguments.mission_render_summary:
        parser.error("--mission-d5b4-final-white requires --mission-render-summary")
    if arguments.mission_resolve_content and not arguments.mission_render_summary:
        parser.error("--mission-resolve-content requires --mission-render-summary")
    if arguments.mission_compose_fallback and not arguments.mission_render_summary:
        parser.error("--mission-compose-fallback requires --mission-render-summary")
    if arguments.mission_fsi and not arguments.mission_render_summary:
        parser.error("--mission-fsi requires --mission-render-summary")
    if arguments.mission_host_render_targets and not arguments.mission_render_summary:
        parser.error("--mission-host-render-targets requires --mission-render-summary")
    if arguments.mission_fsi and arguments.mission_host_render_targets:
        parser.error("--mission-fsi and --mission-host-render-targets are mutually exclusive")
    if arguments.mission_d5b4_depth_bypass and not arguments.mission_d5b4_final_white:
        parser.error("--mission-d5b4-depth-bypass requires --mission-d5b4-final-white")
    if arguments.mission_d5b4_cull_bypass and not arguments.mission_d5b4_depth_bypass:
        parser.error("--mission-d5b4-cull-bypass requires --mission-d5b4-depth-bypass")
    if arguments.mission_stock_deswizzle and arguments.diagnostic_route is None:
        parser.error("--mission-stock-deswizzle requires --diagnostic-route")
    if arguments.mission_stock_water_gradients and arguments.diagnostic_route is None:
        parser.error("--mission-stock-water-gradients requires --diagnostic-route")
    if arguments.target != "ntsc-uj":
        parser.error("PAL remains blocked until the US gameplay receipt passes")
    if arguments.mission_id == 1 and arguments.storage_seed is not None:
        parser.error("Mission 01 release validation must start from empty storage")
    root = PRODUCT / "build" / arguments.target
    manifest_path = root / "manifest.json"
    static_path = root / "static-validation.json"
    if not manifest_path.is_file() or not static_path.is_file():
        parser.error("prepare, build and static validation are required")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    static = json.loads(static_path.read_text(encoding="utf-8"))
    try:
        validate_static_receipt(arguments.target, manifest, static)
    except RuntimeError as error:
        parser.error(str(error))
    hook_map = manifest.get("hook_map", {})
    hook_map_path = PRODUCT / hook_map.get("path", "")
    if (
        hook_map.get("schema") != "ac6.retail-hook-map.v1"
        or not hook_map_path.is_file()
        or sha256(hook_map_path) != hook_map.get("sha256")
    ):
        parser.error("hook-map identity mismatch")
    config = root / "source/ac6recomp_config.toml"
    if (
        not config.is_file()
        or sha256(config) != manifest.get("generation", {}).get("configuration", {}).get("sha256")
    ):
        parser.error("recompilation configuration identity mismatch")
    binary = Path(static["binary"]["path"])
    iso = Path(manifest["iso"]["source_path"])
    if not binary.is_file() or sha256(binary) != static["binary"]["sha256"]:
        parser.error("validated binary identity mismatch")
    missing_markers = missing_route_sync_markers(binary)
    if missing_markers:
        parser.error(
            "qualified 96-step route is not observable in this binary; missing guest "
            "synchronization markers: " + ", ".join(missing_markers)
        )
    if not iso.is_file() or iso.stat().st_size != manifest["iso"]["size"]:
        parser.error("ISO identity/size mismatch")
    if sha256(iso) != manifest["iso"]["sha256"]:
        parser.error("ISO SHA-256 mismatch")
    source_xex = root / "source/assets/default.xex"
    if sha256(source_xex) != manifest["xex"]["sha256"]:
        parser.error("prepared XEX identity mismatch")
    mission_probe_count = sum((
        arguments.mission_launch_hold,
        arguments.mission_launch_long_press,
        arguments.mission_deploy_long_press,
        arguments.mission_sortie_long_press,
        arguments.mission_hangar_confirm,
        arguments.mission_map_confirm,
        arguments.mission_cinematic_handoff,
        arguments.mission_render_summary,
    ))
    if mission_probe_count > 1:
        parser.error("choose at most one Mission 01 launch probe")
    recording = arguments.mission_stock_input_record
    diagnostic = (
        arguments.diagnostic_route is not None
        or recording
        or arguments.mission_launch_hold
        or arguments.mission_launch_long_press
        or arguments.mission_deploy_long_press
        or arguments.mission_sortie_long_press
        or arguments.mission_hangar_confirm
        or arguments.mission_map_confirm
        or arguments.mission_cinematic_handoff
        or arguments.mission_render_summary
        or arguments.mission_postprocess_order
    )
    if arguments.hide_diagnostics and not diagnostic:
        parser.error("--hide-diagnostics is restricted to diagnostic routes")
    route = (
        arguments.diagnostic_route.resolve() if arguments.diagnostic_route else
        arguments.route.resolve() if arguments.route else
        DIAGNOSTIC_ROUTE if diagnostic else ROUTE
    )
    if arguments.diagnostic_route and route.parent != (PRODUCT / "routes").resolve():
        parser.error("diagnostic route must be a direct child of routes/")
    if arguments.route and route.parent != (PRODUCT / "routes").resolve():
        parser.error("route must be a direct child of routes/")
    if not diagnostic and route.parent != (PRODUCT / "routes").resolve():
        parser.error("release route must be a direct child of routes/")
    if arguments.mission_id > 1 and diagnostic:
        parser.error("Mission 02..15 diagnostic routes are not release campaign gates")
    if arguments.mission_id > 1 and arguments.route is None:
        parser.error("Mission 02..15 require an explicit --route")
    if arguments.mission_id > 1 and arguments.input_replay is None:
        parser.error("Mission 02..15 require a strict --input-replay")
    if arguments.mission_id > 1 and arguments.storage_seed is None:
        parser.error("Mission 02..15 require --storage-seed from the preceding mission")
    if arguments.mission_id > 1 and arguments.cache_seed is None:
        parser.error("Mission 02..15 require --cache-seed from the preceding mission")
    if arguments.mission_id == 1 and not diagnostic and arguments.cache_seed is not None:
        parser.error("Mission 01 release validation must build its cache from empty")
    output = arguments.output.resolve()
    if output.exists():
        parser.error("output must not exist")
    for tool in ("Xvfb", "xdotool", "import", "convert", "compare"):
        if shutil.which(tool) is None:
            parser.error(f"missing tool: {tool}")

    runner_module = load_runner()
    route_sources: list[Path] = []
    steps = runner_module.parse_steps(route, sources=route_sources)
    if arguments.mission_stock_input_record:
        # Recording serializes every qualified poll under one bounded mutex;
        # give the clean-boot type28 gate a larger, still finite window so the
        # diagnostic can reach the same route frontier as the stock run.
        for index, (operation, argument, limit) in enumerate(steps):
            if operation == "wait-pulse" and argument == "type28=30":
                steps[index] = (operation, argument, "Escape@600")
                break
    if arguments.mission_deploy_long_press or arguments.mission_sortie_long_press:
        if not is_sealed_route(route, steps):
            parser.error("mission-deploy probe requires the sealed 96-step route")
        # Anchor on the post-weapon-confirm capture: route edits may move the
        # operation number, but this is the one A edge on the Deploy screen.
        try:
            steps = replace_first_key_after_capture(
                steps, "post-weapon-confirm", "space", "0.6"
            )
        except RuntimeError as error:
            parser.error(str(error))
    if arguments.mission_sortie_long_press:
        # The Escape edge hands the launch cinematic to the flight HUD and
        # has the same guest input-inhibit boundary as Deploy.
        escape_indices = [
            index
            for index, step in enumerate(steps)
            if step[0] == "key" and step[1] == "Escape"
        ]
        if len(escape_indices) != 1:
            parser.error(
                "sealed cinematic-handoff route must contain exactly one Escape edge"
            )
        escape_index = escape_indices[0]
        if steps[escape_index][2] != "0.1":
            parser.error("sealed cinematic-handoff step changed unexpectedly")
        steps[escape_index] = ("key", "Escape", "0.6")
    if (
        arguments.mission_launch_hold
        or arguments.mission_launch_long_press
        or arguments.mission_hangar_confirm
        or arguments.mission_map_confirm
        or arguments.mission_cinematic_handoff
        or arguments.mission_render_summary
    ):
        if not is_sealed_route(route, steps):
            parser.error("mission-launch probe requires the sealed 96-step route")
        if arguments.mission_cinematic_handoff or arguments.mission_render_summary:
            # Start bounded Escape retries after the renderer/movie workers are
            # established, and do not send A until the guest-owned title state
            # is exact. Two successful receipts reached type28=30 before their
            # scheduled A edge; a later failed receipt stopped presenting when
            # the fixed Escape/A pair straddled the movie transition.
            steps = [
                ("sleep", "20", ""),
                # wait_log settles four seconds after every edge and stops as
                # soon as the guest reports state 30. No A can leak into an
                # unobserved transition.
                ("wait-pulse", "type28=30", "Escape@90"),
            ] + steps[4:69] + [
                # The hangar is visible at operation 69, but a confirmation
                # injected immediately after the screenshot can land inside
                # its input-inhibit boundary. Settle before the single A edge.
                ("sleep", "2", ""),
                ("key", "space", "0.6"),
                ("sleep", "15", ""), ("capture", "mission-tactical-map", ""),
                ("key", "space", "0.6"),
                ("sleep", "60", ""), ("capture", "mission-briefing", ""),
                ("key", "space", "0.6"),
                ("sleep", "45", ""), ("capture", "mission-cinematic", ""),
                # r470: step-78 "mission-cinematic" was byte-identical to
                # every later capture in every r465-r469 run (sha256-verified)
                # -- viewing it shows a static "Deploy with this selection?
                # A OK / B CANCEL" confirmation dialog, not a playing
                # cinematic. It needs one more A/space confirm, not Escape.
                ("sleep", "2", ""),
                ("key", "space", "0.6"),
                ("sleep", "10", ""), ("capture", "post-deploy-confirm-10s", ""),
                ("sleep", "30", ""), ("capture", "post-deploy-confirm-40s", ""),
                ("sync-log", "", ""),
                ("wait", r"\[ac6-visual-phase\].*cinematic=0.*world=1.*hud=1.*stable=30", "120"),
                ("capture", "gameplay-hud", ""),
            ]
        elif arguments.mission_map_confirm:
            # Operation 69 is the first hangar. Its A confirmation reaches a
            # stable tactical map which visibly requests one more A.
            steps = steps[:69] + [
                ("key", "space", "0.6"),
                ("sleep", "15", ""), ("capture", "mission-tactical-map", ""),
                ("key", "space", "0.6"),
                ("sleep", "15", ""), ("capture", "post-map-a-15s", ""),
                ("sleep", "20", ""), ("capture", "post-map-a-35s", ""),
                ("sleep", "30", ""), ("capture", "post-map-a-65s", ""),
            ]
        elif arguments.mission_hangar_confirm:
            # Operation 69 captures the first hangar screen. The sealed route
            # then presses Shift and returns to weapon selection, so neither
            # historical launch probe actually confirmed A from the hangar.
            steps = steps[:69] + [
                ("key", "space", "0.6"),
                ("sleep", "15", ""), ("capture", "post-hangar-a-15s", ""),
                ("sleep", "20", ""), ("capture", "post-hangar-a-35s", ""),
                ("sleep", "30", ""), ("capture", "post-hangar-a-65s", ""),
            ]
        else:
            # The historical probes retain their exact route prefix for
            # comparison, but operation 73 only returns from weapons to hangar.
            launch_hold = "0.6" if arguments.mission_launch_long_press else "0.1"
            steps = steps[:72] + [("key", "space", launch_hold)] + steps[73:75] + [
                ("sleep", "15", ""), ("capture", "mission-load-50s", ""),
                ("sleep", "20", ""), ("capture", "mission-load-70s", ""),
                ("sleep", "30", ""), ("capture", "mission-load-100s", ""),
            ]
    if arguments.mission_cinematic_handoff or arguments.mission_render_summary:
        # The Campaign/New Game menu can still be in its transition frame when
        # the sealed route's 0.1 s edge is sent. Reuse the one qualified
        # correction from the candidate probe: settle eight seconds and hold
        # each configuration confirmation for 0.6 s. Keep the sealed route
        # bytes and all post-campaign observables unchanged.
        campaign_capture = next(
            (index for index, (operation, argument, _) in enumerate(steps)
             if operation == "capture" and argument == "campaign-new-game"),
            None,
        )
        post_campaign_capture = next(
            (index for index, (operation, argument, _) in enumerate(steps)
             if operation == "capture" and argument == "post-campaign-intro-skip"),
            None,
        )
        if campaign_capture is None or post_campaign_capture is None:
            parser.error("cinematic handoff campaign markers missing")
        campaign_prefix = steps[:campaign_capture + 1]
        campaign_suffix = steps[post_campaign_capture:]
        steps = campaign_prefix + [
            ("sleep", "8", ""),
            ("key", "space", "0.6"),
            ("sleep", "5", ""), ("capture", "difficulty", ""),
            ("key", "space", "0.6"),
            ("sleep", "5", ""), ("capture", "controls", ""),
            ("key", "space", "0.6"),
            ("sleep", "5", ""), ("capture", "language", ""),
            ("key", "space", "0.6"),
            ("sleep", "5", ""),
            ("key", "space", "0.6"),
            ("wait-pulse", r"\[ac6-campaign-transition\].*state=1->2", "Escape@60"),
        ] + campaign_suffix
    if (not diagnostic and arguments.input_replay is None
            and not is_sealed_route(route, steps)):
        parser.error(f"route is not the qualified 96-step route: {len(steps)}")
    if not diagnostic and arguments.input_replay is not None:
        try:
            validate_full_mission_route(steps)
        except RuntimeError as error:
            parser.error(str(error))
    if diagnostic:
        if recording and arguments.route is not None:
            try:
                validate_full_mission_route(steps)
            except RuntimeError as error:
                parser.error(str(error))
        elif (
            not 1 <= len(steps) <= 96
            or sum(operation == "capture" for operation, _, _ in steps) < 2
        ):
            parser.error("diagnostic route must contain 1..96 steps and at least two captures")
    output.mkdir(parents=True)
    storage = output / "storage-root"
    cache = output / "cache-root"
    storage_seed = None
    if arguments.storage_seed is not None:
        storage_seed = arguments.storage_seed.resolve()
        try:
            validate_tree(storage_seed, "storage seed")
        except RuntimeError as error:
            parser.error(str(error))
        shutil.copytree(storage_seed, storage, symlinks=False)
    else:
        storage.mkdir()
    cache_seed = None
    if arguments.cache_seed is not None:
        cache_seed = arguments.cache_seed.resolve()
        try:
            validate_cache_seed(cache_seed)
        except RuntimeError as error:
            parser.error(str(error))
        for name in ("cache", "cache-root"):
            source = cache_seed / name
            destination = output / name
            shutil.copytree(source, destination, symlinks=False)
    cache.mkdir(exist_ok=True)
    record_header = None
    if arguments.mission_stock_input_record:
        record_header = write_stock_input_record_header(
            output, binary, config, route, source_xex, storage,
        )
    input_replay = None
    replay_header = None
    replay_header_document = None
    if arguments.input_replay is not None:
        input_replay = arguments.input_replay.resolve()
        try:
            replay_header_document, replay_header_line = load_replay_header(input_replay)
            validate_replay_header(
                replay_header_document, binary=binary, config=config, route=route,
                source_xex=source_xex, storage_root=storage,
            )
        except RuntimeError as error:
            parser.error(str(error))
        replay_header = stage_replay_header(output, replay_header_line)
    namespace = Namespace(
        binary=binary,
        iso=iso,
        output=output,
        duration=arguments.duration,
        display=runner_module.normalize_display(select_display(arguments.display)),
        storage_root=storage,
        cache_root=cache,
        unlock_fps=False,
        trace_input=None,
        unit_boundary_output=None,
        xam_movie_record=arguments.mission_stock_input_record,
        xam_movie_replay=input_replay,
        xam_movie_header=replay_header or record_header,
        seed_user_data=None,
    )

    class RetailRun(runner_module.OracleRun):
        def focus(self) -> None:
            super().focus()
            focused = subprocess.run(
                ["xdotool", "getwindowfocus"], env=self.display_env,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                check=False,
            ).stdout.strip()
            with (self.args.output / "host-input-focus.log").open("a", encoding="utf-8") as log:
                log.write(f"focus={focused}\n")

        def start(self) -> None:
            display_number = self.args.display.removeprefix(":").split(".", 1)[0]
            if not display_number.isdigit() or Path(f"/tmp/.X11-unix/X{display_number}").exists():
                raise runner_module.RunError("private X display is not available")
            self.xvfb = subprocess.Popen(
                ["Xvfb", self.args.display, "-screen", "0", "1280x720x24"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True,
            )
            time.sleep(2)
            if self.xvfb.poll() is not None:
                raise runner_module.RunError("Xvfb startup failed")
            command = [
                str(self.args.binary), str(self.args.iso), "--mnk_mode=true",
                "--ac6_graphics_backend=vulkan", "--ac6_native_graphics_enabled=true",
                "--ac6_graphics_mode=hybrid_backend_fixes", "--ac6_performance_mode=false",
                "--ac6_unlock_fps=false", "--ac6_dynamic_vblank=false",
                "--async_shader_compilation=false", "--resolution_scale=1",
                "--draw_resolution_scale_x=1", "--draw_resolution_scale_y=1",
                "--ac6_terrain_hd=false", "--ac6_fullres_effects=false",
                "--ac6_widescreen=false", "--ac6_texture_swaps_enabled=false",
                "--log_flush_interval=1", "--log_max_file_size_mb=128",
                f"--log_file={self.log_path}",
                f"--user_data_root={self.args.storage_root}",
            ]
            if arguments.mission_stock_deswizzle:
                # Keep this as a command-line diagnostic override: the
                # validated binary and its manifest remain unchanged, while a
                # fresh shader cache makes the comparison deterministic.
                command.append("--ac6_fix_deswizzle=false")
            if arguments.mission_stock_water_gradients:
                command.append("--ac6_fix_water_line=false")
            if arguments.mission_stock_input_log:
                # Read-only seam evidence: retain ReXGlue's stock MnK driver
                # and log the post-driver XamInputGetState packet. This does
                # not enable the Windows-only AC6 keyboard injector.
                command.append("--ac6_kbm_log=true")
            if self.args.xam_movie_record:
                command.extend([
                    f"--ac6_oracle_controller_header_path={self.args.xam_movie_header}",
                    f"--ac6_oracle_controller_record_path={self.args.output / 'stock-input-record.jsonl'}",
                ])
            if self.args.xam_movie_replay is not None:
                command.extend([
                    f"--ac6_oracle_controller_header_path={self.args.xam_movie_header}",
                    f"--ac6_oracle_controller_replay_path={self.args.xam_movie_replay}",
                ])
            if arguments.mission_render_summary:
                command.extend([
                    "--ac6_render_capture=true",
                    "--ac6_backend_log_signatures=true",
                    "--ac6_log_frontier_passes=true",
                    "--ac6_log_resolve_info=true",
                    "--ac6_log_swap_texture=true",
                    "--ac6_log_texture_load=true",
                    "--ac6_log_world_submission_owner=true",
                    "--ac6_log_campaign_service_owner=true",
                    "--ac6_log_rt_transfers=true",
                    "--ac6_log_composition_textures=true",
                    "--ac6_graphics_diagnostics=false",
                ])
                if arguments.mission_resolve_content:
                    command.extend([
                        "--readback_resolve=fast",
                        "--ac6_log_resolve_content=true",
                    ])
                if arguments.mission_compose_fallback:
                    command.append("--ac6_present_compose_fallback=true")
                if arguments.mission_fsi:
                    command.append("--render_target_path_vulkan=fsi")
                if arguments.mission_host_render_targets:
                    command.append("--render_target_path_vulkan=fbo")
            if arguments.mission_postprocess_order:
                command.extend([
                    "--readback_resolve=fast",
                    "--ac6_log_postprocess_order=true",
                    "--ac6_graphics_diagnostics=false",
                ])
            if arguments.mission_renderdoc_frame:
                command.extend([
                    "--ac6_graphics_diagnostics=false",
                    "--renderdoc_capture_all_activity=false",
                ])
                renderdoc_output = self.args.output / "renderdoc"
                renderdoc_output.mkdir()
                command = [
                    "renderdoccmd", "capture", "--wait-for-exit",
                    "--opt-disallow-vsync",
                    "--working-dir", str(self.args.output),
                    "--capture-file", str(renderdoc_output / arguments.mission_renderdoc_frame),
                ] + command
            if arguments.mission_d5b4_final_white:
                command.extend([
                    "--ac6_d5b4_final_white=true",
                    f"--dump_shaders={self.args.output / 'shader-dump'}",
                ])
            if arguments.mission_d5b4_depth_bypass:
                command.append("--ac6_d5b4_depth_stencil_bypass=true")
            if arguments.mission_d5b4_cull_bypass:
                command.append("--ac6_d5b4_cull_bypass=true")
            self.console = self.console_path.open("wb")
            self.game = subprocess.Popen(
                command, cwd=self.args.output, env=self.display_env,
                stdout=self.console, stderr=subprocess.STDOUT, start_new_session=True,
            )

        def close_graphics_diagnostics(self) -> None:
            # The fixed 1280x720 diagnostic harness opens this host-only ImGui
            # window at (60, 60), with its close button at (519, 68). This is
            # a visual-audit convenience only; guest input remains focused.
            self.focus()
            self.sleep(2)
            subprocess.run(
                ["xdotool", "mousemove", "519", "68", "click", "1",
                 "mousemove", "1279", "719"],
                env=self.display_env, check=True,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            )
            self.sleep(1)
            self.focus()

        def capture(self, label: str) -> None:
            if arguments.hide_diagnostics and not getattr(
                self, "graphics_diagnostics_closed", False
            ):
                # Defer the click until the route reaches its first capture:
                # the preceding route predicate proves that ImGui has drawn
                # the close button. A startup-time click can race it.
                self.close_graphics_diagnostics()
                self.graphics_diagnostics_closed = True
            renderdoc_label = {
                "cinematic-d5b4": "cinematic-view-2",
                "gameplay-hud": "gameplay-hud",
            }.get(arguments.mission_renderdoc_frame)
            if label == renderdoc_label:
                self.focus()
                subprocess.run(
                    # RenderDoc documents F12 as the portable capture key.
                    # Print Screen is window-manager/keysym dependent under
                    # Xvfb, so use F12 for the bounded Linux diagnostic.
                    ["xdotool", "key", "F12"], env=self.display_env, check=True,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                )
                self.sleep(3)
            super().capture(label)

        def execute(self, replay_steps: list[tuple[str, str, str]]) -> None:
            if self.args.xam_movie_replay is None:
                super().execute(replay_steps)
                return
            # The replay owns guest input. Route keys are timing landmarks;
            # waits and captures remain host-side deterministic observables.
            for operation, argument, limit in replay_steps:
                self.executed_steps += 1
                if operation == "sleep":
                    self.sleep(float(argument))
                elif operation in {"key", "mouse"}:
                    continue
                elif operation == "capture":
                    if limit:
                        self.sleep(float(limit))
                    self.capture(argument)
                elif operation == "wait":
                    self.wait_log(argument, float(limit))
                elif operation == "wait-pulse":
                    timeout = self.deadline - time.monotonic()
                    if "@" in limit:
                        _, timeout_text = limit.rsplit("@", 1)
                        timeout = min(timeout, float(timeout_text))
                    self.wait_log(argument, timeout)
                elif operation == "present":
                    start = self.present_count()
                    target = start + int(argument)
                    end = min(self.deadline, time.monotonic() + float(limit))
                    while self.present_count() < target and time.monotonic() < end:
                        self.sleep(0.2)
                    if self.present_count() < target:
                        raise runner_module.RunError("presentation predicate not reached")
                else:
                    raise runner_module.RunError(
                        f"unsupported release replay operation: {operation}"
                    )

        def request_clean_close(self) -> bool:
            if self.game is None or self.game.poll() is not None:
                return self.game is not None and self.game.returncode == 0
            result = subprocess.run(
                ["xdotool", "search", "--classname", "ac6recomp"],
                env=self.display_env, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, check=False,
            )
            windows = result.stdout.split()
            if windows:
                subprocess.run(
                    ["xdotool", "windowclose", windows[-1]], env=self.display_env,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
                )
            try:
                return self.game.wait(timeout=30) == 0
            except subprocess.TimeoutExpired:
                return False

    before = runner_module.shm_inventory()
    run = RetailRun(namespace)
    error = ""
    clean_shutdown = False
    started = time.time()
    try:
        run.start()
        run.execute(steps)
        clean_shutdown = run.request_clean_close()
    except (runner_module.RunError, OSError, subprocess.SubprocessError, ValueError) as caught:
        error = str(caught)
        if diagnostic and run.game is not None and run.game.poll() is None:
            try:
                runner_module.OracleRun.capture(run, "failure-observation")
            except (OSError, subprocess.SubprocessError):
                pass
    finally:
        game_status, xvfb_status = run.close()
        runner_module.cleanup_owned_shm(before)

    console = run.console_path.read_text(encoding="utf-8", errors="replace") \
        if run.console_path.is_file() else ""
    log = run.log_path.read_text(encoding="utf-8", errors="replace") \
        if run.log_path.is_file() else ""
    runtime_text = console + "\n" + log
    fatal = sorted(
        set(runner_module.FATAL.findall(runtime_text))
        | set(AUDIO_FATAL.findall(runtime_text))
    )
    captures = {entry["path"].split("-", 2)[-1].removesuffix(".png"): output / entry["path"]
                for entry in run.captures}
    route_capture_names = [argument for operation, argument, _ in steps if operation == "capture"]
    missing = [name for name in route_capture_names if name not in captures]
    replay_loaded = "AC6 controller raw v4 replay loaded before guest launch" in runtime_text
    replay_finalized = re.search(
        r"AC6 controller replay finalized polls=(\d+) markers=(\d+)", runtime_text
    )
    replay_complete = (
        input_replay is None or
        (replay_loaded and replay_finalized is not None)
    )
    post_mission_marker = "[ac6-post-mission] task=InterMissionSelect"
    post_mission_index = runtime_text.rfind(post_mission_marker)
    post_mission_text = (
        runtime_text[post_mission_index:] if post_mission_index >= 0 else ""
    )
    level_set_lines = re.findall(
        r"\[ac6-current-level-set\][^\r\n]*", post_mission_text
    )
    save_lines = re.findall(r"\[ac6-save-manager\][^\r\n]*", post_mission_text)
    progression = {
        "debrief_capture": "debrief" in captures,
        "post_mission_capture": "post-mission" in captures,
        "intermission_entered": post_mission_index >= 0,
        "level_set_lines": level_set_lines,
        "save_lines": save_lines,
        "save_completed_after_intermission": any(
            re.search(r"op30=\d+->0", line) and "flag34=0->1" in line
            for line in save_lines
        ),
    }
    mission_handoff_progressed = True
    if arguments.mission_cinematic_handoff or arguments.mission_render_summary:
        if "post-weapon-confirm" in captures and "mission-tactical-map" in captures:
            mission_handoff_progressed = changed_pixels(
                captures["post-weapon-confirm"], captures["mission-tactical-map"]
            ) > 0
        else:
            mission_handoff_progressed = False
    center_metrics: dict[str, float] = {}
    effects: dict[str, int] = {}
    visual_errors: list[str] = []
    if not diagnostic and not missing:
        visual_phases = re.findall(r"\[ac6-visual-phase\][^\r\n]*", runtime_text)
        center_metrics, effects, visual_errors = validate_gameplay_visuals(
            captures, visual_phases
        )
    capture_ready = (
        not error and run.executed_steps == len(steps) and not missing and not fatal
        and clean_shutdown and mission_handoff_progressed and not visual_errors
        and replay_complete
    )
    renderdoc_captures = sorted(str(path.relative_to(output))
                                for path in output.glob("renderdoc/*.rdc"))
    if arguments.mission_renderdoc_frame and len(renderdoc_captures) != 1:
        capture_ready = False
        visual_errors.append("RenderDoc diagnostic did not produce exactly one .rdc")
    result = {
        "schema": (
            "ac6.retail-route-diagnostic.v1" if diagnostic
            else GAMEPLAY_SCHEMA
        ),
        "status": (
            "diagnostic-capture-ready" if diagnostic and capture_ready
            else "capture-ready" if capture_ready else "fail"
        ),
        "target": arguments.target,
        "mission_id": arguments.mission_id,
        "execution_mode": "strict-replay" if input_replay is not None else "host-route",
        "manifest": manifest,
        "binary": static["binary"],
        "route": {"path": str(route.relative_to(PRODUCT)), "sha256": sha256(route),
                  "steps": len(steps),
                  "executed_steps": run.executed_steps},
        "duration_seconds": time.time() - started,
        "world_center_metrics": center_metrics,
        "control_changed_pixels": effects,
        "progression": progression,
        "fatal_matches": fatal,
        "clean_shutdown": clean_shutdown,
        "game_status": game_status,
        "xvfb_status": xvfb_status,
        "error": error or "; ".join(visual_errors) or (
            "missing captures: " + ", ".join(missing) if missing else
            "mission handoff did not leave first hangar" if not mission_handoff_progressed else
            "strict replay did not load and finalize" if not replay_complete else ""
        ),
        "captures": run.captures,
        "renderdoc_captures": renderdoc_captures,
        "cache_seed": str(cache_seed) if cache_seed is not None else None,
        "cache": {
            "cold_start": cache_seed is None,
            "seed": str(cache_seed) if cache_seed is not None else None,
        },
        "storage": {
            "seed": str(storage_seed) if storage_seed is not None else None,
            "seed_manifest_sha256": (
                tree_manifest_sha256(storage_seed) if storage_seed is not None else None
            ),
            "output": tree_summary(storage),
        },
    }
    if input_replay is not None:
        assert replay_header_document is not None
        result["input_replay"] = {
            "path": str(input_replay),
            "sha256": sha256(input_replay),
            "header": replay_header_document,
            "loaded": replay_loaded,
            "finalized": replay_finalized is not None,
            "polls": int(replay_finalized.group(1)) if replay_finalized else None,
            "markers": int(replay_finalized.group(2)) if replay_finalized else None,
        }
    if arguments.mission_stock_input_record:
        record_path = output / "stock-input-record.jsonl"
        result["stock_input_record"] = {
            "path": str(record_path.relative_to(output)),
            "exists": record_path.is_file(),
            "bytes": record_path.stat().st_size if record_path.is_file() else 0,
            "sha256": sha256(record_path) if record_path.is_file() else None,
        }
    (output / "RESULT.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(output / "RESULT.json")
    return 0 if capture_ready else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"run_gate: {error}", file=sys.stderr)
        raise SystemExit(2)
