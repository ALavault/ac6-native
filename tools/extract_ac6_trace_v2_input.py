#!/usr/bin/env python3
"""Extract the arm-gated controller TSV from a qualified trace-v2 JSONL."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from build_ac6_execution_trace_v2 import DOMAINS, TraceV2Error, load_jsonl


def extract(raw: Path, start_tick: int, tick_count: int) -> str:
    try:
        events = load_jsonl(raw, start_tick, tick_count)
    except TraceV2Error as error:
        raise ValueError(str(error)) from error
    rows: list[str] = []
    expected_tick = start_tick
    for offset in range(0, len(events), len(DOMAINS)):
        group = events[offset:offset + len(DOMAINS)]
        input_event = group[0]
        if input_event["domain"] != "controller_input" or input_event["tick"] != expected_tick:
            raise ValueError(f"controller event tick {expected_tick}")
        payload = input_event["payload"]
        if set(payload) != {"pitch", "roll", "yaw", "throttle", "buttons"}:
            raise ValueError(f"controller event shape {expected_tick}")
        values = [payload[name] for name in ("pitch", "roll", "yaw", "throttle", "buttons")]
        limits = [(-32768, 32767), (-32768, 32767), (-32768, 32767), (0, 255), (0, 65535)]
        if any(not isinstance(value, int) or isinstance(value, bool) or
               value < lower or value > upper
               for value, (lower, upper) in zip(values, limits)):
            raise ValueError(f"controller event range {expected_tick}")
        rows.append(f"{expected_tick} {values[0]} {values[1]} {values[2]} "
                    f"{values[3]} {values[4]}\n")
        expected_tick += 1
    if len(rows) != tick_count:
        raise ValueError("controller row count")
    return "".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--start-tick", type=int, default=1)
    parser.add_argument("--tick-count", type=int, default=3600)
    args = parser.parse_args()
    try:
        text = extract(args.raw_jsonl, args.start_tick, args.tick_count)
        args.output.write_text(text, encoding="utf-8", newline="")
    except (OSError, ValueError) as error:
        print(f"ac6_trace_v2_input=fail reason={error}")
        return 1
    print(f"ac6_trace_v2_input=pass rows={args.tick_count} "
          f"sha256={hashlib.sha256(text.encode()).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
