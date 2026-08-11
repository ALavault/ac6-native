from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from build_ac6_execution_trace_v2 import (  # noqa: E402
    DOMAINS,
    TRACE_SCHEMA,
    TraceV2Error,
    build_trace,
    validate_events,
)
from compare_ac6_execution_traces import (  # noqa: E402
    ComparisonError,
    ORACLE_COMMIT,
    XEX_SHA256,
    compare_documents,
)


NATIVE_COMMIT = "9" * 40


def payload(domain: str, tick: int, digest: str = "a") -> dict:
    if domain == "controller_input":
        return {"pitch": 0, "roll": 0, "yaw": 0, "throttle": 0, "buttons": 0}
    if domain == "simulation_snapshot":
        return {"tick": tick, "position": [0.0, 1.0, 2.0]}
    if domain == "mission_objectives":
        return {"state": 2, "sub_mission": 0, "step": tick, "objectives": []}
    if domain == "graphics_submission":
        return {"backend": "headless", "draw_packets": 0}
    return {"simulation": digest * 64}


def events(ticks: int = 2) -> list[dict]:
    result = []
    for tick in range(1, ticks + 1):
        for domain in DOMAINS:
            result.append({
                "sequence": len(result),
                "tick": tick,
                "domain": domain,
                "payload": payload(domain, tick),
            })
    return result


def artifact(digest: str) -> dict:
    return {"path": "fixture", "sha256": digest * 64}


def trace(role: str, ticks: int = 2) -> dict:
    return {
        "schema": TRACE_SCHEMA,
        "header": {
            "role": role,
            "target": {"module": "default.xex", "xex_sha256": XEX_SHA256},
            "commits": {"oracle": ORACLE_COMMIT, "native": NATIVE_COMMIT},
            "patch_stack": artifact("1"),
            "binary": artifact("2" if role == "oracle" else "3"),
            "replay": artifact("4"),
            "probe": {"id": "mission01-controlled-sortie", **artifact("5")},
            "capture": artifact("6" if role == "oracle" else "7"),
            "window": {
                "id": "controlled-sortie",
                "start_tick": 1,
                "tick_count": ticks,
                "sample_hz": 30,
                "cadence": {
                    "oracle_update_hz": 30,
                    "native_simulation_hz": 60,
                    "native_ticks_per_sample": 2,
                    "input_resampling": "zero_order_hold",
                    "snapshot_sampling": "last_native_tick_in_sample",
                },
                "domains": list(DOMAINS),
            },
        },
        "event_count": ticks * len(DOMAINS),
        "events": events(ticks),
    }


def test_equal_cross_implementation_trace() -> None:
    report = compare_documents(trace("oracle"), trace("native"))
    assert report["equal"] is True
    assert report["compared_events"] == 10
    assert report["first_divergence"] is None


def test_first_domain_divergence_is_precise() -> None:
    candidate = trace("native")
    candidate["events"][6]["payload"]["position"][1] = 9.0
    report = compare_documents(trace("oracle"), candidate)
    assert report["equal"] is False
    assert report["first_divergence"] == {
        "path": "events[6].payload.position[1]",
        "reference": 1.0,
        "candidate": 9.0,
        "sequence": 6,
        "tick": 2,
        "domain": "simulation_snapshot",
    }


def test_selected_domains_can_isolate_simulation() -> None:
    candidate = trace("native")
    candidate["events"][3]["payload"]["draw_packets"] = 99
    report = compare_documents(
        trace("oracle"), candidate, ("simulation_snapshot", "output_hashes")
    )
    assert report["equal"] is True
    assert report["compared_events"] == 4


def test_wrong_domain_order_is_rejected() -> None:
    malformed = events(1)
    malformed[0]["domain"] = "simulation_snapshot"
    with pytest.raises(TraceV2Error, match="event 0 domain"):
        validate_events(malformed, 1, 1)


def test_capture_contract_mismatch_is_rejected() -> None:
    candidate = trace("native")
    candidate["header"]["replay"]["sha256"] = "8" * 64
    with pytest.raises(ComparisonError, match="capture contract mismatch: replay"):
        compare_documents(trace("oracle"), candidate)


def test_builder_seals_all_artifacts(tmp_path: Path) -> None:
    raw = tmp_path / "raw.jsonl"
    raw.write_text("".join(json.dumps(event) + "\n" for event in events(1)),
                   encoding="utf-8")
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)
    document = build_trace(
        raw, "oracle", ORACLE_COMMIT, NATIVE_COMMIT,
        artifacts[0], artifacts[1], artifacts[2], artifacts[3],
        "mission01-controlled-sortie", "controlled-sortie", 1, 1, 30, 60, 2,
    )
    assert document["event_count"] == len(DOMAINS)
    assert document["header"]["capture"]["sha256"] == hashlib.sha256(
        raw.read_bytes()).hexdigest()
    assert document["header"]["binary"]["sha256"] == hashlib.sha256(
        b"binary").hexdigest()
