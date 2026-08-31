#!/usr/bin/env python3
"""Convert a bounded, read-only oracle event log into an AC6 Xenos capsule.

The input is an event log emitted by a separately instrumented oracle.  This
tool never launches that oracle, never writes guest memory, and rejects guest
writes or raw retail-byte fields before producing the product-neutral capsule.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import sys
from typing import Any


PRODUCT = pathlib.Path(__file__).resolve().parents[1]
CAPSULE_TOOL = PRODUCT / "tools/xenos_capsule.py"
SPEC = importlib.util.spec_from_file_location("xenos_capsule", CAPSULE_TOOL)
if SPEC is None or SPEC.loader is None:  # pragma: no cover - packaging error
    raise RuntimeError("cannot load xenos capsule validator")
CAPSULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPSULE)


class CaptureError(RuntimeError):
    pass


def read_events(path: pathlib.Path, limit: int = 1_000_000) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if len(events) >= limit:
                raise CaptureError("oracle event log exceeds capsule bound")
            if not line.strip():
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError as error:
                raise CaptureError(f"invalid event JSON at line {line_number}") from error
            if not isinstance(event, dict):
                raise CaptureError(f"event at line {line_number} is not an object")
            if event.get("kind") in {"guest_write", "retail_bytes", "socket"}:
                raise CaptureError(f"forbidden oracle effect at line {line_number}")
            events.append(event)
    if not events:
        raise CaptureError("oracle event log is empty")
    return events


def capture(
    event_log: pathlib.Path,
    output: pathlib.Path,
    xex_sha256: str,
    iso_sha256: str,
    ghidra_project: str,
    route_sha256: str,
    ring_dword_count: int,
) -> dict[str, Any]:
    document = {
        "schema": CAPSULE.SCHEMA,
        "target": "ntsc-uj",
        "source": {
            "xex_sha256": xex_sha256,
            "iso_sha256": iso_sha256,
            "ghidra_project": ghidra_project,
            "route_sha256": route_sha256,
        },
        "ring": {"dword_count": ring_dword_count},
        "events": read_events(event_log),
    }
    canonical = CAPSULE.canonical_bytes(document)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(canonical)
    return json.loads(canonical)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--xex-sha256", required=True)
    parser.add_argument("--iso-sha256", required=True)
    parser.add_argument("--ghidra-project", required=True)
    parser.add_argument("--route-sha256", required=True)
    parser.add_argument("--ring-dword-count", required=True, type=int)
    args = parser.parse_args(argv)
    try:
        result = capture(
            args.input, args.output, args.xex_sha256, args.iso_sha256,
            args.ghidra_project, args.route_sha256, args.ring_dword_count,
        )
        print(json.dumps(result["validated"], sort_keys=True))
        return 0
    except (OSError, CAPSULE.CapsuleError, CaptureError) as error:
        print(f"capture: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
