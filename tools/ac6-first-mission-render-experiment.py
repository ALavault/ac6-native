#!/usr/bin/env python3
"""Run one bounded AC6 Mission 01 render experiment and write its manifest.

This is an observation harness.  It never copies retail containers and it does
not alter the guest state.  The authoritative visible path is the configured
ReXGlue/Xenia Vulkan runner; native renderer modes are intentionally not
selected here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BINARY = Path(
    "/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/"
    "ac6-gapfill/out/build/linux-bridge-relwithdebinfo/ac6recomp"
)
DEFAULT_RUNNER = Path(
    "/fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/"
    "ac6-gapfill/tools/ac6-run.sh"
)
DEFAULT_STEP_FILE = ROOT / "scripts/ac6-first-mission-bridge-airborne-probe.steps"

CORPUS = {
    "default.xex": ROOT / "game-files/default.xex",
    "DATA.TBL": ROOT / "game-files/DATA.TBL",
}

BASELINE_CVARS = {
    "ac6_performance_mode": "false",
    "log_level": "debug",
    "ac6_unlock_fps": "false",
    "ac6_render_capture": "true",
    "ac6_log_frontier_draws": "true",
    "ac6_backend_signature_diagnostics": "true",
    "ac6_backend_log_signatures": "false",
    "ac6_d3d_trace": "false",
    "resolution_scale": "1",
    "draw_resolution_scale_x": "1",
    "draw_resolution_scale_y": "1",
    "direct_host_resolve": "false",
    "ac6_log_ui_dispatch": "true",
    "ac6_log_gameplay_state": "true",
    "ac6_log_d5b4_constants": "true",
    "ac6_backend_debug_swap": "true",
}


def sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_capture(command: list[str], env: dict[str, str], out: Path) -> dict[str, Any]:
    started = time.time()
    result = subprocess.run(command, env=env, cwd=ROOT, check=False)
    return {
        "command": [shlex.join(command)],
        "returncode": result.returncode,
        "started_unix": started,
        "finished_unix": time.time(),
        "duration_seconds": time.time() - started,
        "out": str(out),
    }


def git_value(*args: str) -> str | None:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL, check=False,
    )
    value = result.stdout.strip()
    return value or None


def command_sha256(command: list[str]) -> str | None:
    result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL, check=False)
    if result.returncode != 0:
        return None
    return hashlib.sha256(result.stdout).hexdigest()


def vulkan_summary() -> str | None:
    result = subprocess.run(
        ["vulkaninfo", "--summary"], text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    # Keep the manifest bounded even on machines with verbose loader output.
    return "\n".join(result.stdout.splitlines()[:80])


def build_manifest(args: argparse.Namespace, out: Path, command: list[str]) -> dict[str, Any]:
    return {
        "schema": "ac6.render-experiment.v1",
        "run_id": args.run_id,
        "lane": args.lane,
        "authoritative_renderer": "Vulkan RexGlue/Xenia backend",
        "native_renderer_selected": False,
        "repo": {
            "root": str(ROOT),
            "head": git_value("rev-parse", "HEAD"),
            "diff_hash": command_sha256(["git", "diff", "--no-ext-diff", "--binary"]),
            "index_diff_hash": command_sha256(["git", "diff", "--cached", "--no-ext-diff", "--binary"]),
            "branch": git_value("branch", "--show-current"),
        },
        "binary": {
            "path": str(args.binary),
            "sha256": sha256(args.binary),
        },
        "corpus": {name: {"path": str(path), "sha256": sha256(path)} for name, path in CORPUS.items()},
        "cvars": dict(args.cvars),
        "environment": {
            "DISPLAY": args.display,
            "SDL_AUDIODRIVER": "dummy",
            "VK_ICD_FILENAMES": os.environ.get("VK_ICD_FILENAMES", ""),
            "gpu_summary": vulkan_summary(),
            "platform": platform.platform(),
            "python": platform.python_version(),
        },
        "probes": {
            "step_file": str(args.step_file),
            "capture_at": 0,
            "wait_for": "type28=30",
            "wait_pulse": "Escape+space:0.1:2",
            "bounded": True,
        },
        "command": [shlex.join(command)],
        "checkpoints": {
            "C0": "last correct hangar capture; assigned by post-run report",
            "C1": "first stable cinematic aircraft capture; assigned by post-run report",
            "C2": "last cinematic capture; assigned by post-run report",
            "C3": "first gameplay HSM transition; log-qualified",
            "C4": "first UpObj/UpCam gameplay tick; log-qualified",
            "C5": "first stable partial HUD/black-world capture",
            "C6": "stable gameplay capture after >=60 ticks",
            "C7": "capture after qualified analog input",
        },
        "retail_payload_policy": "No PAC/XEX/container bytes are copied into the run manifest.",
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    parser.add_argument("--runner", type=Path, default=DEFAULT_RUNNER)
    parser.add_argument("--step-file", type=Path, default=DEFAULT_STEP_FILE)
    parser.add_argument("--display", default=":120")
    parser.add_argument("--duration", type=int, default=900)
    parser.add_argument("--lane", choices=("stock", "observe", "bridge"), default="observe")
    parser.add_argument("--user-data-root", type=Path)
    parser.add_argument("--cvar", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.out = args.out.resolve()
    args.binary = args.binary.resolve()
    args.runner = args.runner.resolve()
    args.step_file = args.step_file.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    if args.user_data_root is None:
        args.user_data_root = args.out / "user-data"
    args.user_data_root = args.user_data_root.resolve()
    args.user_data_root.mkdir(parents=True, exist_ok=True)

    args.cvars = dict(BASELINE_CVARS)
    for item in args.cvar:
        if "=" not in item:
            raise SystemExit(f"invalid --cvar (expected NAME=VALUE): {item}")
        name, value = item.split("=", 1)
        args.cvars[name] = value

    command = [
        str(args.runner),
        "--out", str(args.out),
        "--duration", str(args.duration),
        "--display", args.display,
        "--binary", str(args.binary),
        "--capture-at", "0",
        "--startup-timeout", "120",
        "--keys", "0:Escape:0.1,2:space:0.1",
        "--wait-for", "type28=30",
        "--wait-pulse", "Escape+space:0.1:2",
        "--wait-stall-timeout", "60",
        "--step-file", str(args.step_file),
        "--",
    ]
    command.extend(f"--{name}={value}" for name, value in args.cvars.items())
    command.append(f"--user_data_root={args.user_data_root}")

    manifest = build_manifest(args, args.out, command)
    manifest["dry_run"] = args.dry_run
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if args.dry_run:
        print(json.dumps(manifest, indent=2))
        return 0

    env = os.environ.copy()
    env["SDL_AUDIODRIVER"] = "dummy"
    env["DISPLAY"] = args.display
    run_result = run_capture(command, env, args.out)
    manifest["execution"] = run_result
    captures = []
    for path in sorted(args.out.glob("*.png")):
        captures.append({"path": str(path), "sha256": sha256(path), "bytes": path.stat().st_size})
    manifest["captures"] = captures
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return 0 if run_result["returncode"] == 0 else run_result["returncode"]


if __name__ == "__main__":
    raise SystemExit(main())
