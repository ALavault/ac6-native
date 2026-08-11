#!/usr/bin/env python3
"""Report the first deterministic divergence between AC6 execution traces v2."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any

from build_ac6_execution_trace_v2 import (
    DOMAINS,
    TRACE_SCHEMA,
    TraceV2Error,
    validate_events,
)


REPORT_SCHEMA = "ac6.execution-first-divergence.v2"
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
ARTIFACT_KEYS = {"path", "sha256"}


class ComparisonError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_artifact(value: object, name: str) -> dict[str, str]:
    if (not isinstance(value, dict) or set(value) != ARTIFACT_KEYS or
            not isinstance(value["path"], str) or not value["path"] or
            not isinstance(value["sha256"], str) or
            SHA256.fullmatch(value["sha256"]) is None):
        raise ComparisonError(name)
    return value


def validate_trace(document: object, role: str,
                   maximum_events: int = 3_000_000) -> dict[str, Any]:
    if not isinstance(document, dict) or set(document) != {
            "schema", "header", "event_count", "events"}:
        raise ComparisonError(f"{role} shape")
    if document["schema"] != TRACE_SCHEMA:
        raise ComparisonError(f"{role} schema")
    header = document["header"]
    if not isinstance(header, dict) or set(header) != {
            "role", "target", "commits", "patch_stack", "binary", "replay",
            "probe", "capture", "window"}:
        raise ComparisonError(f"{role} header")
    if header["role"] != role:
        raise ComparisonError(f"{role} role")
    target = header["target"]
    if target != {"module": "default.xex", "xex_sha256": XEX_SHA256}:
        raise ComparisonError(f"{role} target")
    commits = header["commits"]
    if (not isinstance(commits, dict) or set(commits) != {"oracle", "native"} or
            commits["oracle"] != ORACLE_COMMIT or
            not isinstance(commits["native"], str) or
            COMMIT.fullmatch(commits["native"]) is None):
        raise ComparisonError(f"{role} commits")
    for key in ("patch_stack", "binary", "replay", "capture"):
        validate_artifact(header[key], f"{role} {key}")
    probe = header["probe"]
    if (not isinstance(probe, dict) or set(probe) != {"id", "path", "sha256"} or
            not isinstance(probe["id"], str) or not probe["id"]):
        raise ComparisonError(f"{role} probe")
    validate_artifact({"path": probe.get("path"), "sha256": probe.get("sha256")},
                      f"{role} probe artifact")
    window = header["window"]
    if (not isinstance(window, dict) or set(window) != {
            "id", "start_tick", "tick_count", "sample_hz", "cadence", "domains"} or
            not isinstance(window["id"], str) or not window["id"] or
            not isinstance(window["start_tick"], int) or isinstance(window["start_tick"], bool) or
            not isinstance(window["tick_count"], int) or isinstance(window["tick_count"], bool) or
            window["tick_count"] <= 0 or window["sample_hz"] != 30 or
            window["cadence"] != {
                "oracle_update_hz": 30,
                "native_simulation_hz": 60,
                "native_ticks_per_sample": 2,
                "input_resampling": "zero_order_hold",
                "snapshot_sampling": "last_native_tick_in_sample",
            } or
            window["domains"] != list(DOMAINS)):
        raise ComparisonError(f"{role} window")
    events = document["events"]
    if (not isinstance(document["event_count"], int) or
            document["event_count"] != len(events) or len(events) > maximum_events):
        raise ComparisonError(f"{role} event count")
    try:
        validate_events(events, window["start_tick"], window["tick_count"])
    except TraceV2Error as error:
        raise ComparisonError(f"{role} {error}") from error
    return document


def require_same_capture_contract(reference: dict[str, Any],
                                  candidate: dict[str, Any]) -> None:
    left = reference["header"]
    right = candidate["header"]
    comparisons = {
        "target": (left["target"], right["target"]),
        "commits": (left["commits"], right["commits"]),
        "patch_stack": (left["patch_stack"]["sha256"],
                        right["patch_stack"]["sha256"]),
        "replay": (left["replay"]["sha256"], right["replay"]["sha256"]),
        "probe": ((left["probe"]["id"], left["probe"]["sha256"]),
                  (right["probe"]["id"], right["probe"]["sha256"])),
        "window": (left["window"], right["window"]),
    }
    for name, (expected, actual) in comparisons.items():
        if expected != actual:
            raise ComparisonError(f"capture contract mismatch: {name}")


def first_value_difference(reference: Any, candidate: Any,
                           path: str) -> dict[str, Any] | None:
    if type(reference) is not type(candidate):
        return {"path": path, "reference": reference, "candidate": candidate}
    if isinstance(reference, dict):
        for key in sorted(set(reference) | set(candidate)):
            child = f"{path}.{key}"
            if key not in reference:
                return {"path": child, "reference": "<missing>",
                        "candidate": candidate[key]}
            if key not in candidate:
                return {"path": child, "reference": reference[key],
                        "candidate": "<missing>"}
            difference = first_value_difference(reference[key], candidate[key], child)
            if difference is not None:
                return difference
        return None
    if isinstance(reference, list):
        for index, (expected, actual) in enumerate(zip(reference, candidate)):
            difference = first_value_difference(expected, actual, f"{path}[{index}]")
            if difference is not None:
                return difference
        if len(reference) != len(candidate):
            return {"path": f"{path}.length", "reference": len(reference),
                    "candidate": len(candidate)}
        return None
    if reference != candidate:
        return {"path": path, "reference": reference, "candidate": candidate}
    return None


def compare_documents(reference: dict[str, Any], candidate: dict[str, Any],
                      domains: tuple[str, ...] = DOMAINS,
                      maximum_events: int = 3_000_000) -> dict[str, Any]:
    reference = validate_trace(reference, "oracle", maximum_events)
    candidate = validate_trace(candidate, "native", maximum_events)
    if (not domains or len(domains) != len(set(domains)) or
            any(domain not in DOMAINS for domain in domains)):
        raise ComparisonError("comparison domains")
    require_same_capture_contract(reference, candidate)

    first: dict[str, Any] | None = None
    inspected = 0
    for expected, actual in zip(reference["events"], candidate["events"]):
        if expected["domain"] not in domains:
            continue
        inspected += 1
        first = first_value_difference(
            expected["payload"], actual["payload"],
            f"events[{expected['sequence']}].payload",
        )
        if first is not None:
            first.update({
                "sequence": expected["sequence"],
                "tick": expected["tick"],
                "domain": expected["domain"],
            })
            break

    return {
        "schema": REPORT_SCHEMA,
        "equal": first is None,
        "domains": list(domains),
        "compared_events": inspected,
        "reference": {"schema": reference["schema"],
                      "event_count": reference["event_count"]},
        "candidate": {"schema": candidate["schema"],
                      "event_count": candidate["event_count"]},
        "first_divergence": first,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--max-events", type=int, default=3_000_000)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.max_events <= 0:
            raise ComparisonError("max-events")
        domains = tuple(part.strip() for part in arguments.domains.split(",") if part.strip())
        reference = json.loads(arguments.reference.read_text(encoding="utf-8"))
        candidate = json.loads(arguments.candidate.read_text(encoding="utf-8"))
        report = compare_documents(reference, candidate, domains, arguments.max_events)
        report["reference"].update(
            {"path": str(arguments.reference), "sha256": sha256(arguments.reference)})
        report["candidate"].update(
            {"path": str(arguments.candidate), "sha256": sha256(arguments.candidate)})
        if arguments.report is not None:
            arguments.report.write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
    except (ComparisonError, OSError, json.JSONDecodeError) as error:
        print(f"execution_trace_compare=fail reason={error}")
        return 2
    if report["equal"]:
        print(f"execution_trace_compare=equal events={report['compared_events']}")
        return 0
    divergence = report["first_divergence"]
    print(f"execution_trace_compare=diverged sequence={divergence['sequence']} "
          f"tick={divergence['tick']} domain={divergence['domain']} "
          f"path={divergence['path']}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
