from __future__ import annotations

import copy
import sys
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from compare_ac6_execution_traces import (  # noqa: E402
    ComparisonError,
    NATIVE_SCHEMA,
    ORACLE_COMMIT,
    ORACLE_SCHEMA,
    XEX_SHA256,
    compare_documents,
)


def event(sequence: int, digest: str) -> dict:
    return {
        "sequence": sequence,
        "tick": sequence,
        "guest_address": "0x82267370",
        "input": {
            "axes": {"lx": 0, "ly": 0, "rx": 0, "ry": 0},
            "triggers": {"left": 0, "right": 0},
            "buttons": [],
        },
        "graphics": {"draw_calls": sequence, "state_digest": f"state-{sequence}"},
        "output_hashes": {"simulation": digest * 64},
    }


def oracle_trace() -> dict:
    return {
        "schema": ORACLE_SCHEMA,
        "qualification": {
            "manifest": "analysis/oracle/ac6-recomp-dcd41b/manifest.json",
            "manifest_sha256": "1" * 64,
            "oracle_commit": ORACLE_COMMIT,
            "xex_sha256": XEX_SHA256,
            "probe": "mission01-frame",
            "probe_contract_sha256": "2" * 64,
            "raw_sha256": "3" * 64,
        },
        "event_count": 2,
        "events": [event(0, "a"), event(1, "b")],
    }


def native_trace() -> dict:
    trace = oracle_trace()
    trace["schema"] = NATIVE_SCHEMA
    trace["qualification"] = {
        "producer": "ac6-native",
        "content_identity": "4" * 64,
        "replay_sha256": "5" * 64,
    }
    return trace


def test_equal_cross_implementation_trace() -> None:
    report = compare_documents(oracle_trace(), native_trace())
    assert report["equal"] is True
    assert report["compared_events"] == 2
    assert report["first_divergence"] is None


def test_first_hash_divergence_is_precise() -> None:
    candidate = native_trace()
    candidate["events"][1]["output_hashes"]["simulation"] = "c" * 64
    report = compare_documents(oracle_trace(), candidate)
    assert report["equal"] is False
    assert report["first_divergence"] == {
        "path": "events[1].output_hashes.simulation",
        "reference": "b" * 64,
        "candidate": "c" * 64,
        "sequence": 1,
        "tick": {"reference": 1, "candidate": 1},
    }


def test_selected_domains_can_isolate_simulation() -> None:
    candidate = native_trace()
    candidate["events"][0]["graphics"]["draw_calls"] = 99
    report = compare_documents(oracle_trace(), candidate, ("output_hashes",))
    assert report["equal"] is True


def test_event_count_divergence_is_reported_after_common_prefix() -> None:
    candidate = native_trace()
    candidate["events"].pop()
    candidate["event_count"] = 1
    report = compare_documents(oracle_trace(), candidate)
    assert report["first_divergence"]["path"] == "event_count"
    assert report["first_divergence"]["sequence"] == 1


def test_unqualified_reference_is_rejected() -> None:
    reference = copy.deepcopy(oracle_trace())
    del reference["qualification"]
    with pytest.raises(ComparisonError, match="reference qualification"):
        compare_documents(reference, native_trace())
