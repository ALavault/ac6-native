#!/usr/bin/env python3
"""Extract bounded AC6 gameplay observations from one native runtime log."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


LINE = re.compile(r"^\[(?P<time>[^]]+)\].*\[(?P<tag>ac6-gameplay-[^]]+|xam-input)\] (?P<body>.*)$")
PAIR = re.compile(r"(?P<key>[a-zA-Z0-9_]+)=(?P<value>[^ ]+)")


def parse_value(value: str):
    if value.startswith("0x") and "," not in value:
        return "0x" + value[2:].upper()
    if value.lstrip("-").isdigit():
        return int(value)
    return value


def record(match: re.Match[str], lane: str, run: str) -> dict:
    result = {
        "timestamp": match.group("time"),
        "event": match.group("tag"),
        "lane": lane,
        "run": run,
    }
    for item in PAIR.finditer(match.group("body")):
        key = item.group("key")
        if key == "event":
            key = "guest_event"
        result[key] = parse_value(item.group("value").rstrip(","))
    return result


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in rows))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--out-root", type=Path, default=Path("analysis"))
    parser.add_argument("--lane", default="bridge")
    parser.add_argument("--run", required=True)
    args = parser.parse_args()

    rows: list[dict] = []
    for line in args.log.read_text(errors="replace").splitlines():
        match = LINE.match(line)
        if match:
            rows.append(record(match, args.lane, args.run))

    gameplay = args.out_root / "gameplay"
    mission = args.out_root / "mission1"
    write_jsonl(
        gameplay / "update_phases.jsonl",
        [row for row in rows if row["event"] in {
            "ac6-gameplay-phase", "ac6-gameplay-tick", "ac6-gameplay-object-pipeline"
        }],
    )
    write_jsonl(
        gameplay / "mode_transitions.jsonl",
        [row for row in rows if row["event"] == "ac6-gameplay-mode-task"],
    )
    write_jsonl(
        gameplay / "mission_hsm_transitions.jsonl",
        [row for row in rows if row["event"] == "ac6-gameplay-tick"],
    )
    write_jsonl(
        gameplay / "input_events.jsonl",
        [row for row in rows if row["event"] in {
            "xam-input", "ac6-gameplay-canonical-input"
        }],
    )
    write_jsonl(
        mission / "unit_registry_snapshots.jsonl",
        [row for row in rows if row["event"] in {
            "ac6-gameplay-unit-census", "ac6-gameplay-player-update",
            "ac6-gameplay-child-dispatch"
        }],
    )
    print(f"extracted {len(rows)} bounded events for {args.run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
