#!/usr/bin/env python3
"""Seal ordered AC6 Mission 01 JSONL events as ac6.execution-trace.v2."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any


TRACE_SCHEMA = "ac6.execution-trace.v2"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
DOMAINS = (
    "controller_input",
    "simulation_snapshot",
    "mission_objectives",
    "graphics_submission",
    "output_hashes",
)
EVENT_KEYS = {"sequence", "tick", "domain", "payload"}
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")


class TraceV2Error(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path: Path, name: str) -> None:
    if not path.is_file():
        raise TraceV2Error(f"{name} is not a file")


def validate_events(events: object, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if not isinstance(events, list) or len(events) != tick_count * len(DOMAINS):
        raise TraceV2Error("event count")
    for sequence, event in enumerate(events):
        if not isinstance(event, dict) or set(event) != EVENT_KEYS:
            raise TraceV2Error(f"event {sequence} shape")
        expected_tick = start_tick + sequence // len(DOMAINS)
        expected_domain = DOMAINS[sequence % len(DOMAINS)]
        if event["sequence"] != sequence:
            raise TraceV2Error(f"event {sequence} sequence")
        if (not isinstance(event["tick"], int) or isinstance(event["tick"], bool) or
                event["tick"] != expected_tick):
            raise TraceV2Error(f"event {sequence} tick")
        if event["domain"] != expected_domain:
            raise TraceV2Error(f"event {sequence} domain")
        if not isinstance(event["payload"], dict):
            raise TraceV2Error(f"event {sequence} payload")
        if expected_domain == "output_hashes":
            if not event["payload"] or any(
                not isinstance(name, str) or not name or
                not isinstance(value, str) or SHA256.fullmatch(value) is None
                for name, value in event["payload"].items()
            ):
                raise TraceV2Error(f"event {sequence} output hashes")
    return events


def load_jsonl(path: Path, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError as error:
                raise TraceV2Error(f"line {line_number}: {error.msg}") from error
    return validate_events(events, start_tick, tick_count)


def artifact(path: Path) -> dict[str, str]:
    return {"path": path.as_posix(), "sha256": sha256(path)}


def build_trace(
    raw_path: Path,
    role: str,
    oracle_commit: str,
    native_commit: str,
    patch_stack: Path,
    binary: Path,
    replay: Path,
    probe: Path,
    probe_id: str,
    window_id: str,
    start_tick: int,
    tick_count: int,
    sample_hz: int,
    native_simulation_hz: int,
    native_ticks_per_sample: int,
) -> dict[str, Any]:
    if role not in {"oracle", "native"}:
        raise TraceV2Error("role")
    if COMMIT.fullmatch(oracle_commit) is None or COMMIT.fullmatch(native_commit) is None:
        raise TraceV2Error("commit identity")
    if not probe_id or not window_id:
        raise TraceV2Error("probe or window identity")
    if (start_tick < 0 or tick_count <= 0 or tick_count > 600_000 or
            sample_hz <= 0 or native_simulation_hz <= 0 or
            native_ticks_per_sample <= 0 or
            sample_hz * native_ticks_per_sample != native_simulation_hz):
        raise TraceV2Error("window bounds")
    for path, name in (
        (raw_path, "raw capture"),
        (patch_stack, "patch stack"),
        (binary, "binary"),
        (replay, "replay"),
        (probe, "probe"),
    ):
        require_file(path, name)
    events = load_jsonl(raw_path, start_tick, tick_count)
    return {
        "schema": TRACE_SCHEMA,
        "header": {
            "role": role,
            "target": {"module": "default.xex", "xex_sha256": XEX_SHA256},
            "commits": {"oracle": oracle_commit, "native": native_commit},
            "patch_stack": artifact(patch_stack),
            "binary": artifact(binary),
            "replay": artifact(replay),
            "probe": {"id": probe_id, **artifact(probe)},
            "capture": artifact(raw_path),
            "window": {
                "id": window_id,
                "start_tick": start_tick,
                "tick_count": tick_count,
                "sample_hz": sample_hz,
                "cadence": {
                    "oracle_update_hz": sample_hz,
                    "native_simulation_hz": native_simulation_hz,
                    "native_ticks_per_sample": native_ticks_per_sample,
                    "input_resampling": "zero_order_hold",
                    "snapshot_sampling": "last_native_tick_in_sample",
                },
                "domains": list(DOMAINS),
            },
        },
        "event_count": len(events),
        "events": events,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--role", choices=("oracle", "native"), required=True)
    parser.add_argument("--oracle-commit", required=True)
    parser.add_argument("--native-commit", required=True)
    parser.add_argument("--patch-stack", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--probe-id", required=True)
    parser.add_argument("--window", required=True)
    parser.add_argument("--start-tick", type=int, required=True)
    parser.add_argument("--tick-count", type=int, required=True)
    parser.add_argument("--sample-hz", type=int, default=30)
    parser.add_argument("--native-simulation-hz", type=int, default=60)
    parser.add_argument("--native-ticks-per-sample", type=int, default=2)
    arguments = parser.parse_args()
    try:
        trace = build_trace(
            arguments.raw_jsonl, arguments.role, arguments.oracle_commit,
            arguments.native_commit, arguments.patch_stack, arguments.binary,
            arguments.replay, arguments.probe, arguments.probe_id,
            arguments.window, arguments.start_tick, arguments.tick_count,
            arguments.sample_hz, arguments.native_simulation_hz,
            arguments.native_ticks_per_sample,
        )
        arguments.output.write_text(
            json.dumps(trace, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    except (OSError, TraceV2Error) as error:
        print(f"execution_trace_v2=fail reason={error}")
        return 1
    print(f"execution_trace_v2=pass role={arguments.role} "
          f"ticks={arguments.tick_count} events={trace['event_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
