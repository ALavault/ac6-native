#!/usr/bin/env python3
"""Run a bounded NTSC-U/J save-create then save-load experiment.

The experiment is deliberately separate from the Mission 01 visual gate.  It
uses the same binary, ISO, Vulkan configuration and keyboard route, while the
two processes share only the explicitly selected user-data directory.
"""

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
RUNNER_PATH = WORKSPACE / "tools/ac6-oracle-run.py"
ROUTE = PRODUCT / "routes/mission01-qualified-96.steps"
CREATE_STEPS = 38
FATAL = re.compile(
    r"REX_FATAL|Unresolved branch|ac6-oracle-indirect-miss|"
    r"ac6-oracle-host-trap|Unhandled SIGSEGV",
    re.IGNORECASE,
)
CURRENT_LEVEL = re.compile(
    r"\[ac6-current-level\][^\r\n]*\bselector=(\d+)\s+value=(\d+)\b"
)


def load_runner():
    sys.path.insert(0, str(WORKSPACE / "tools"))
    spec = importlib.util.spec_from_file_location("ac6_save_oracle_runner", RUNNER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("save runner cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_manifest(root: Path) -> dict[str, object]:
    files = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise RuntimeError(f"save storage contains symlink: {path}")
        if path.is_file():
            files.append({
                "path": path.relative_to(root).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            })
    return {"root": str(root), "files": files}


def save_container_summary(root: Path) -> dict[str, object]:
    """Describe the AC6 save container without interpreting mission payloads."""
    candidates = sorted(
        path for path in root.rglob("save.dat")
        if path.is_file() and path.parent.name == "sav_acecombat6"
    )
    if len(candidates) != 1:
        return {
            "valid": False,
            "reason": f"expected one save.dat, found {len(candidates)}",
            "path": None,
        }
    path = candidates[0]
    header = path.read_bytes()[:28]
    magic = b"\0\0\0S\0\0\0A\0\0\0V\0\0\0E"
    sentinel = b"\xfe\xfe\xfe\xfe"
    valid = (
        len(header) == 28
        and header[:16] == magic
        and int.from_bytes(header[16:20], "big") == 6
        and header[20:24] == b"\0\0\0\0"
        and header[24:28] == sentinel
    )
    return {
        "valid": valid,
        "path": str(path.relative_to(root)),
        "bytes": path.stat().st_size,
        "magic": "SAVE" if header[:16] == magic else None,
        "version": int.from_bytes(header[16:20], "big") if len(header) >= 20 else None,
        "sentinel": "0xFEFE" if header[24:28] == sentinel else None,
        "reason": "" if valid else "invalid SAVE header",
    }


def current_level_observations(runtime_text: str) -> list[dict[str, int]]:
    """Return bounded read-only current-level getter observations."""
    return [
        {"selector": int(selector), "value": int(value)}
        for selector, value in CURRENT_LEVEL.findall(runtime_text)
    ]


def phase_steps(runner_module, phase: str) -> list[tuple[str, str, str]]:
    all_steps = runner_module.parse_steps(ROUTE)
    if phase == "create":
        # The qualified route has already synchronized every asynchronous save
        # state.  Stop after the slot-create confirmation and let the file
        # writes settle before closing the process.
        return all_steps[:CREATE_STEPS] + [("sleep", "5", "")]
    if phase == "load":
        # An existing user-data tree takes a different title path from a
        # fresh profile: it publishes selector44=3 directly. Synchronize on
        # that read-only marker, then use the qualified existing-save suffix.
        # The two Left edges and one-second settles are intentional: the
        # dialog first exposes the selected slot and only then accepts the
        # affirmative load choice. No guest state is forced by this route.
        return [
            ("key", "Escape", "0.1"),
            ("sleep", "2", ""),
            ("key", "space", "0.1"),
            ("wait-pulse", "selector44=3", "Escape+space@120"),
            ("capture", "existing-save-browser", ""),
            ("key", "space", "0.1"),
            ("wait", "type28=[^ ]*6", "20"),
            ("capture", "existing-save-type6", ""),
            ("sleep", "1", ""),
            ("key", "Left", "0.1"),
            ("sleep", "1", ""),
            ("key", "space", "0.1"),
            ("wait", "type28=[^ ]*8", "20"),
            ("key", "space", "0.1"),
            ("wait", "type28=[^ ]*10", "20"),
            ("capture", "existing-save-loaded", ""),
            ("capture", "save-load-complete", ""),
            ("sleep", "5", ""),
        ]
    raise ValueError(f"unknown phase: {phase}")


class RetailSaveRun:
    """Small NTSC-U/J adapter around the shared route runner."""

    def __init__(self, runner_module, arguments: Namespace):
        self.runner_module = runner_module
        self.args = arguments
        self.run = runner_module.OracleRun(arguments)

    @property
    def log_path(self) -> Path:
        return self.args.output / "ac6recomp.log"

    @property
    def console_path(self) -> Path:
        return self.args.output / "console.log"

    def start(self) -> None:
        run = self.run
        display_number = self.args.display.removeprefix(":").split(".", 1)[0]
        if (not display_number.isdigit() or
                Path(f"/tmp/.X11-unix/X{display_number}").exists()):
            raise self.runner_module.RunError("private X display is not available")
        run.xvfb = subprocess.Popen(
            ["Xvfb", self.args.display, "-screen", "0", "1280x720x24"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        time.sleep(2)
        if run.xvfb.poll() is not None:
            raise self.runner_module.RunError("Xvfb startup failed")
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
        run.console = self.console_path.open("wb")
        run.game = subprocess.Popen(
            command, cwd=self.args.output, env=run.display_env,
            stdout=run.console, stderr=subprocess.STDOUT, start_new_session=True,
        )

    def request_clean_close(self) -> bool:
        run = self.run
        if run.game is None or run.game.poll() is not None:
            return run.game is not None and run.game.returncode == 0
        windows = subprocess.run(
            ["xdotool", "search", "--classname", "ac6recomp"],
            env=run.display_env, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, check=False,
        ).stdout.split()
        if windows:
            subprocess.run(
                ["xdotool", "windowclose", windows[-1]], env=run.display_env,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
            )
        try:
            return run.game.wait(timeout=30) == 0
        except subprocess.TimeoutExpired:
            return False

    def close(self) -> tuple[int | None, int | None]:
        return self.run.close()


def run_phase(
    runner_module, phase: str, binary: Path, iso: Path, output: Path,
    storage_root: Path, display: str, duration: int,
    expected_level: int | None = None,
) -> dict[str, object]:
    output.mkdir(parents=True)
    cache_root = output / "cache-root"
    cache_root.mkdir()
    arguments = Namespace(
        binary=binary,
        iso=iso,
        route=ROUTE,
        output=output,
        duration=duration,
        display=display,
        storage_root=storage_root,
        cache_root=cache_root,
        unlock_fps=False,
        trace_input=None,
        unit_boundary_output=None,
        xam_movie_record=False,
        xam_movie_replay=None,
    )
    run = RetailSaveRun(runner_module, arguments)
    steps = phase_steps(runner_module, phase)
    started = time.time()
    error = ""
    clean_shutdown = False
    before = runner_module.shm_inventory()
    try:
        run.start()
        run.run.execute(steps)
        clean_shutdown = run.request_clean_close()
    except (runner_module.RunError, OSError, subprocess.SubprocessError, ValueError) as caught:
        error = str(caught)
    finally:
        game_status, xvfb_status = run.close()
        runner_module.cleanup_owned_shm(before)
    console = run.console_path.read_text(encoding="utf-8", errors="replace") \
        if run.console_path.is_file() else ""
    log = run.log_path.read_text(encoding="utf-8", errors="replace") \
        if run.log_path.is_file() else ""
    runtime_text = console + "\n" + log
    fatal = sorted(set(FATAL.findall(runtime_text)))
    storage = tree_manifest(storage_root)
    save_container = save_container_summary(storage_root)
    level_observations = current_level_observations(runtime_text)
    level_verified = (
        expected_level is None
        or any(
            observation["selector"] == 1
            and observation["value"] == expected_level
            for observation in level_observations
        )
    )
    route_complete = run.run.executed_steps == len(steps)
    # The retail process occasionally keeps a movie worker alive after it has
    # handled WM_DELETE, so the harness may need the runner's bounded cleanup
    # path (game_status=-9).  That teardown detail is recorded below; it must
    # not hide a completed save transition or a load transition with no fatal.
    if not save_container["valid"] and not error:
        error = str(save_container["reason"])
    if not level_verified and not error:
        error = f"current level {expected_level} was not observed after load"
    phase_success = (
        not error and not fatal and route_complete and clean_shutdown
        and game_status == 0 and xvfb_status == 0 and save_container["valid"]
        and level_verified
    )
    result = {
        "schema": "ac6.retail-save-phase.v1",
        "phase": phase,
        "status": "pass" if phase_success else "fail",
        "binary": {"path": str(binary), "sha256": sha256(binary)},
        "iso": {"path": str(iso), "sha256": sha256(iso), "bytes": iso.stat().st_size},
        "route": {"path": str(ROUTE), "sha256": sha256(ROUTE),
                  "steps": len(steps), "executed_steps": run.run.executed_steps},
        "duration_seconds": time.time() - started,
        "display": display,
        "audio_driver": "dummy",
        "game_status": game_status,
        "xvfb_status": xvfb_status,
        "clean_shutdown": clean_shutdown,
        "error": error,
        "fatal_matches": fatal,
        "captures": run.run.captures,
        "guest_milestones": run.run.guest_milestones,
        "save_storage": storage,
        "save_container": save_container,
        "expected_current_level": expected_level,
        "current_level_observations": level_observations,
        "current_level_verified": level_verified,
        "route_complete": route_complete,
        "save_markers": sorted(set(re.findall(r"\[ac6-save-[^]]+\][^\r\n]*", runtime_text))),
    }
    (output / "RESULT.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output / "status").write_text(result["status"] + "\n", encoding="utf-8")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--iso", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--display", default=":170")
    parser.add_argument("--duration", type=int, default=240)
    parser.add_argument(
        "--load-only", action="store_true",
        help="run only the load phase from an existing storage seed",
    )
    parser.add_argument(
        "--storage-seed", type=Path, default=None,
        help="regular storage tree used by --load-only",
    )
    parser.add_argument(
        "--expected-level", type=int, choices=range(1, 16),
        help="require the load process to read this campaign level (load-only)",
    )
    arguments = parser.parse_args()
    binary = arguments.binary.resolve()
    iso = arguments.iso.resolve()
    output = arguments.output.resolve()
    if output.exists():
        parser.error("output must not exist")
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error("binary is not executable")
    if not iso.is_file():
        parser.error("ISO is not a regular file")
    if not 1 <= arguments.duration <= 3600:
        parser.error("duration outside bounds")
    if arguments.load_only and arguments.storage_seed is None:
        parser.error("--load-only requires --storage-seed")
    if arguments.expected_level is not None and not arguments.load_only:
        parser.error("--expected-level requires --load-only")
    if arguments.storage_seed is not None:
        arguments.storage_seed = arguments.storage_seed.resolve()
        if (not arguments.storage_seed.is_dir() or arguments.storage_seed.is_symlink()):
            parser.error("storage seed must be a regular directory")
    runner_module = load_runner()
    storage_root = output / "storage-root"
    storage_root.mkdir(parents=True)
    if arguments.load_only:
        # Validate the complete seed before copying it.  In particular, do not
        # let copytree follow a symlink out of the explicitly supplied tree.
        tree_manifest(arguments.storage_seed)
        shutil.copytree(arguments.storage_seed, storage_root, dirs_exist_ok=True)
        load = run_phase(
            runner_module, "load", binary, iso, output / "load",
            storage_root, arguments.display, arguments.duration,
            arguments.expected_level,
        )
        result = {
            "schema": "ac6.retail-save-experiment.v1",
            "status": load["status"],
            "create": None,
            "load": load,
            "storage_seed": str(arguments.storage_seed),
            "storage": tree_manifest(storage_root),
        }
        (output / "RESULT.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        (output / "status").write_text(result["status"] + "\n", encoding="utf-8")
        return 0 if result["status"] == "pass" else 2
    create = run_phase(
        runner_module, "create", binary, iso, output / "create",
        storage_root, arguments.display, arguments.duration,
    )
    if create["status"] != "pass":
        (output / "RESULT.json").write_text(
            json.dumps({"schema": "ac6.retail-save-experiment.v1", "status": "fail",
                        "create": create}, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return 2
    load = run_phase(
        runner_module, "load", binary, iso, output / "load",
        storage_root, arguments.display, arguments.duration,
    )
    result = {
        "schema": "ac6.retail-save-experiment.v1",
        "status": "pass" if load["status"] == "pass" else "fail",
        "create": create,
        "load": load,
        "storage": tree_manifest(storage_root),
    }
    (output / "RESULT.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (output / "status").write_text(result["status"] + "\n", encoding="utf-8")
    return 0 if result["status"] == "pass" else 2


if __name__ == "__main__":
    raise SystemExit(main())
