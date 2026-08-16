#!/usr/bin/env python3
"""The codegen-OFF product must reject play/replay/probe before path validation."""
from __future__ import annotations

import subprocess
import sys


def run(binary: str, command: str, extra: list[str] | None = None) -> None:
    arguments = [binary, command]
    if command == "replay":
        arguments.append("/path/that/must/not/be/read.trace")
    arguments.extend(["--store", "/path/that/must/not/be/read"])
    if command == "probe":
        arguments.extend([
            "--until", "frontend", "--max-ticks", "1",
            "--trace", "/path/that/must/not/be/written.trace",
            "--report", "/path/that/must/not/be/written.json",
        ])
    arguments.extend(extra or [])
    completed = subprocess.run(
        arguments, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    expected = f"{command} unavailable: generated guest is not linked in this build"
    if completed.returncode != 3 or expected not in completed.stdout:
        raise AssertionError(
            f"{command} did not fail at the no-guest boundary: "
            f"exit={completed.returncode} output={completed.stdout!r}"
        )
    if "frontend ready" in completed.stdout or "replay accepted" in completed.stdout:
        raise AssertionError(f"{command} advertised unavailable functionality")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("expected ac6-demo-recomp path")
    run(sys.argv[1], "play")
    run(sys.argv[1], "replay")
    run(sys.argv[1], "probe")
    run(sys.argv[1], "play", [
        "--xam-movie-record", "/path/that/must/not/be/written.xam.jsonl",
    ])
    run(sys.argv[1], "replay", [
        "--xam-movie-replay", "/path/that/must/not/be/read.xam.jsonl",
    ])
    run(sys.argv[1], "probe", [
        "--xam-movie-replay", "/path/that/must/not/be/read.xam.jsonl",
    ])
    print("AC6_DEMO_NO_GUEST_CLI_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
