#!/usr/bin/env python3
"""Report the first deterministic divergence between AC6 execution traces v2."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import stat
import tempfile
from pathlib import Path
from typing import Any

from build_ac6_execution_trace_v2 import (
    DOMAINS,
    MAX_TRACE_TICKS,
    TRACE_SCHEMA,
    TraceV2Error,
    validate_cadence,
    validate_controller_replay_contract,
    validate_events,
    validate_observation,
)


REPORT_SCHEMA = "ac6.execution-first-divergence.v2"
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
ARTIFACT_KEYS = {"path", "sha256"}
LEGACY_WINDOW_KEYS = {"id", "start_tick", "tick_count", "sample_hz", "cadence", "domains"}
RECEIPT_WINDOW_KEYS = LEGACY_WINDOW_KEYS | {"controller_replay", "observation"}
MAX_TRACE_BYTES = 1024 * 1024 * 1024
MAX_TOTAL_TRACE_BYTES = 1024 * 1024 * 1024
MAX_VALUE_DEPTH = 64
MAX_VALUE_NODES = 1_000_000
MAX_CONTAINER_ITEMS = 1_000_000
MAX_STRING_BYTES = 1024 * 1024
MAX_JSON_INTEGER_DIGITS = 20


class ComparisonError(ValueError):
    pass


def _reject_json_constant(value: str) -> None:
    raise ComparisonError(f"non-finite JSON number: {value}")


def _parse_json_int(value: str) -> int:
    if len(value.removeprefix("-")) > MAX_JSON_INTEGER_DIGITS:
        raise ComparisonError("JSON integer bound")
    return int(value)


def _validate_json_nesting(data: bytes, name: str) -> None:
    depth = 0
    in_string = False
    escaped = False
    for byte in data:
        if in_string:
            if escaped:
                escaped = False
            elif byte == 0x5C:
                escaped = True
            elif byte == 0x22:
                in_string = False
            continue
        if byte == 0x22:
            in_string = True
        elif byte in (0x5B, 0x7B):
            depth += 1
            if depth > MAX_VALUE_DEPTH:
                raise ComparisonError(f"{name} depth bound")
        elif byte in (0x5D, 0x7D):
            depth -= 1
            if depth < 0:
                raise ComparisonError(f"{name} nesting")
    if in_string or depth != 0:
        raise ComparisonError(f"{name} nesting")


def _read_trace_snapshot(
    path: Path,
    name: str,
    maximum_bytes: int | None = None,
    *,
    total_limited: bool = False,
) -> tuple[object, str, int]:
    limit = MAX_TRACE_BYTES if maximum_bytes is None else maximum_bytes
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NONBLOCK)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise ComparisonError(f"{name} is not a regular file")
        with os.fdopen(descriptor, "rb") as source:
            descriptor = -1
            data = source.read(limit + 1)
            if len(data) > limit or source.read(1):
                reason = "comparison total byte bound" if total_limited else f"{name} byte bound"
                raise ComparisonError(reason)
    except OSError as error:
        raise ComparisonError(f"{name} unreadable: {error}") from error
    finally:
        if descriptor >= 0:
            os.close(descriptor)
    _validate_json_nesting(data, name)
    try:
        document = json.loads(
            data,
            parse_int=_parse_json_int,
            parse_constant=_reject_json_constant,
        )
    except UnicodeDecodeError as error:
        raise ComparisonError(f"{name} utf-8") from error
    except json.JSONDecodeError as error:
        raise ComparisonError(f"{name} JSON: {error.msg}") from error
    except RecursionError as error:
        raise ComparisonError(f"{name} JSON recursion bound") from error
    return document, hashlib.sha256(data).hexdigest(), len(data)


def _paths_alias(left: Path, right: Path) -> bool:
    if left.resolve(strict=False) == right.resolve(strict=False):
        return True
    try:
        return left.exists() and right.exists() and left.samefile(right)
    except OSError:
        return False


def _atomic_write(output: Path, data: bytes) -> None:
    descriptor = -1
    temporary: Path | None = None
    try:
        descriptor, temporary_name = tempfile.mkstemp(prefix=f".{output.name}.", suffix=".tmp", dir=output.parent)
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            descriptor = -1
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
        temporary = None
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def validate_artifact(value: object, name: str) -> dict[str, str]:
    if (
        not isinstance(value, dict)
        or set(value) != ARTIFACT_KEYS
        or not isinstance(value["path"], str)
        or not value["path"]
        or not isinstance(value["sha256"], str)
        or SHA256.fullmatch(value["sha256"]) is None
    ):
        raise ComparisonError(name)
    return value


def _validate_value_bounds(value: object, where: str) -> None:
    pending: list[tuple[object, int]] = [(value, 0)]
    nodes = 0
    while pending:
        current, depth = pending.pop()
        nodes += 1
        if nodes > MAX_VALUE_NODES:
            raise ComparisonError(f"{where} node bound")
        if depth > MAX_VALUE_DEPTH:
            raise ComparisonError(f"{where} depth bound")
        if isinstance(current, dict):
            if len(current) > MAX_CONTAINER_ITEMS:
                raise ComparisonError(f"{where} member bound")
            for key, child in current.items():
                if not isinstance(key, str) or len(key.encode()) > MAX_STRING_BYTES:
                    raise ComparisonError(f"{where} key bound")
                pending.append((child, depth + 1))
        elif isinstance(current, list):
            if len(current) > MAX_CONTAINER_ITEMS:
                raise ComparisonError(f"{where} item bound")
            pending.extend((child, depth + 1) for child in current)
        elif isinstance(current, str) and len(current.encode()) > MAX_STRING_BYTES:
            raise ComparisonError(f"{where} string bound")
        elif isinstance(current, float) and not math.isfinite(current):
            raise ComparisonError(f"{where} non-finite number")


def validate_trace(
    document: object,
    role: str,
    maximum_events: int = 3_000_000,
    *,
    allow_legacy_diagnostic: bool = False,
) -> dict[str, Any]:
    if not isinstance(document, dict) or set(document) != {"schema", "header", "event_count", "events"}:
        raise ComparisonError(f"{role} shape")
    if document["schema"] != TRACE_SCHEMA:
        raise ComparisonError(f"{role} schema")
    header = document["header"]
    _validate_value_bounds(header, f"{role} header")
    if not isinstance(header, dict) or set(header) != {
        "role",
        "target",
        "commits",
        "patch_stack",
        "binary",
        "replay",
        "probe",
        "capture",
        "window",
    }:
        raise ComparisonError(f"{role} header")
    if header["role"] != role:
        raise ComparisonError(f"{role} role")
    target = header["target"]
    if target != {"module": "default.xex", "xex_sha256": XEX_SHA256}:
        raise ComparisonError(f"{role} target")
    commits = header["commits"]
    if (
        not isinstance(commits, dict)
        or set(commits) != {"oracle", "native"}
        or commits["oracle"] != ORACLE_COMMIT
        or not isinstance(commits["native"], str)
        or COMMIT.fullmatch(commits["native"]) is None
    ):
        raise ComparisonError(f"{role} commits")
    for key in ("patch_stack", "binary", "replay", "capture"):
        validate_artifact(header[key], f"{role} {key}")
    probe = header["probe"]
    if (
        not isinstance(probe, dict)
        or set(probe) != {"id", "path", "sha256"}
        or not isinstance(probe["id"], str)
        or not probe["id"]
    ):
        raise ComparisonError(f"{role} probe")
    validate_artifact({"path": probe.get("path"), "sha256": probe.get("sha256")}, f"{role} probe artifact")
    window = header["window"]
    if (
        not isinstance(window, dict)
        or frozenset(window) not in {frozenset(LEGACY_WINDOW_KEYS), frozenset(RECEIPT_WINDOW_KEYS)}
        or not isinstance(window["id"], str)
        or not window["id"]
        or not isinstance(window["start_tick"], int)
        or isinstance(window["start_tick"], bool)
        or not isinstance(window["tick_count"], int)
        or isinstance(window["tick_count"], bool)
        or window["start_tick"] < 1
        or window["tick_count"] <= 0
        or window["tick_count"] > MAX_TRACE_TICKS
        or window["start_tick"] > MAX_TRACE_TICKS - window["tick_count"] + 1
        or window["domains"] != list(DOMAINS)
    ):
        raise ComparisonError(f"{role} window")
    try:
        cadence = validate_cadence(window["cadence"], f"{role} cadence")
    except TraceV2Error as error:
        raise ComparisonError(str(error)) from error
    if window["sample_hz"] != cadence["oracle_update_hz"]:
        raise ComparisonError(f"{role} window sample hz")
    if "controller_replay" in window:
        try:
            replay_contract = validate_controller_replay_contract(
                window["controller_replay"], f"{role} controller replay"
            )
            source_markers = replay_contract["parent_window"]["marker_count"]
            validate_observation(window["observation"], cadence, source_markers, f"{role} observation")
        except TraceV2Error as error:
            raise ComparisonError(str(error)) from error
        if header["replay"]["sha256"] != replay_contract["projected_replay_sha256"]:
            raise ComparisonError(f"{role} replay receipt mismatch")
        if window["start_tick"] < 1 or window["start_tick"] > source_markers - window["tick_count"] + 1:
            raise ComparisonError(f"{role} window bounds")
    else:
        if not allow_legacy_diagnostic:
            raise ComparisonError(f"{role} unreceipted trace is diagnostic-only")
        if cadence != {
            "oracle_update_hz": 30,
            "native_simulation_hz": 60,
            "native_ticks_per_sample": 2,
            "input_resampling": "zero_order_hold",
            "snapshot_sampling": "last_native_tick_in_sample",
        }:
            raise ComparisonError(f"{role} unreceipted cadence")
    events = document["events"]
    if (
        not isinstance(events, list)
        or not isinstance(document["event_count"], int)
        or isinstance(document["event_count"], bool)
        or document["event_count"] != len(events)
        or len(events) > maximum_events
    ):
        raise ComparisonError(f"{role} event count")
    try:
        validate_events(events, window["start_tick"], window["tick_count"])
    except TraceV2Error as error:
        raise ComparisonError(f"{role} {error}") from error
    return document


def require_same_capture_contract(reference: dict[str, Any], candidate: dict[str, Any]) -> None:
    left = reference["header"]
    right = candidate["header"]
    comparisons = {
        "target": (left["target"], right["target"]),
        "commits": (left["commits"], right["commits"]),
        "patch_stack": (left["patch_stack"]["sha256"], right["patch_stack"]["sha256"]),
        "replay": (left["replay"]["sha256"], right["replay"]["sha256"]),
        "probe": ((left["probe"]["id"], left["probe"]["sha256"]), (right["probe"]["id"], right["probe"]["sha256"])),
        "window": (left["window"], right["window"]),
    }
    for name, (expected, actual) in comparisons.items():
        if expected != actual:
            raise ComparisonError(f"capture contract mismatch: {name}")


def first_value_difference(reference: Any, candidate: Any, path: str) -> dict[str, Any] | None:
    if type(reference) is not type(candidate):
        return {"path": path, "reference": reference, "candidate": candidate}
    if isinstance(reference, dict):
        for key in sorted(set(reference) | set(candidate)):
            child = f"{path}.{key}"
            if key not in reference:
                return {"path": child, "reference": "<missing>", "candidate": candidate[key]}
            if key not in candidate:
                return {"path": child, "reference": reference[key], "candidate": "<missing>"}
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
            return {"path": f"{path}.length", "reference": len(reference), "candidate": len(candidate)}
        return None
    if reference != candidate:
        return {"path": path, "reference": reference, "candidate": candidate}
    return None


def difference_kind(difference: dict[str, Any] | None) -> str:
    """Classify a first mismatch without assigning semantics to raw fields.

    A missing or extra object key is a producer/schema boundary.  It must not
    be reported as a gameplay value divergence: the oracle probe deliberately
    exposes raw guest words in some domains while the native producer exposes
    semantic fields.  Type and scalar/list differences remain value
    divergences and are still reported at the exact first path.
    """
    if difference is None:
        return "equal"
    if difference.get("reference") == "<missing>" or difference.get("candidate") == "<missing>":
        return "producer_schema_mismatch"
    return "value_divergence"


def compare_documents(
    reference: dict[str, Any],
    candidate: dict[str, Any],
    domains: tuple[str, ...] = DOMAINS,
    maximum_events: int = 3_000_000,
    *,
    allow_legacy_diagnostic: bool = False,
) -> dict[str, Any]:
    if not allow_legacy_diagnostic:
        raise ComparisonError("trace comparison gate is disabled until runner attestation is verified")
    reference = validate_trace(
        reference,
        "oracle",
        maximum_events,
        allow_legacy_diagnostic=allow_legacy_diagnostic,
    )
    candidate = validate_trace(
        candidate,
        "native",
        maximum_events,
        allow_legacy_diagnostic=allow_legacy_diagnostic,
    )
    if not domains or len(domains) != len(set(domains)) or any(domain not in DOMAINS for domain in domains):
        raise ComparisonError("comparison domains")
    require_same_capture_contract(reference, candidate)

    first: dict[str, Any] | None = None
    inspected = 0
    for expected, actual in zip(reference["events"], candidate["events"]):
        if expected["domain"] not in domains:
            continue
        inspected += 1
        first = first_value_difference(
            expected["payload"],
            actual["payload"],
            f"events[{expected['sequence']}].payload",
        )
        if first is not None:
            first.update(
                {
                    "sequence": expected["sequence"],
                    "tick": expected["tick"],
                    "domain": expected["domain"],
                }
            )
            break

    return {
        "schema": REPORT_SCHEMA,
        "equal": first is None,
        "comparison_kind": difference_kind(first),
        "proof_level": "structural_diagnostic",
        "domains": list(domains),
        "compared_events": inspected,
        "reference": {"schema": reference["schema"], "event_count": reference["event_count"]},
        "candidate": {"schema": candidate["schema"], "event_count": candidate["event_count"]},
        "first_divergence": first,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--max-events", type=int, default=3_000_000)
    parser.add_argument(
        "--allow-legacy-diagnostic",
        action="store_true",
        help="read old unreceipted 30/2 traces as non-gating diagnostics",
    )
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.max_events <= 0:
            raise ComparisonError("max-events")
        if arguments.report is not None and (
            _paths_alias(arguments.report, arguments.reference) or _paths_alias(arguments.report, arguments.candidate)
        ):
            raise ComparisonError("report aliases an input")
        domains = tuple(part.strip() for part in arguments.domains.split(",") if part.strip())
        reference_limit = min(MAX_TRACE_BYTES, MAX_TOTAL_TRACE_BYTES)
        reference, reference_sha256, reference_bytes = _read_trace_snapshot(
            arguments.reference,
            "reference",
            reference_limit,
            total_limited=reference_limit < MAX_TRACE_BYTES,
        )
        remaining = MAX_TOTAL_TRACE_BYTES - reference_bytes
        if remaining <= 0:
            raise ComparisonError("comparison total byte bound")
        candidate_limit = min(MAX_TRACE_BYTES, remaining)
        candidate, candidate_sha256, _ = _read_trace_snapshot(
            arguments.candidate,
            "candidate",
            candidate_limit,
            total_limited=candidate_limit < MAX_TRACE_BYTES,
        )
        report = compare_documents(
            reference,
            candidate,
            domains,
            arguments.max_events,
            allow_legacy_diagnostic=arguments.allow_legacy_diagnostic,
        )
        report["reference"].update({"path": str(arguments.reference), "sha256": reference_sha256})
        report["candidate"].update({"path": str(arguments.candidate), "sha256": candidate_sha256})
        if arguments.report is not None:
            _atomic_write(
                arguments.report,
                (json.dumps(report, indent=2, sort_keys=True) + "\n").encode(),
            )
    except (ComparisonError, OSError) as error:
        print(f"execution_trace_compare=fail reason={error}")
        return 2
    if report["equal"]:
        print(f"execution_trace_compare=equal proof_level={report['proof_level']} events={report['compared_events']}")
        return 0
    divergence = report["first_divergence"]
    print(
        f"execution_trace_compare={report['comparison_kind']} "
        f"sequence={divergence['sequence']} "
        f"tick={divergence['tick']} domain={divergence['domain']} "
        f"path={divergence['path']}"
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
