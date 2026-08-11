#!/usr/bin/env python3
"""Normalize bounded AC6_recomp JSONL probe events into a qualified trace."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

from audit_ac6_oracle_manifest import ManifestError, sha256, validate_document

TRACE_SCHEMA = "ac6.recomp-oracle-trace.v1"
SHA256 = re.compile(r"[0-9a-f]{64}")
EVENT_KEYS = {"tick", "guest_address", "input", "graphics", "output_hashes"}


class TraceError(ValueError):
    pass


def guest_address(value: object) -> str:
    if isinstance(value, str):
        try:
            number = int(value, 0)
        except ValueError as error:
            raise TraceError("guest_address is not an integer") from error
    elif isinstance(value, int) and not isinstance(value, bool):
        number = value
    else:
        raise TraceError("guest_address type")
    if not 0 <= number <= 0xFFFFFFFF:
        raise TraceError("guest_address outside the 32-bit guest space")
    return f"0x{number:08X}"


def normalize_input(value: object) -> dict:
    if not isinstance(value, dict) or set(value) != {"axes", "triggers", "buttons"}:
        raise TraceError("input shape")
    axes, triggers, buttons = value["axes"], value["triggers"], value["buttons"]
    if not isinstance(axes, dict) or not all(
            isinstance(k, str) and isinstance(v, int) and not isinstance(v, bool) and
            -32768 <= v <= 32767 for k, v in axes.items()):
        raise TraceError("input axes")
    if not isinstance(triggers, dict) or not all(
            isinstance(k, str) and isinstance(v, int) and not isinstance(v, bool) and
            0 <= v <= 255 for k, v in triggers.items()):
        raise TraceError("input triggers")
    if not isinstance(buttons, list) or not all(isinstance(button, str) for button in buttons):
        raise TraceError("input buttons")
    if len(buttons) != len(set(buttons)):
        raise TraceError("duplicate input button")
    return {"axes": dict(sorted(axes.items())), "triggers": dict(sorted(triggers.items())),
            "buttons": sorted(buttons)}


def normalize_event(value: object, sequence: int, previous_tick: int) -> tuple[dict, int]:
    if not isinstance(value, dict) or set(value) != EVENT_KEYS:
        raise TraceError(f"event {sequence} shape")
    tick = value["tick"]
    if not isinstance(tick, int) or isinstance(tick, bool) or tick < previous_tick:
        raise TraceError(f"event {sequence} tick")
    graphics = value["graphics"]
    if not isinstance(graphics, dict):
        raise TraceError(f"event {sequence} graphics")
    output_hashes = value["output_hashes"]
    if not isinstance(output_hashes, dict) or not output_hashes:
        raise TraceError(f"event {sequence} output_hashes")
    for name, digest in output_hashes.items():
        if not isinstance(name, str) or not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise TraceError(f"event {sequence} output hash")
    event = {
        "sequence": sequence,
        "tick": tick,
        "guest_address": guest_address(value["guest_address"]),
        "input": normalize_input(value["input"]),
        "graphics": graphics,
        "output_hashes": dict(sorted(output_hashes.items())),
    }
    return event, tick


def load_events(path: Path, maximum: int) -> list[dict]:
    events: list[dict] = []
    previous_tick = 0
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            if len(events) >= maximum:
                raise TraceError("event bound exceeded")
            try:
                raw = json.loads(line)
            except json.JSONDecodeError as error:
                raise TraceError(f"line {line_number}: {error.msg}") from error
            event, previous_tick = normalize_event(raw, len(events), previous_tick)
            events.append(event)
    if not events:
        raise TraceError("empty trace")
    return events


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("probe")
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        validate_document(manifest, args.artifact_root)
        probe = next((item for item in manifest["probes"] if item["name"] == args.probe), None)
        if probe is None:
            raise TraceError(f"unknown probe: {args.probe}")
        contract_path = args.artifact_root / probe["contract"]
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        maximum = contract.get("bounds", {}).get("max_events")
        if not isinstance(maximum, int) or maximum <= 0:
            raise TraceError("probe max_events")
        events = load_events(args.raw_jsonl, maximum)
        trace = {
            "schema": TRACE_SCHEMA,
            "qualification": {
                "manifest": str(args.manifest),
                "manifest_sha256": sha256(args.manifest),
                "oracle_commit": manifest["oracle"]["commit"],
                "xex_sha256": manifest["target"]["sha256"],
                "probe": args.probe,
                "probe_contract_sha256": probe["sha256"],
                "raw_sha256": sha256(args.raw_jsonl),
            },
            "event_count": len(events),
            "events": events,
        }
        args.output.write_text(json.dumps(trace, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, json.JSONDecodeError, ManifestError, TraceError) as error:
        print(f"oracle_trace=fail reason={error}")
        return 1
    print(f"oracle_trace=pass events={len(events)} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
