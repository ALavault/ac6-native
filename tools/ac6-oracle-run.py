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
from typing import NamedTuple

from build_ac6_execution_trace_v2 import TraceV2Error, load_jsonl

ROOT = Path(__file__).resolve().parents[1]
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
DATA_TBL_SHA256 = "82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5"
XAM_MOVIE_SCHEMA = "ac6.xam-input-movie.v1"
TRACE_INPUT_TICKS = 3600
TRACE_INPUT_MAX_BYTES = 512 * 1024
REPLAY_LOADED_MARKER = "AC6 oracle replay loaded: 3600 manager-tick rows"
FATAL = re.compile(
    r"REX_FATAL|Unresolved branch|ac6-oracle-indirect-miss|"
    r"ac6-oracle-host-trap|AC6 oracle replay input invalid|"
    r"AC6 XAM input movie.*failed|Unhandled SIGSEGV",
    re.IGNORECASE,
)


class RunError(RuntimeError):
    pass


class TraceInputSnapshot(NamedTuple):
    source: Path
    payload: bytes
    sha256: str
    rows: int


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


def canonical_sha256(value: object) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def content_manifest(source: Path) -> tuple[dict[str, object], str]:
    entries: list[dict[str, object]] = []
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise RunError("content source contains a symlink")
        if path.is_file():
            entries.append({
                "path": path.relative_to(source).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            })
    manifest = {"schema": "ac6.content-root-manifest.v1", "files": entries}
    return manifest, canonical_sha256(manifest)


def stage_isolated_content(source: Path, destination: Path) -> tuple[dict[str, object], str]:
    manifest, digest = content_manifest(source)
    destination.mkdir()
    for child in source.iterdir():
        (destination / child.name).symlink_to(child.resolve(), target_is_directory=child.is_dir())
    return manifest, digest


def empty_tree_sha256() -> str:
    return hashlib.sha256(b"").hexdigest()


def write_xam_movie_header(arguments: argparse.Namespace) -> tuple[Path, dict[str, object]]:
    runtime_root = arguments.binary.parents[3]
    runtime_config = runtime_root / "ac6recomp_config.toml"
    if not runtime_config.is_file():
        raise RunError("runtime configuration identity is unavailable")
    behavior = {
        "audio_driver": "dummy",
        "mnk_mode": True,
        "performance_mode": False,
        "unlock_fps": arguments.unlock_fps,
        "native_graphics": False,
    }
    empty = empty_tree_sha256()
    header = {
        "kind": "header",
        "schema": XAM_MOVIE_SCHEMA,
        "lane": "bridge",
        "initial_state": "FROM_XEX_LAUNCH",
        "target": {
            "target_id": "ac6-pal-default-xex",
            "module": "default.xex",
            "xex_sha256": XEX_SHA256,
            "data_tbl_sha256": DATA_TBL_SHA256,
            "base_address": "82000000",
        },
        "content": {"manifest_sha256": arguments.content_manifest_sha256},
        "runtime": {
            "commit": "dcd41b7457fcac8242f8ef40de83d1719390d5af",
            "diff_sha256": arguments.runtime_diff_sha256,
            "binary_sha256": sha256(arguments.binary),
        },
        "configuration": {
            "runtime_sha256": sha256(runtime_config),
            "behavior_sha256": canonical_sha256(behavior),
        },
        "profile": {"id": arguments.profile_id, "tree_sha256": empty},
        "roots": {
            "storage": {"label": "storage", "tree_sha256": empty, "isolated": True},
            "content": {"label": "content", "tree_sha256": arguments.content_manifest_sha256,
                        "isolated": True},
            "cache": {"label": "cache", "tree_sha256": empty, "isolated": True},
        },
        "route": {
            "recipe": "scripts/ac6-oracle-mission01-controlled-sortie-delayed.steps",
            "sha256": sha256(arguments.route),
        },
        "interventions": ["mission01-step-recipe"],
    }
    path = arguments.output / "xam-input-movie.header.jsonl"
    path.write_text(json.dumps(header, sort_keys=True, separators=(",", ":")) + "\n",
                    encoding="utf-8")
    path.chmod(0o444)
    return path, header


def xam_movie_summary(path: Path) -> dict[str, object]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
        header = json.loads(lines[0])
        footer = json.loads(lines[-1])
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, IndexError) as error:
        raise RunError(f"XAM movie unreadable: {error}") from error
    if (header.get("schema") != XAM_MOVIE_SCHEMA or footer.get("kind") != "footer"):
        raise RunError("XAM movie identity")
    events = [json.loads(line) for line in lines[1:-1]]
    if footer.get("event_count") != len(events) or not events:
        raise RunError("XAM movie trace-length mismatch")
    normalized = []
    for ordinal, event in enumerate(events):
        expected = {
            "kind": "XamInputGetState", "ordinal": ordinal,
            "caller_lr": event.get("caller_lr"), "user": event.get("user"),
            "flags": event.get("flags"),
            "state_ptr_null": event.get("state_ptr_null"),
            "result": event.get("result"), "state16": event.get("state16"),
        }
        if event.get("kind") != "XamInputGetState" or event.get("ordinal") != ordinal:
            raise RunError("XAM movie event-type or ordinal mismatch")
        normalized.append(json.dumps(expected, sort_keys=True, separators=(",", ":")) + "\n")
    normalized_sha = hashlib.sha256("".join(normalized).encode()).hexdigest()
    if footer.get("normalized_sha256") != normalized_sha:
        raise RunError("XAM movie normalized stream mismatch")
    return {
        "path": path.name,
        "sha256": sha256(path),
        "events": len(events),
        "normalized_sha256": normalized_sha,
        "caller_lrs": sorted({event["caller_lr"] for event in events}),
        "guest_threads": sorted({event["guest_thread"] for event in events}),
        "first_guest_tick": events[0]["guest_tick"],
        "last_guest_tick": events[-1]["guest_tick"],
    }


def guest_milestone_digest(
    text: str, steps: list[tuple[str, str, str]]
) -> tuple[list[dict[str, object]], str]:
    milestones: list[dict[str, object]] = []
    offset = 0
    for step, (operation, pattern, _) in enumerate(steps, start=1):
        if operation not in {"wait", "wait-pulse"}:
            continue
        match = re.compile(pattern).search(text, offset)
        if match is None:
            raise RunError(f"guest milestone absent at route step {step}")
        value = match.group(0)
        milestones.append({
            "step": step,
            "predicate_sha256": hashlib.sha256(pattern.encode()).hexdigest(),
            "value_sha256": hashlib.sha256(value.encode()).hexdigest(),
        })
        offset = match.end()
    return milestones, canonical_sha256(milestones)


def stage_user_data_seed(source: Path, destination: Path) -> dict[str, object]:
    """Copy a bounded bridge seed while recording its immutable tree digest.

    A seed is an explicit bridge intervention.  It is never used by the
    product lane and symlinks are rejected so a route cannot escape the
    caller-owned temporary tree.
    """
    source = source.resolve()
    if not source.is_dir() or source.is_symlink():
        raise RunError("user-data seed must be a regular directory")
    files: list[Path] = []
    total_bytes = 0
    digest = hashlib.sha256()
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise RunError("user-data seed contains a symlink")
        if path.is_file():
            relative = path.relative_to(source).as_posix()
            payload = path.read_bytes()
            files.append(path)
            total_bytes += len(payload)
            digest.update(relative.encode("utf-8"))
            digest.update(b"\0")
            digest.update(hashlib.sha256(payload).digest())
            digest.update(b"\0")
    if total_bytes > 64 * 1024 * 1024:
        raise RunError("user-data seed exceeds 64 MiB")
    shutil.copytree(source, destination, symlinks=False)
    return {
        "source_path": str(source),
        "tree_sha256": digest.hexdigest(),
        "file_count": len(files),
        "bytes": total_bytes,
    }


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


def validate_trace_input_payload(
    payload: bytes, ticks: int = TRACE_INPUT_TICKS
) -> int:
    if len(payload) > TRACE_INPUT_MAX_BYTES:
        raise RunError("trace input exceeds byte bound")
    try:
        lines = payload.decode("utf-8").splitlines()
    except UnicodeError as error:
        raise RunError(f"trace input is not UTF-8: {error}") from error
    rows = 0
    for line_number, line in enumerate(lines, start=1):
        if line == "" or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 6:
            raise RunError(f"malformed trace input line {line_number}")
        try:
            tick, pitch, roll, yaw, throttle, buttons = map(int, fields)
        except ValueError as error:
            raise RunError(f"non-decimal trace input line {line_number}") from error
        rows += 1
        if tick != rows:
            raise RunError(f"non-sequential trace input tick at line {line_number}")
        if not -32768 <= pitch <= 32767:
            raise RunError(f"trace input pitch outside bounds at line {line_number}")
        if not -32768 <= roll <= 32767:
            raise RunError(f"trace input roll outside bounds at line {line_number}")
        if not -32768 <= yaw <= 32767:
            raise RunError(f"trace input yaw outside bounds at line {line_number}")
        if not 0 <= throttle <= 255:
            raise RunError(f"trace input throttle outside bounds at line {line_number}")
        if not 0 <= buttons <= 65535:
            raise RunError(f"trace input buttons outside bounds at line {line_number}")
        if rows > ticks:
            raise RunError(f"trace input exceeds {ticks} ticks")
    if rows != ticks:
        raise RunError(f"trace input has {rows} rows, expected {ticks}")
    return rows


def load_trace_input_snapshot(
    path: Path, ticks: int = TRACE_INPUT_TICKS
) -> TraceInputSnapshot:
    try:
        payload = path.read_bytes()
    except OSError as error:
        raise RunError(f"trace input is unreadable: {error}") from error
    rows = validate_trace_input_payload(payload, ticks)
    return TraceInputSnapshot(
        source=path,
        payload=payload,
        sha256=hashlib.sha256(payload).hexdigest(),
        rows=rows,
    )


def validate_trace_input(path: Path, ticks: int = TRACE_INPUT_TICKS) -> int:
    return load_trace_input_snapshot(path, ticks).rows


def stage_trace_input(snapshot: TraceInputSnapshot, destination: Path) -> None:
    try:
        with destination.open("xb") as output:
            output.write(snapshot.payload)
            output.flush()
            os.fsync(output.fileno())
        destination.chmod(0o444)
    except OSError as error:
        raise RunError(f"trace input staging failed: {error}") from error
    if sha256(destination) != snapshot.sha256:
        raise RunError("trace input staging identity mismatch")


class OracleRun:
    def __init__(self, arguments: argparse.Namespace) -> None:
        self.args = arguments
        self.deadline = time.monotonic() + arguments.duration
        self.display_env = {**os.environ, "DISPLAY": arguments.display,
                            "SDL_AUDIODRIVER": "dummy"}
        cache_root = getattr(arguments, "cache_root", None)
        if cache_root is not None:
            self.display_env["XDG_CACHE_HOME"] = str(cache_root)
        self.xvfb: subprocess.Popen[bytes] | None = None
        self.game: subprocess.Popen[bytes] | None = None
        self.console = None
        self.log_offset = 0
        self.console_offset = 0
        self.pending_log_text = ""
        self.captures: list[dict[str, object]] = []
        self.executed_steps = 0
        self.trace_v2_armed = False
        self.user_data_seed: dict[str, object] | None = None
        self.guest_milestones: list[dict[str, object]] = []

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
        while time.monotonic() < end:
            self.require_time()
            self.pending_log_text += self.new_log_text()
            match = expression.search(self.pending_log_text)
            if match:
                matched = match.group(0)
                self.guest_milestones.append({
                    "step": self.executed_steps,
                    "predicate_sha256": hashlib.sha256(pattern.encode()).hexdigest(),
                    "matched_sha256": hashlib.sha256(matched.encode()).hexdigest(),
                })
                self.pending_log_text = self.pending_log_text[match.end():]
                return
            if pulse:
                for key in parse_pulse_keys(pulse):
                    self.input_edge("key", key, "0.1")
                    # Guest transitions may complete nearly two seconds after
                    # an edge, then need one flush interval to become visible.
                    # Settle before the next key so a late pulse cannot accept
                    # the following dialog.
                    self.sleep(4)
                    self.pending_log_text += self.new_log_text()
                    match = expression.search(self.pending_log_text)
                    if match:
                        matched = match.group(0)
                        self.guest_milestones.append({
                            "step": self.executed_steps,
                            "predicate_sha256": hashlib.sha256(pattern.encode()).hexdigest(),
                            "matched_sha256": hashlib.sha256(matched.encode()).hexdigest(),
                        })
                        self.pending_log_text = self.pending_log_text[match.end():]
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
        if getattr(self.args, "xam_movie_replay", None) is not None:
            self.wait_log(
                "AC6 XAM input movie strict replay consumed all events",
                self.deadline - time.monotonic(),
            )
            return
        for operation, argument, limit in steps:
            self.executed_steps += 1
            if operation == "sleep":
                self.sleep(float(argument))
            elif operation in {"key", "mouse"}:
                self.input_edge(operation, argument, limit)
            elif operation == "capture":
                # XAM movies are guest-boundary artefacts.  Host presentation
                # frames are deliberately outside both recording and replay.
                if not (getattr(self.args, "xam_movie_record", False) or
                        getattr(self.args, "xam_movie_replay", None)):
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
            str(self.args.binary), str(self.args.content_root), "--mnk_mode=true",
            "--ac6_performance_mode=false", "--log_flush_interval=1",
            f"--log_file={self.log_path}",
            f"--user_data_root={self.args.storage_root}",
            f"--ac6_oracle_probe_path={self.args.output / 'mission01-frame.raw.jsonl'}",
            f"--ac6_oracle_trace_v2_path={self.args.output / 'mission01-execution-v2.raw.jsonl'}",
            f"--ac6_oracle_trace_v2_arm_path={self.args.output / 'mission01-execution-v2.arm'}",
            f"--ac6_unlock_fps={'true' if self.args.unlock_fps else 'false'}",
            "--audio_trace_telemetry=true",
            "--audio_trace_render_driver_verbose=true",
            "--ac6_render_capture=true",
            "--ac6_native_graphics_enabled=false",
        ]
        trace_input = getattr(self.args, "trace_input", None)
        if trace_input:
            command.append(f"--ac6_oracle_trace_v2_input_path={trace_input}")
        if self.args.xam_movie_record or self.args.xam_movie_replay is not None:
            command.append(f"--ac6_xam_movie_header_path={self.args.xam_movie_header}")
        if self.args.xam_movie_record:
            command.append(
                f"--ac6_xam_movie_record_path={self.args.output / 'xam-input-movie.jsonl'}"
            )
        if self.args.xam_movie_replay is not None:
            command.append(f"--ac6_xam_movie_replay_path={self.args.xam_movie_replay}")
        unit_boundary_output = getattr(self.args, "unit_boundary_output", None)
        if unit_boundary_output:
            command.append(f"--ac6_oracle_unit_boundary_path={unit_boundary_output}")
        self.console = self.console_path.open("wb")
        self.game = subprocess.Popen(
            command, cwd=self.args.binary.parent, env=self.display_env,
            stdout=self.console, stderr=subprocess.STDOUT, start_new_session=True,
        )

    def close(self) -> tuple[int | None, int | None]:
        if (self.args.xam_movie_record or self.args.xam_movie_replay is not None) and \
                self.game is not None and self.game.poll() is None:
            result = subprocess.run(
                ["xdotool", "search", "--classname", "ac6recomp"],
                env=self.display_env, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, check=False,
            )
            windows = result.stdout.split()
            if windows:
                subprocess.run(["xdotool", "windowclose", windows[-1]],
                               env=self.display_env, check=False,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                try:
                    self.game.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    pass
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
    parser.add_argument(
        "--trace-input", type=Path, default=None,
        help="replay the qualified controller_input rows from a trace-v2 TSV",
    )
    parser.add_argument(
        "--unit-boundary-output", type=Path, default=None,
        help="capture-only CX360UnitManager insertion JSONL path",
    )
    parser.add_argument(
        "--seed-user-data", type=Path, default=None,
        help="explicit bridge-only user-data seed; recorded as an intervention",
    )
    parser.add_argument(
        "--xam-movie-record", action="store_true",
        help="record every guest-visible XamInputGetState result from XEX launch",
    )
    parser.add_argument(
        "--xam-movie-replay", type=Path, default=None,
        help="strictly replay a sealed XAM input movie while bypassing host HID",
    )
    parser.add_argument(
        "--runtime-diff-sha256", default=None,
        help="qualified runtime patch/diff SHA-256 required by XAM movie mode",
    )
    parser.add_argument("--profile-id", default="isolated-english-new-game")
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
    if arguments.xam_movie_record and arguments.xam_movie_replay is not None:
        raise SystemExit("oracle runner: select exactly one XAM movie mode")
    movie_mode = arguments.xam_movie_record or arguments.xam_movie_replay is not None
    if movie_mode and (arguments.runtime_diff_sha256 is None or
                       re.fullmatch(r"[0-9a-f]{64}", arguments.runtime_diff_sha256) is None):
        raise SystemExit("oracle runner: XAM movie mode requires runtime diff SHA-256")
    if movie_mode and arguments.seed_user_data is not None:
        raise SystemExit("oracle runner: XAM movie FROM_XEX_LAUNCH forbids a user-data seed")
    if arguments.xam_movie_replay is not None:
        arguments.xam_movie_replay = arguments.xam_movie_replay.resolve()
        if not arguments.xam_movie_replay.is_file():
            raise SystemExit("oracle runner: XAM movie replay is not a regular file")
    trace_input_snapshot: TraceInputSnapshot | None = None
    if arguments.trace_input is not None:
        arguments.trace_input = arguments.trace_input.resolve()
        if not arguments.trace_input.is_file():
            raise SystemExit("oracle runner: trace input is not a regular file")
        try:
            trace_input_snapshot = load_trace_input_snapshot(arguments.trace_input)
        except RunError as error:
            raise SystemExit(f"oracle runner: {error}") from error
    if not arguments.binary.is_file() or not os.access(arguments.binary, os.X_OK):
        raise SystemExit("oracle runner: binary is not executable")
    if sha256(arguments.game_dir / "default.xex") != XEX_SHA256:
        raise SystemExit("oracle runner: PAL XEX identity mismatch")
    if sha256(arguments.game_dir / "DATA.TBL") != DATA_TBL_SHA256:
        raise SystemExit("oracle runner: DATA.TBL identity mismatch")
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
    arguments.storage_root = arguments.output / "storage-root"
    arguments.content_root = arguments.output / "content-root"
    arguments.cache_root = arguments.output / "cache-root"
    arguments.storage_root.mkdir()
    arguments.cache_root.mkdir()
    try:
        content_document, arguments.content_manifest_sha256 = stage_isolated_content(
            arguments.game_dir, arguments.content_root
        )
    except (OSError, RunError) as error:
        raise SystemExit(f"oracle runner: isolated content root: {error}") from error
    (arguments.output / "content-root-manifest.json").write_text(
        json.dumps(content_document, indent=2) + "\n", encoding="utf-8"
    )
    if movie_mode:
        try:
            arguments.xam_movie_header, arguments.xam_movie_header_document = \
                write_xam_movie_header(arguments)
        except (OSError, RunError) as error:
            raise SystemExit(f"oracle runner: XAM movie header: {error}") from error
    if arguments.seed_user_data is not None:
        try:
            arguments.storage_root.rmdir()
            runner_seed = stage_user_data_seed(
                arguments.seed_user_data, arguments.storage_root
            )
        except (OSError, RunError) as error:
            raise SystemExit(f"oracle runner: {error}") from error
        # The runner object is created below; retain the record on the
        # namespace so it is present even if startup fails.
        arguments.user_data_seed = runner_seed
    if trace_input_snapshot is not None:
        runtime_trace_input = arguments.output / "trace-input.tsv"
        try:
            stage_trace_input(trace_input_snapshot, runtime_trace_input)
        except RunError as error:
            raise SystemExit(f"oracle runner: {error}") from error
        arguments.trace_input = runtime_trace_input

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
    combined_log = log_text + "\n" + console_text
    xam_movie: dict[str, object] | None = None
    guest_milestones: list[dict[str, object]] = []
    guest_milestone_sha256: str | None = None
    if movie_mode:
        movie_path = (arguments.output / "xam-input-movie.jsonl"
                      if arguments.xam_movie_record else arguments.xam_movie_replay)
        try:
            xam_movie = xam_movie_summary(movie_path)
            guest_milestones, guest_milestone_sha256 = guest_milestone_digest(
                combined_log, steps
            )
        except RunError as caught:
            cleanup_error = cleanup_error or str(caught)
        expected_marker = ("AC6 XAM input movie finalized events="
                           if arguments.xam_movie_record
                           else "AC6 XAM input movie strict replay consumed all events")
        if expected_marker not in combined_log:
            cleanup_error = cleanup_error or "XAM movie runtime completion marker absent"
    replay_loaded = (
        REPLAY_LOADED_MARKER in log_text or REPLAY_LOADED_MARKER in console_text
    )
    if arguments.trace_input is not None and not replay_loaded:
        cleanup_error = cleanup_error or "trace input: runtime replay marker absent"
    runtime_trace_input_sha256: str | None = None
    if trace_input_snapshot is not None:
        try:
            runtime_trace_input_sha256 = sha256(arguments.trace_input)
        except OSError as caught:
            cleanup_error = cleanup_error or f"trace input: {caught}"
        if runtime_trace_input_sha256 != trace_input_snapshot.sha256:
            cleanup_error = cleanup_error or "trace input: staged identity changed"
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
        "trace_input": ({"source_path": str(trace_input_snapshot.source),
                         "runtime_path": str(arguments.trace_input),
                         "sha256": trace_input_snapshot.sha256,
                         "runtime_sha256": runtime_trace_input_sha256,
                         "rows": trace_input_snapshot.rows,
                         "runtime_loaded": replay_loaded}
                        if trace_input_snapshot is not None else None),
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
        "xam_input_movie": xam_movie,
        "guest_milestones": {
            "schema": "ac6.guest-milestone-stream.v1",
            "events": guest_milestones,
            "digest_sha256": guest_milestone_sha256,
        } if movie_mode else None,
        "initial_state": "FROM_XEX_LAUNCH" if movie_mode else None,
        "isolated_roots": ({
            "storage": str(arguments.storage_root),
            "content": str(arguments.content_root),
            "cache": str(arguments.cache_root),
            "content_manifest_sha256": arguments.content_manifest_sha256,
        } if movie_mode else None),
        "user_data_seed": getattr(arguments, "user_data_seed", None),
        "interventions": ([{
            "kind": "seeded-user-data",
            "behavioral": True,
            "qualification": "bridge-only",
            "tree_sha256": arguments.user_data_seed["tree_sha256"],
        }] if getattr(arguments, "user_data_seed", None) is not None else []) +
        ([{
            "kind": "mission01-step-recipe",
            "behavioral": True,
            "qualification": "bridge-only-bootstrap",
            "host_timestamps_recorded": False,
            "host_presentation_frames_recorded": False,
        }] if arguments.xam_movie_record else []),
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
