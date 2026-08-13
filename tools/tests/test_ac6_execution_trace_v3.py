from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import build_ac6_execution_trace_v3 as trace_v3  # noqa: E402


def write_json(path: Path, value: object) -> Path:
    path.write_text(json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n")
    return path


def raw_events(path: Path, domains: tuple[str, ...], ticks: int = 2) -> Path:
    events = []
    for tick in range(1, ticks + 1):
        for domain in domains:
            events.append({
                "sequence": len(events), "tick": tick, "domain": domain,
                "payload": {"value": tick},
            })
    path.write_text("".join(json.dumps(event) + "\n" for event in events))
    return path


def receipt(path: Path) -> Path:
    return write_json(path, {
        "kind": "native_projection_receipt",
        "schema": "ac6.native-controller-projection-receipt.v4",
        "source": {
            "raw_schema": "ac6.controller-input-replay.v4",
            "oracle": {
                "target": trace_v3.NTSC_UJ_ORACLE_TARGET_IDENTITY,
                "marker_contract": trace_v3.NTSC_UJ_MARKER_CONTRACT,
            },
        },
        "native_target": trace_v3.PAL_NATIVE_TARGET_IDENTITY,
        "cadence": {"source_hz": 30, "native_hz": 60, "hold": 2},
    })


def artifacts(tmp_path: Path) -> list[Path]:
    result = []
    for name in ("stack", "binary", "build", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        result.append(path)
    return result


def test_v3_separates_oracle_pal_and_six_domains(tmp_path: Path) -> None:
    stack, binary, build, replay, probe = artifacts(tmp_path)
    trace = trace_v3.build_trace(
        raw_events(tmp_path / "raw.jsonl", trace_v3.DOMAINS), "oracle",
        ROOT / "analysis/oracle/ac6-recomp-ab90b-us/identity.json",
        receipt(tmp_path / "receipt.json"), stack, binary, build, replay, probe,
        "mission01", "1" * 40, "boot", 1, 2,
    )
    assert trace["schema"] == "ac6.execution-trace.v3"
    assert trace["header"]["oracle"]["target"]["xex_sha256"] == (
        "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc"
    )
    assert trace["header"]["native_target"]["xex_sha256"] == (
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
    )
    assert trace["header"]["window"]["domains"] == list(trace_v3.DOMAINS)
    sealed = write_json(tmp_path / "trace.json", trace)
    assert trace_v3.load_trace(sealed)[0] == "current-v3"


def test_v3_refuses_five_domain_relabel(tmp_path: Path) -> None:
    stack, binary, build, replay, probe = artifacts(tmp_path)
    with pytest.raises(trace_v3.TraceV3Error, match="event .* contract|event count"):
        trace_v3.build_trace(
            raw_events(tmp_path / "raw.jsonl", (
                "input", "simulation", "objectives", "graphics", "hashes")),
            "native", ROOT / "analysis/oracle/ac6-recomp-ab90b-us/identity.json",
            receipt(tmp_path / "receipt.json"), stack, binary, build, replay, probe,
            "mission01", "1" * 40, "boot", 1, 2,
        )


def test_v3_refuses_receipt_without_inter_region_lineage(tmp_path: Path) -> None:
    invalid = json.loads(receipt(tmp_path / "receipt.json").read_text())
    invalid["source"]["oracle"]["target"] = trace_v3.PAL_NATIVE_TARGET_IDENTITY
    write_json(tmp_path / "receipt.json", invalid)
    with pytest.raises(trace_v3.TraceV3Error, match="receipt lineage"):
        trace_v3._receipt(tmp_path / "receipt.json")


def test_v3_reader_refuses_oracle_pal_relabel(tmp_path: Path) -> None:
    stack, binary, build, replay, probe = artifacts(tmp_path)
    trace = trace_v3.build_trace(
        raw_events(tmp_path / "raw.jsonl", trace_v3.DOMAINS), "oracle",
        ROOT / "analysis/oracle/ac6-recomp-ab90b-us/identity.json",
        receipt(tmp_path / "receipt.json"), stack, binary, build, replay, probe,
        "mission01", "1" * 40, "boot", 1, 2,
    )
    trace["header"]["oracle"]["target"] = trace_v3.PAL_NATIVE_TARGET_IDENTITY
    path = write_json(tmp_path / "trace.json", trace)
    with pytest.raises(trace_v3.TraceV3Error, match="trace lineage"):
        trace_v3.load_trace(path)


def test_v2_read_is_historical_only(tmp_path: Path) -> None:
    domains = (
        "controller_input", "simulation_snapshot", "mission_objectives",
        "graphics_submission", "output_hashes",
    )
    events_path = raw_events(tmp_path / "v2-events.jsonl", domains, 1)
    events = [json.loads(line) for line in events_path.read_text().splitlines()]
    events[-1]["payload"] = {"state": "0" * 64}
    trace = {
        "schema": "ac6.execution-trace.v2",
        "header": {"window": {"start_tick": 1, "tick_count": 1}},
        "event_count": 5,
        "events": events,
    }
    path = write_json(tmp_path / "v2.json", trace)
    classification, loaded = trace_v3.load_trace(path)
    assert classification == "historical-v2"
    assert loaded["schema"] == "ac6.execution-trace.v2"
