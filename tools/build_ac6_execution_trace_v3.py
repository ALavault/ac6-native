#!/usr/bin/env python3
"""Seal AC6 oracle/native observations as ac6.execution-trace.v3.

Version 3 separates the NTSC-U/J behavioural oracle from the PAL native
target.  Version 2 is accepted only by ``load_trace`` as historical input and
can never be emitted or promoted by this tool.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import tempfile
from pathlib import Path
from typing import Any

from ac6_controller_input_replay import (
    NTSC_UJ_MARKER_CONTRACT,
    NTSC_UJ_ORACLE_TARGET_IDENTITY,
    PAL_NATIVE_TARGET_IDENTITY,
    PROJECTION_RECEIPT_SCHEMA_V4,
)
from build_ac6_execution_trace_v2 import (
    MAX_ARTIFACT_BYTES,
    MAX_LINE_BYTES,
    MAX_RAW_BYTES,
    MAX_TRACE_BYTES,
    MAX_TRACE_TICKS,
    TraceV2Error,
    load_jsonl_bytes as load_v2_jsonl_bytes,
)


TRACE_SCHEMA = "ac6.execution-trace.v3"
HISTORICAL_SCHEMA = "ac6.execution-trace.v2"
ORACLE_COMMIT = "ab90b54713e5889f33eee1cc8681dae89fe83d1e"
ORACLE_TREE = "1e60427e316a2667d189eb1e067a8ec7d776fd50"
IDENTITY_SCHEMA = "ac6.recomp-oracle-identity.v1"
DOMAINS = ("input", "simulation", "objectives", "graphics", "media", "hashes")
EVENT_KEYS = {"sequence", "tick", "domain", "payload"}
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{40}")
MAX_BUILD_BYTES = 16 * 1024 * 1024
MAX_RECEIPT_BYTES = 64 * 1024


class TraceV3Error(ValueError):
    pass


def _read(path: Path, limit: int, label: str) -> bytes:
    size = path.stat().st_size
    if size > limit:
        raise TraceV3Error(f"{label} byte bound")
    data = path.read_bytes()
    if len(data) != size:
        raise TraceV3Error(f"{label} changed while reading")
    return data


def _json(data: bytes, label: str) -> dict[str, Any]:
    try:
        value = json.loads(data, parse_constant=lambda value: (_ for _ in ()).throw(
            TraceV3Error(f"{label} non-finite number: {value}")))
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise TraceV3Error(f"{label} JSON") from error
    if not isinstance(value, dict):
        raise TraceV3Error(f"{label} shape")
    return value


def _artifact(path: Path, limit: int, label: str) -> tuple[dict[str, Any], bytes]:
    data = _read(path, limit, label)
    return {
        "path": path.as_posix(),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }, data


def _validate_artifact(value: object, label: str, *, with_id: bool = False) -> None:
    keys = {"path", "size", "sha256"} | ({"id"} if with_id else set())
    if (not isinstance(value, dict) or set(value) != keys or
            not isinstance(value.get("path"), str) or not value["path"] or
            not isinstance(value.get("size"), int) or isinstance(value["size"], bool) or
            value["size"] < 0 or SHA256.fullmatch(str(value.get("sha256"))) is None or
            (with_id and (not isinstance(value.get("id"), str) or not value["id"]))):
        raise TraceV3Error(f"{label} artifact")


def _oracle(identity_path: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    artifact, data = _artifact(identity_path, MAX_BUILD_BYTES, "oracle identity")
    identity = _json(data, "oracle identity")
    reference = identity.get("oracle_reference", {})
    target = identity.get("target", {})
    if (identity.get("schema") != IDENTITY_SCHEMA or
            reference.get("commit") != ORACLE_COMMIT or
            reference.get("tree") != ORACLE_TREE):
        raise TraceV3Error("oracle revision identity")
    normalized_target = {
        "target_id": target.get("target_id"),
        "title_id": target.get("title_id"),
        "media_id": target.get("media_id"),
        "module": target.get("module"),
        "xex_sha256": target.get("sha256"),
        "xex_version": target.get("xex_version"),
        "base_version": target.get("base_version"),
        "module_xxh3": str(target.get("module_xxh3", "")).upper(),
        "entry_point": str(target.get("entry_point", "")).removeprefix("0x"),
        "region_mask": str(target.get("region_mask", "")).removeprefix("0x"),
    }
    if normalized_target != NTSC_UJ_ORACLE_TARGET_IDENTITY:
        raise TraceV3Error("oracle target identity")
    return artifact, {
        "implementation_commit": ORACLE_COMMIT,
        "implementation_tree": ORACLE_TREE,
        "identity": artifact,
        "target": normalized_target,
        "marker": NTSC_UJ_MARKER_CONTRACT,
    }


def _receipt(path: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    artifact, data = _artifact(path, MAX_RECEIPT_BYTES, "projection receipt")
    receipt = _json(data, "projection receipt")
    source = receipt.get("source", {})
    oracle = source.get("oracle", {}) if isinstance(source, dict) else {}
    if (receipt.get("kind") != "native_projection_receipt" or
            receipt.get("schema") != PROJECTION_RECEIPT_SCHEMA_V4 or
            receipt.get("native_target") != PAL_NATIVE_TARGET_IDENTITY or
            source.get("raw_schema") != "ac6.controller-input-replay.v4" or
            oracle.get("target") != NTSC_UJ_ORACLE_TARGET_IDENTITY or
            oracle.get("marker_contract") != NTSC_UJ_MARKER_CONTRACT):
        raise TraceV3Error("projection receipt lineage")
    return artifact, receipt


def _events(data: bytes, start_tick: int, tick_count: int) -> list[dict[str, Any]]:
    if len(data) > MAX_RAW_BYTES:
        raise TraceV3Error("raw capture byte bound")
    events: list[dict[str, Any]] = []
    for line_number, line in enumerate(data.splitlines(), 1):
        if len(line) > MAX_LINE_BYTES:
            raise TraceV3Error("raw capture line byte bound")
        try:
            event = json.loads(line)
        except (json.JSONDecodeError, UnicodeDecodeError) as error:
            raise TraceV3Error(f"raw capture line {line_number} JSON") from error
        sequence = len(events)
        expected_tick = start_tick + sequence // len(DOMAINS)
        expected_domain = DOMAINS[sequence % len(DOMAINS)]
        if (not isinstance(event, dict) or set(event) != EVENT_KEYS or
                event.get("sequence") != sequence or event.get("tick") != expected_tick or
                event.get("domain") != expected_domain or
                not isinstance(event.get("payload"), dict)):
            raise TraceV3Error(f"raw capture event {sequence} contract")
        events.append(event)
    if len(events) != tick_count * len(DOMAINS):
        raise TraceV3Error("raw capture event count")
    return events


def build_trace(raw_path: Path, role: str, identity_path: Path, receipt_path: Path,
                patch_stack: Path, binary: Path, build_manifest: Path, replay: Path,
                probe: Path, probe_id: str, native_commit: str, window_id: str,
                start_tick: int, tick_count: int) -> dict[str, Any]:
    if role not in {"oracle", "native"} or COMMIT.fullmatch(native_commit) is None:
        raise TraceV3Error("producer identity")
    if not probe_id or not window_id or start_tick < 1 or not 0 < tick_count <= MAX_TRACE_TICKS:
        raise TraceV3Error("window identity")
    _, oracle = _oracle(identity_path)
    receipt_artifact, receipt = _receipt(receipt_path)
    patch_artifact, _ = _artifact(patch_stack, MAX_BUILD_BYTES, "patch stack")
    binary_artifact, _ = _artifact(binary, MAX_ARTIFACT_BYTES, "binary")
    build_artifact, _ = _artifact(build_manifest, MAX_BUILD_BYTES, "build manifest")
    replay_artifact, _ = _artifact(replay, MAX_ARTIFACT_BYTES, "replay")
    probe_artifact, _ = _artifact(probe, MAX_BUILD_BYTES, "probe")
    capture_artifact, raw = _artifact(raw_path, MAX_RAW_BYTES, "raw capture")
    events = _events(raw, start_tick, tick_count)
    cadence = receipt.get("cadence", {})
    source_hz = cadence.get("source_hz")
    native_hz = cadence.get("native_hz")
    hold = cadence.get("hold")
    if (source_hz, native_hz, hold) not in {(30, 60, 2), (60, 60, 1)}:
        raise TraceV3Error("qualified cadence")
    return {
        "schema": TRACE_SCHEMA,
        "header": {
            "role": role,
            "oracle": oracle,
            "projection_receipt": {
                "schema": PROJECTION_RECEIPT_SCHEMA_V4, **receipt_artifact},
            "native_target": PAL_NATIVE_TARGET_IDENTITY,
            "producer": {
                "native_commit": native_commit,
                "patch_stack": patch_artifact,
                "binary": binary_artifact,
                "build": build_artifact,
                "probe": {"id": probe_id, **probe_artifact},
            },
            "replay": replay_artifact,
            "capture": capture_artifact,
            "window": {
                "id": window_id,
                "start_tick": start_tick,
                "tick_count": tick_count,
                "cadence": {"source_hz": source_hz, "native_hz": native_hz, "hold": hold},
                "domains": list(DOMAINS),
            },
        },
        "event_count": len(events),
        "events": events,
    }


def load_trace(path: Path) -> tuple[str, dict[str, Any]]:
    """Read v3, or v2 with an explicit historical-only classification."""
    data = _read(path, MAX_TRACE_BYTES, "execution trace")
    trace = _json(data, "execution trace")
    if trace.get("schema") == HISTORICAL_SCHEMA:
        header = trace.get("header", {})
        window = header.get("window", {}) if isinstance(header, dict) else {}
        load_v2_jsonl_bytes(
            ("\n".join(json.dumps(event, separators=(",", ":")) for event in trace.get("events", [])) + "\n").encode(),
            window.get("start_tick"), window.get("tick_count"),
        )
        return "historical-v2", trace
    if trace.get("schema") != TRACE_SCHEMA:
        raise TraceV3Error("execution trace schema")
    header = trace.get("header", {})
    if not isinstance(header, dict) or set(header) != {
            "role", "oracle", "projection_receipt", "native_target", "producer",
            "replay", "capture", "window"}:
        raise TraceV3Error("execution trace header")
    oracle = header["oracle"]
    if (header["role"] not in {"oracle", "native"} or
            not isinstance(oracle, dict) or set(oracle) != {
                "implementation_commit", "implementation_tree", "identity", "target", "marker"} or
            oracle["implementation_commit"] != ORACLE_COMMIT or
            oracle["implementation_tree"] != ORACLE_TREE or
            oracle["target"] != NTSC_UJ_ORACLE_TARGET_IDENTITY or
            oracle["marker"] != NTSC_UJ_MARKER_CONTRACT or
            header["native_target"] != PAL_NATIVE_TARGET_IDENTITY):
        raise TraceV3Error("execution trace lineage")
    _validate_artifact(oracle["identity"], "oracle identity")
    receipt = header["projection_receipt"]
    if not isinstance(receipt, dict) or receipt.get("schema") != PROJECTION_RECEIPT_SCHEMA_V4:
        raise TraceV3Error("execution trace receipt")
    _validate_artifact({key: value for key, value in receipt.items() if key != "schema"},
                       "projection receipt")
    producer = header["producer"]
    if (not isinstance(producer, dict) or set(producer) != {
            "native_commit", "patch_stack", "binary", "build", "probe"} or
            COMMIT.fullmatch(str(producer.get("native_commit"))) is None):
        raise TraceV3Error("execution trace producer")
    for key in ("patch_stack", "binary", "build"):
        _validate_artifact(producer[key], f"producer {key}")
    _validate_artifact(producer["probe"], "producer probe", with_id=True)
    _validate_artifact(header["replay"], "replay")
    _validate_artifact(header["capture"], "capture")
    window = header.get("window", {}) if isinstance(header, dict) else {}
    if (not isinstance(window, dict) or set(window) != {
            "id", "start_tick", "tick_count", "cadence", "domains"} or
            not isinstance(window["id"], str) or not window["id"] or
            window["domains"] != list(DOMAINS) or
            window["cadence"] not in (
                {"source_hz": 30, "native_hz": 60, "hold": 2},
                {"source_hz": 60, "native_hz": 60, "hold": 1})):
        raise TraceV3Error("execution trace window")
    encoded = ("\n".join(json.dumps(event, separators=(",", ":")) for event in trace.get("events", [])) + "\n").encode()
    _events(encoded, window.get("start_tick"), window.get("tick_count"))
    if trace.get("event_count") != len(trace.get("events", [])):
        raise TraceV3Error("execution trace event count")
    return "current-v3", trace


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        Path(temporary).unlink(missing_ok=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw_jsonl", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--role", choices=("oracle", "native"), required=True)
    parser.add_argument("--oracle-identity", type=Path, required=True)
    parser.add_argument("--projection-receipt", type=Path, required=True)
    parser.add_argument("--patch-stack", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--build-manifest", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--probe-id", required=True)
    parser.add_argument("--native-commit", required=True)
    parser.add_argument("--window", required=True)
    parser.add_argument("--start-tick", type=int, required=True)
    parser.add_argument("--tick-count", type=int, required=True)
    arguments = parser.parse_args()
    try:
        trace = build_trace(arguments.raw_jsonl, arguments.role, arguments.oracle_identity,
                            arguments.projection_receipt, arguments.patch_stack,
                            arguments.binary, arguments.build_manifest, arguments.replay,
                            arguments.probe, arguments.probe_id, arguments.native_commit,
                            arguments.window, arguments.start_tick, arguments.tick_count)
        _atomic_write(arguments.output,
                      (json.dumps(trace, indent=2, sort_keys=True, allow_nan=False) + "\n").encode())
    except (OSError, TraceV2Error, TraceV3Error) as error:
        print(f"execution_trace_v3=fail reason={error}")
        return 1
    print(f"execution_trace_v3=pass role={arguments.role} ticks={arguments.tick_count} events={trace['event_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
