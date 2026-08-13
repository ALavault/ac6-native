#!/usr/bin/env python3
"""Verify and run the pinned AC6 NTSC-U/J ReXGlue Linux/Vulkan oracle."""
from __future__ import annotations

import argparse
import ctypes
import fcntl
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = (
    ROOT
    / "analysis/oracle/ac6-recomp-ab90b-us/patches/"
    "linux-vulkan-minimal-v1.json"
)
PRESENT_RE = re.compile(r"AC6_ORACLE_PRESENT index=([0-9]+)")
POLL_RE = re.compile(r"AC6_ORACLE_PHYSICAL_POLL index=([0-9]+)")
FATAL_RE = re.compile(r"REX_FATAL|Unhandled SIGSEGV|Failed to submit", re.I)


class OracleError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tree_sha256(root: Path) -> tuple[int, int, str]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    digest = hashlib.sha256()
    byte_count = 0
    for path in files:
        if path.is_symlink():
            raise OracleError(f"generated symlink: {path}")
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        payload = path.read_bytes()
        byte_count += len(payload)
        digest.update(payload)
        digest.update(b"\0")
    return len(files), byte_count, digest.hexdigest()


def xxh3(path: Path) -> str:
    library = ctypes.CDLL("libxxhash.so.0")
    function = library.XXH3_64bits
    function.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    function.restype = ctypes.c_uint64
    payload = path.read_bytes()
    buffer = ctypes.create_string_buffer(payload, len(payload))
    return f"{function(buffer, len(payload)):016x}"


def git(checkout: Path, *arguments: str) -> str:
    return subprocess.run(
        ["git", "-C", str(checkout), *arguments],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout.strip()


def cmake_cache(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key = key_type.split(":", 1)[0]
        values[key] = value
    return values


def load_manifest(path: Path = DEFAULT_MANIFEST) -> dict[str, object]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("schema") != "ac6.recomp-linux-minimal-patch.v1":
        raise OracleError("Linux oracle manifest schema")
    return document


def verify_lineage(
    checkout: Path, build_dir: Path, content_dir: Path,
    manifest_path: Path = DEFAULT_MANIFEST,
) -> dict[str, object]:
    document = load_manifest(manifest_path)
    base = document["base"]
    target = document["target"]
    build = document["build"]
    if git(checkout, "rev-parse", "HEAD") != base["commit"]:
        raise OracleError("oracle commit mismatch")
    if git(checkout, "rev-parse", "HEAD^{tree}") != base["tree"]:
        raise OracleError("oracle tree mismatch")
    if git(checkout, "rev-parse", "HEAD:thirdparty/rexglue-sdk") != base["rexglue_tree"]:
        raise OracleError("ReXGlue tree mismatch")

    for record in document["patch_stack"]:
        path = ROOT / record["path"]
        if sha256(path) != record["sha256"]:
            raise OracleError(f"patch identity mismatch: {record['path']}")
    for relative, expected in document["source_files"].items():
        path = checkout / relative
        if not path.is_file() or sha256(path) != expected:
            raise OracleError(f"patched source mismatch: {relative}")

    xex = content_dir / "default.xex"
    if not xex.is_file() or xex.stat().st_size != target["xex_size"]:
        raise OracleError("full XEX size mismatch")
    if sha256(xex) != target["xex_sha256"]:
        raise OracleError("full XEX SHA-256 mismatch")
    if xxh3(xex) != target["xex_xxh3"]:
        raise OracleError("full XEX XXH3 mismatch")
    identity = json.loads((ROOT / target["identity_manifest"]).read_text())
    identity_target = identity["target"]
    if (
        identity_target["title_id"] != target["title_id"]
        or identity_target["media_id"] != target["media_id"]
        or identity_target["xex_version"] != target["version"]
        or identity_target["sha256"] != target["xex_sha256"]
        or identity_target["module_xxh3"] != target["module_xxh3"]
    ):
        raise OracleError("Title/Media/version identity mismatch")
    checkout_xex = checkout / "assets/default.xex"
    runtime_assets = build_dir / "assets"
    if (
        not checkout_xex.exists()
        or not checkout_xex.samefile(xex)
        or not runtime_assets.exists()
        or not runtime_assets.samefile(content_dir)
    ):
        raise OracleError("build assets do not resolve to verified content")

    cache = cmake_cache(build_dir / "CMakeCache.txt")
    for key, expected in build["cmake"].items():
        if cache.get(key) != expected:
            raise OracleError(f"CMake contract mismatch: {key}")
    if cache.get("CMAKE_CXX_COMPILER") != build["compiler"]:
        raise OracleError("compiler mismatch")
    if sha256(checkout / "ac6recomp_config.toml") != build["runtime_config_sha256"]:
        raise OracleError("runtime configuration mismatch")
    generated = tree_sha256(checkout / "generated")
    expected_generated = build["generated"]
    if generated != (
        expected_generated["files"], expected_generated["bytes"],
        expected_generated["tree_sha256"],
    ):
        raise OracleError("generated tree mismatch")
    binary = build_dir / build["binary"]["name"]
    if (
        not os.access(binary, os.X_OK)
        or binary.stat().st_size != build["binary"]["size"]
        or sha256(binary) != build["binary"]["sha256"]
    ):
        raise OracleError("oracle binary mismatch")
    return {
        "xex_sha256": target["xex_sha256"],
        "xex_xxh3": target["xex_xxh3"],
        "module_xxh3": target["module_xxh3"],
        "commit": base["commit"],
        "generated_tree_sha256": generated[2],
        "binary": str(binary),
        "binary_sha256": build["binary"]["sha256"],
    }


def controller_inventory() -> list[str]:
    root = Path("/dev/input/by-id")
    return sorted(path.name for path in root.glob("*-joystick") if path.exists())


def reserve_display() -> int:
    for number in range(90, 190):
        if not Path(f"/tmp/.X11-unix/X{number}").exists():
            return number
    raise OracleError("no private X display available")


def read_rgb_mean(path: Path) -> float:
    result = subprocess.run(
        ["identify", "-format", "%[fx:(mean.r+mean.g+mean.b)/3]", str(path)],
        check=True, text=True, stdout=subprocess.PIPE,
    )
    return float(result.stdout)


def terminate(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    for request in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(process.pid, request)
            process.wait(timeout=3)
            return
        except (ProcessLookupError, subprocess.TimeoutExpired):
            pass


def execute(
    checkout: Path, build_dir: Path, content_dir: Path, output: Path,
    manifest_path: Path, expected_controllers: int, timeout_seconds: int,
) -> dict[str, object]:
    if output.exists():
        raise OracleError("output must not exist")
    actual_controllers = controller_inventory()
    if len(actual_controllers) != expected_controllers:
        raise OracleError("physical controller count mismatch")
    lineage = verify_lineage(checkout, build_dir, content_dir, manifest_path)
    output.mkdir(parents=True)
    profile = output / "profile"
    captures = output / "captures"
    profile.mkdir()
    captures.mkdir()
    display_number = reserve_display()
    display = f":{display_number}"
    console = (output / "console.log").open("wb")
    xvfb: subprocess.Popen[bytes] | None = None
    game: subprocess.Popen[bytes] | None = None
    started_raw_ns = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    existing_logs = set((build_dir / "logs").glob("ac6recomp_*.log"))
    run_error = ""
    capture_records: list[dict[str, object]] = []
    log_path: Path | None = None
    try:
        xvfb = subprocess.Popen(
            ["Xvfb", display, "-screen", "0", "1280x720x24", "-nolisten", "tcp", "-ac"],
            start_new_session=True, stdout=console, stderr=subprocess.STDOUT,
        )
        for _ in range(100):
            if Path(f"/tmp/.X11-unix/X{display_number}").exists():
                break
            time.sleep(0.02)
        else:
            raise OracleError("private X display did not start")
        environment = os.environ.copy()
        environment.update({
            "DISPLAY": display,
            "SDL_AUDIODRIVER": "dummy",
            "XDG_DATA_HOME": str(profile),
        })
        game = subprocess.Popen(
            [lineage["binary"]], cwd=checkout, env=environment,
            start_new_session=True, stdout=console, stderr=subprocess.STDOUT,
        )
        deadline = time.monotonic() + timeout_seconds
        observation_raw_ns = 0
        log_text = ""
        while time.monotonic() < deadline:
            new_logs = set((build_dir / "logs").glob("ac6recomp_*.log")) - existing_logs
            if new_logs:
                log_path = max(new_logs, key=lambda path: path.stat().st_mtime_ns)
                sample_raw_ns = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
                log_text = log_path.read_text(encoding="utf-8", errors="replace")
                if "AC6_ORACLE_PHYSICAL_POLL index=2" in log_text:
                    observation_raw_ns = sample_raw_ns
                    break
            if game.poll() is not None:
                raise OracleError("oracle exited before title poll")
            time.sleep(0.05)
        else:
            raise OracleError("oracle title/poll timeout")
        if FATAL_RE.search(log_text):
            raise OracleError("fatal runtime log marker")
        time.sleep(1)
        window = subprocess.run(
            ["xdotool", "search", "--name", r"^ac6recomp \["],
            env=environment, check=True, text=True, stdout=subprocess.PIPE,
        ).stdout.splitlines()[0]
        for index in range(3):
            path = captures / f"present-{index}.png"
            subprocess.run(
                ["import", "-display", display, "-window", window, str(path)],
                env=environment, check=True, timeout=5,
            )
            mean = read_rgb_mean(path)
            if mean <= 0.0:
                raise OracleError("black Vulkan capture")
            capture_records.append({
                "path": path.relative_to(output).as_posix(),
                "sha256": sha256(path), "rgb_mean": mean,
            })
            time.sleep(0.25)
        if len({record["sha256"] for record in capture_records}) < 2:
            raise OracleError("boot captures are not visually distinct")
        subprocess.run(
            ["xdotool", "windowclose", window], env=environment,
            check=True, timeout=5,
        )
        try:
            game_status = game.wait(timeout=30)
        except subprocess.TimeoutExpired as error:
            raise OracleError("oracle did not stop cleanly") from error
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        if "Execution complete" not in log_text or "AC6_ORACLE_SHUTDOWN" not in log_text:
            raise OracleError("clean shutdown markers absent")
        shutil.copy2(log_path, output / "oracle.log")
        return {
            "schema": "ac6.recomp-linux-oracle-run.v1",
            "status": "passed", "lineage": lineage,
            "clock": {
                "id": "CLOCK_MONOTONIC_RAW",
                "before_launch_ns": started_raw_ns,
                "before_marker_observation_ns": observation_raw_ns,
                "cadence_inferred": False,
            },
            "controllers": actual_controllers,
            "display": display, "audio_driver": "dummy",
            "game_status": game_status,
            "presents_observed": max(map(int, PRESENT_RE.findall(log_text))),
            "polls_observed": max(map(int, POLL_RE.findall(log_text))) + 1,
            "captures": capture_records,
            "log_sha256": sha256(output / "oracle.log"),
        }
    except Exception as error:
        run_error = str(error)
        raise
    finally:
        terminate(game)
        terminate(xvfb)
        console.close()
        if run_error:
            (output / "failure.json").write_text(
                json.dumps({"schema": "ac6.recomp-linux-oracle-failure.v1",
                            "error": run_error}, indent=2) + "\n"
            )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--content-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expected-controllers", type=int, choices=(0, 1), default=0)
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--verify-only", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_args()
    checkout = arguments.checkout.resolve()
    build_dir = arguments.build_dir.resolve()
    content_dir = arguments.content_dir.resolve()
    manifest = arguments.manifest.resolve()
    lock_name = hashlib.sha256(str(checkout).encode()).hexdigest()[:16]
    lock_path = Path(tempfile.gettempdir()) / f"ac6-linux-oracle-{lock_name}.lock"
    with lock_path.open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise SystemExit("oracle runner: checkout already active") from error
        try:
            if arguments.verify_only:
                result = verify_lineage(checkout, build_dir, content_dir, manifest)
            else:
                if arguments.output is None:
                    raise OracleError("--output is required for a run")
                result = execute(
                    checkout, build_dir, content_dir, arguments.output.resolve(),
                    manifest, arguments.expected_controllers, arguments.timeout,
                )
                (arguments.output.resolve() / "manifest.json").write_text(
                    json.dumps(result, indent=2) + "\n", encoding="utf-8"
                )
        except (OSError, OracleError, subprocess.SubprocessError) as error:
            raise SystemExit(f"oracle runner: {error}") from error
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
