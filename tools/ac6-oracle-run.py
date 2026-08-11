#!/usr/bin/env python3
"""Run a bounded AC6_recomp route on a private X display with owned cleanup."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import stat
import subprocess
import time
from pathlib import Path

from build_ac6_execution_trace_v2 import TraceV2Error, load_jsonl

ROOT = Path(__file__).resolve().parents[1]
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
FATAL = re.compile(
    r"REX_FATAL|Unresolved branch|ac6-oracle-indirect-miss|"
    r"ac6-oracle-host-trap|Unhandled SIGSEGV",
    re.IGNORECASE,
)


class RunError(RuntimeError):
    pass


def normalize_display(value: str) -> str:
    if value.isdigit():
        value = f":{value}"
    if re.fullmatch(r":[0-9]+(?:\.0)?", value) is None:
        raise RunError("X display must be :N, N, or :N.0")
    return value


def parse_pulse_keys(value: str) -> list[str]:
    keys = value.split("+")
    if not 1 <= len(keys) <= 4 or any(
        re.fullmatch(r"[A-Za-z0-9_]+", key) is None for key in keys
    ):
        raise RunError("invalid pulse key sequence")
    return keys


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def shm_inventory(root: Path = Path("/dev/shm")) -> list[dict[str, object]]:
    entries = []
    for pattern in (
        "rexglue_memory_*", "xenia_memory_*", "xenia_code_cache_*"
    ):
        for path in root.glob(pattern):
            if path.is_file():
                entries.append({"name": path.name, "bytes": path.stat().st_size})
    return sorted(entries, key=lambda item: str(item["name"]))


def cleanup_owned_shm(
    before: list[dict[str, object]], root: Path = Path("/dev/shm")
) -> list[dict[str, object]]:
    baseline = {str(record["name"]) for record in before}
    cleaned = []
    for record in shm_inventory(root):
        name = str(record["name"])
        if name in baseline or re.fullmatch(r"rexglue_memory_[0-9]+", name) is None:
            continue
        path = root / name
        metadata = path.stat(follow_symlinks=False)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_uid != os.getuid():
            raise RunError(f"refusing shared-memory cleanup for {name}")
        path.unlink()
        cleaned.append(record)
    return cleaned


def terminate_owned(process: subprocess.Popen[bytes] | None) -> int | None:
    if process is None:
        return None
    if process.poll() is not None:
        return process.returncode
    for request, timeout in ((signal.SIGINT, 3), (signal.SIGTERM, 2), (signal.SIGKILL, 2)):
        try:
            os.killpg(process.pid, request)
        except ProcessLookupError:
            break
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            continue
    return process.poll()


def parse_steps(path: Path, stack: tuple[Path, ...] = (),
                sources: list[Path] | None = None) -> list[tuple[str, str, str]]:
    path = path.resolve()
    if path in stack or len(stack) >= 8:
        raise RunError("recursive or over-deep route include")
    if sources is not None and path not in sources:
        sources.append(path)
    steps = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) > 3 or not fields[0]:
            raise RunError(f"malformed route line {line_number}")
        fields += [""] * (3 - len(fields))
        if fields[0] == "include":
            if not fields[1] or fields[2]:
                raise RunError(f"malformed route include at line {line_number}")
            included = (path.parent / fields[1]).resolve()
            try:
                included.relative_to(ROOT)
            except ValueError as error:
                raise RunError("route include outside project") from error
            steps.extend(parse_steps(included, (*stack, path), sources))
            continue
        steps.append((fields[0], fields[1], fields[2]))
    if not steps:
        raise RunError("empty route")
    return steps


def validate_trace_timing(steps: list[tuple[str, str, str]], unlock_fps: bool) -> None:
    if unlock_fps and any(operation == "arm-trace" for operation, _, _ in steps):
        raise RunError(
            "trace routes must enable the 60 Hz correction at arm time, after startup"
        )


class OracleRun:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.args = arguments
        self.deadline = time.monotonic() + arguments.duration
        self.display_env = {**os.environ, "DISPLAY": arguments.display,
                            "SDL_AUDIODRIVER": "dummy"}
        self.xvfb: subprocess.Popen[bytes] | None = None
        self.game: subprocess.Popen[bytes] | None = None
        self.console = None
        self.log_offset = 0
        self.console_offset = 0
        self.captures: list[dict[str, object]] = []
        self.executed_steps = 0
        self.trace_v2_armed = False

    @property
    def log_path(self) -> Path:
        return self.args.output / "ac6recomp.log"

    @property
    def console_path(self) -> Path:
        return self.args.output / "console.log"

    def require_time(self) -> None:
        if time.monotonic() >= self.deadline:
            raise RunError("route duration exceeded")
        if self.game is not None and self.game.poll() is not None:
            raise RunError(f"runtime exited early with status {self.game.returncode}")

    def sleep(self, seconds: float) -> None:
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.require_time()
            time.sleep(min(0.2, end - time.monotonic()))

    def focus(self) -> None:
        for _ in range(100):
            self.require_time()
            result = subprocess.run(
                ["xdotool", "search", "--classname", "ac6recomp"],
                env=self.display_env, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, check=False,
            )
            windows = result.stdout.split()
            if windows:
                window = windows[-1]
                subprocess.run(["xdotool", "windowactivate", window], env=self.display_env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               check=False)
                subprocess.run(["xdotool", "windowfocus", window], env=self.display_env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               check=False)
                return
            time.sleep(0.1)
        raise RunError("no focusable AC6 window")

    def input_edge(self, kind: str, value: str, hold: str) -> None:
        self.focus()
        duration = float(hold or "0.1")
        if not 0 < duration <= 5:
            raise RunError("input hold outside bounds")
        if kind == "key":
            down, up = ["xdotool", "keydown", value], ["xdotool", "keyup", value]
        else:
            if value not in {"1", "2", "3"}:
                raise RunError("mouse button outside bounds")
            down, up = ["xdotool", "mousedown", value], ["xdotool", "mouseup", value]
        subprocess.run(down, env=self.display_env, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.sleep(duration)
        subprocess.run(up, env=self.display_env, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def new_log_text(self) -> str:
        chunks = []
        for path, offset_name in (
            (self.log_path, "log_offset"),
            (self.console_path, "console_offset"),
        ):
            if not path.is_file():
                continue
            with path.open("r", encoding="utf-8", errors="replace") as source:
                source.seek(getattr(self, offset_name))
                chunks.append(source.read())
                setattr(self, offset_name, source.tell())
        return "\n".join(chunks)

    def wait_log(self, pattern: str, timeout: float, pulse: str = "") -> None:
        expression = re.compile(pattern)
        end = min(self.deadline, time.monotonic() + timeout)
        pending = ""
        while time.monotonic() < end:
            self.require_time()
            pending += self.new_log_text()
            if expression.search(pending):
                return
            if pulse:
                for key in parse_pulse_keys(pulse):
                    self.input_edge("key", key, "0.1")
                    # Guest transitions may complete nearly two seconds after
                    # an edge, then need one flush interval to become visible.
                    # Settle before the next key so a late pulse cannot accept
                    # the following dialog.
                    self.sleep(4)
                    pending += self.new_log_text()
                    if expression.search(pending):
                        return
            else:
                self.sleep(1)
        raise RunError(f"log predicate not reached: {pattern}")

    def capture(self, label: str) -> None:
        if re.fullmatch(r"[A-Za-z0-9._-]+", label) is None:
            raise RunError("unsafe capture label")
        path = self.args.output / f"step-{self.executed_steps:02d}-{label}.png"
        subprocess.run(["import", "-window", "root", str(path)], env=self.display_env,
                       check=True, timeout=20, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        self.captures.append({"path": path.name, "bytes": path.stat().st_size,
                              "sha256": sha256(path)})

    def present_count(self) -> int:
        if not self.log_path.is_file():
            return 0
        return self.log_path.read_text(encoding="utf-8", errors="replace").count("PRESENT")

    def execute(self, steps: list[tuple[str, str, str]]) -> None:
        for operation, argument, limit in steps:
            self.executed_steps += 1
            if operation == "sleep":
                self.sleep(float(argument))
            elif operation in {"key", "mouse"}:
                self.input_edge(operation, argument, limit)
            elif operation == "capture":
                self.capture(argument)
            elif operation == "wait":
                self.wait_log(argument, float(limit))
            elif operation == "wait-pulse":
                self.wait_log(argument, self.deadline - time.monotonic(), limit)
            elif operation == "present":
                start = self.present_count()
                target = start + int(argument)
                end = min(self.deadline, time.monotonic() + float(limit))
                while self.present_count() < target and time.monotonic() < end:
                    self.sleep(0.2)
                if self.present_count() < target:
                    raise RunError("presentation predicate not reached")
            elif operation == "arm-trace":
                if argument or limit or self.trace_v2_armed:
                    raise RunError("invalid trace arm step")
                (self.args.output / "mission01-execution-v2.arm").write_text(
                    "armed\n", encoding="utf-8"
                )
                self.trace_v2_armed = True
            else:
                raise RunError(f"unknown route operation: {operation}")

    def start(self) -> None:
        display_number = self.args.display.removeprefix(":").split(".", 1)[0]
        if not display_number.isdigit() or Path(f"/tmp/.X11-unix/X{display_number}").exists():
            raise RunError("private X display is not available")
        self.xvfb = subprocess.Popen(
            ["Xvfb", self.args.display, "-screen", "0", "1280x720x24"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True,
        )
        time.sleep(2)
        if self.xvfb.poll() is not None:
            raise RunError("Xvfb startup failed")

        command = [
            str(self.args.binary), str(self.args.game_dir), "--mnk_mode=true",
            "--ac6_performance_mode=false", "--log_flush_interval=1",
            f"--log_file={self.log_path}",
            f"--user_data_root={self.args.output / 'user-data'}",
            f"--ac6_oracle_probe_path={self.args.output / 'mission01-frame.raw.jsonl'}",
            f"--ac6_oracle_trace_v2_path={self.args.output / 'mission01-execution-v2.raw.jsonl'}",
            f"--ac6_oracle_trace_v2_arm_path={self.args.output / 'mission01-execution-v2.arm'}",
            f"--ac6_unlock_fps={'true' if self.args.unlock_fps else 'false'}",
            "--audio_trace_telemetry=true",
            "--audio_trace_render_driver_verbose=true",
            "--ac6_render_capture=true",
            "--ac6_native_graphics_enabled=false",
        ]
        self.console = self.console_path.open("wb")
        self.game = subprocess.Popen(
            command, cwd=self.args.binary.parent, env=self.display_env,
            stdout=self.console, stderr=subprocess.STDOUT, start_new_session=True,
        )

    def close(self) -> tuple[int | None, int | None]:
        game_status = terminate_owned(self.game)
        xvfb_status = terminate_owned(self.xvfb)
        if self.console is not None:
            self.console.close()
        return game_status, xvfb_status


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--game-dir", type=Path, default=ROOT / "game-files")
    parser.add_argument("--route", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=int, default=900)
    parser.add_argument("--display", default=":120")
    parser.add_argument("--unlock-fps", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_args()
    try:
        arguments.display = normalize_display(arguments.display)
    except RunError as error:
        raise SystemExit(f"oracle runner: {error}") from error
    arguments.binary = arguments.binary.resolve()
    arguments.game_dir = arguments.game_dir.resolve()
    arguments.route = arguments.route.resolve()
    arguments.output = arguments.output.resolve()
    if not arguments.binary.is_file() or not os.access(arguments.binary, os.X_OK):
        raise SystemExit("oracle runner: binary is not executable")
    if sha256(arguments.game_dir / "default.xex") != XEX_SHA256:
        raise SystemExit("oracle runner: PAL XEX identity mismatch")
    if arguments.output.exists():
        raise SystemExit("oracle runner: output must not exist")
    if not 1 <= arguments.duration <= 3600:
        raise SystemExit("oracle runner: duration outside bounds")
    for tool in ("Xvfb", "xdotool", "import"):
        if shutil.which(tool) is None:
            raise SystemExit(f"oracle runner: missing tool {tool}")
    route_sources: list[Path] = []
    steps = parse_steps(arguments.route, sources=route_sources)
    try:
        validate_trace_timing(steps, arguments.unlock_fps)
    except RunError as error:
        raise SystemExit(f"oracle runner: {error}") from error
    arguments.output.mkdir(parents=True)
    (arguments.output / "user-data").mkdir()

    before = shm_inventory()
    started = time.time()
    runner = OracleRun(arguments)
    error = ""
    game_status = xvfb_status = None
    try:
        runner.start()
        runner.execute(steps)
    except KeyboardInterrupt:
        error = "interrupted"
    except (RunError, OSError, subprocess.SubprocessError, ValueError) as caught:
        error = str(caught)
    finally:
        game_status, xvfb_status = runner.close()

    cleanup_error = ""
    cleaned_shared_memory: list[dict[str, object]] = []
    after_termination = shm_inventory()
    try:
        cleaned_shared_memory = cleanup_owned_shm(before)
    except (OSError, RunError) as caught:
        cleanup_error = str(caught)
    after = shm_inventory()
    log_text = runner.log_path.read_text(encoding="utf-8", errors="replace") \
        if runner.log_path.is_file() else ""
    console_path = arguments.output / "console.log"
    console_text = console_path.read_text(encoding="utf-8", errors="replace") \
        if console_path.is_file() else ""
    fatal_matches = sorted(set(FATAL.findall(log_text + "\n" + console_text)))
    trace_v2: dict[str, object] | None = None
    if runner.trace_v2_armed:
        trace_path = arguments.output / "mission01-execution-v2.raw.jsonl"
        try:
            events = load_jsonl(trace_path, 1, 3600)
            trace_v2 = {
                "path": trace_path.name,
                "sha256": sha256(trace_path),
                "ticks": 3600,
                "events": len(events),
            }
        except (OSError, TraceV2Error) as caught:
            cleanup_error = cleanup_error or f"trace v2: {caught}"
    manifest = {
        "schema": "ac6.recomp-oracle-run.v1",
        "binary": {"path": str(arguments.binary), "sha256": sha256(arguments.binary)},
        "target": {"module": "default.xex", "sha256": XEX_SHA256},
        "route": {"path": str(arguments.route), "sha256": sha256(arguments.route),
                  "sources": [{"path": str(path), "sha256": sha256(path)}
                              for path in route_sources],
                  "steps": len(steps), "executed_steps": runner.executed_steps},
        "duration_seconds": time.time() - started,
        "display": arguments.display,
        "audio_driver": "dummy",
        "audio_trace_telemetry": True,
        "unlock_fps": arguments.unlock_fps,
        "game_status": game_status,
        "xvfb_status": xvfb_status,
        "error": error,
        "fatal_matches": fatal_matches,
        "captures": runner.captures,
        "shared_memory_before": before,
        "shared_memory_after_termination": after_termination,
        "shared_memory_cleaned": cleaned_shared_memory,
        "shared_memory_after": after,
        "trace_v2": trace_v2,
    }
    (arguments.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    if error or cleanup_error or fatal_matches or before != after:
        print(f"oracle_run=fail error={error or cleanup_error or fatal_matches or 'shared-memory leak'}")
        return 1
    print(f"oracle_run=pass steps={runner.executed_steps} captures={len(runner.captures)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
