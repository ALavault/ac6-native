from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from build_ac6_execution_trace_v2 import (  # noqa: E402
    CONTROLLER_MAPPING,
    DOMAINS,
    PROJECTION_RECEIPT_SCHEMA,
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
INPUT_A = {"pitch": 100, "roll": -200, "yaw": 300, "throttle": 40, "buttons": 5}
INPUT_B = {"pitch": -600, "roll": 700, "yaw": -800, "throttle": 90, "buttons": 10}


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


def events_with_inputs(inputs: list[dict], logical_ticks: list[int]) -> list[dict]:
    assert len(inputs) == len(logical_ticks)
    result = []
    for raw_tick, (controller, logical_tick) in enumerate(zip(inputs, logical_ticks), 1):
        for domain in DOMAINS:
            result.append(
                {
                    "sequence": len(result),
                    "tick": raw_tick,
                    "domain": domain,
                    "payload": dict(controller) if domain == "controller_input" else payload(domain, logical_tick),
                }
            )
    return result


def write_jsonl(path: Path, values: list[dict]) -> None:
    path.write_text("".join(json.dumps(value) + "\n" for value in values), encoding="utf-8")


def input_bytes(controller: dict) -> bytes:
    return struct.pack(
        "<hhhBH",
        controller["pitch"],
        controller["roll"],
        controller["yaw"],
        controller["throttle"],
        controller["buttons"],
    )


def projection_fixture(
    tmp_path: Path, source_inputs: list[dict], source_hz: int, native_hz: int
) -> tuple[Path, Path, list[Path]]:
    hold = native_hz // source_hz
    frames = b"".join(input_bytes(controller) * hold for controller in source_inputs)
    input_digest = hashlib.sha256(frames).digest()
    cache_digest = "c" * 64
    replay_data = bytearray(b"AC6RTPLY\0")
    replay_data.extend(struct.pack("<IIIIII", 3, 1, 1, 1, 1, 1))
    replay_data.extend(bytes.fromhex(cache_digest))
    replay_data.extend(struct.pack("<QI", 0xAC60000000000001, 0))
    replay_data.extend(struct.pack("<Q", len(source_inputs) * hold))
    replay_data.extend(input_digest)
    replay_data.extend(struct.pack("<I", len(source_inputs) * hold))
    replay_data.extend(frames)
    replay = tmp_path / f"projected-{source_hz}.ac6rpl"
    replay.write_bytes(replay_data)

    receipt = {
        "kind": "native_projection_receipt",
        "schema": PROJECTION_RECEIPT_SCHEMA,
        "source": {
            "raw_replay_sha256": "1" * 64,
            "raw_payload_sha256": "2" * 64,
            "parent_replay_sha256": "3" * 64,
            "parent_payload_sha256": "4" * 64,
            "parent_window": {"start_marker": 21, "marker_count": len(source_inputs)},
        },
        "target": {
            "title_id": "4E4D07D1",
            "media_id": "0379EFB3",
            "module": "default.xex",
            "xex_sha256": XEX_SHA256,
            "xex_version": "v0.0.0.11",
            "base_version": "v0.0.0.11",
            "module_xxh3": "abcdef0123456789",
            "marker_address": "8226D1C8",
            "marker_code_sha256": "5" * 64,
        },
        "cadence": {
            "source_hz": source_hz,
            "native_hz": native_hz,
            "resampling": "identity" if hold == 1 else "zero_order_hold",
            "hold": hold,
        },
        "mapping": CONTROLLER_MAPPING,
        "cache_index_sha256": cache_digest,
        "output": {
            "format": "AC6RTPLY",
            "version": 3,
            "mission_id": 1,
            "difficulty": 1,
            "difficulty_name": "Normal",
            "aircraft_id": 1,
            "weapon_id": 1,
            "capability_data_valid": True,
            "random_seed": 0xAC60000000000001,
            "checkpoint_count": 0,
            "source_marker_count": len(source_inputs),
            "frame_count": len(source_inputs) * hold,
            "final_tick": len(source_inputs) * hold,
            "input_digest_sha256": input_digest.hex(),
            "final_digest_sha256": input_digest.hex(),
            "output_sha256": hashlib.sha256(replay_data).hexdigest(),
        },
    }
    receipt_path = tmp_path / f"receipt-{source_hz}.json"
    receipt_path.write_text(
        json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8"
    )
    artifacts = []
    for name in ("stack", "oracle-binary", "native-binary", "probe"):
        path = tmp_path / f"{name}-{source_hz}"
        path.write_bytes(name.encode())
        artifacts.append(path)
    return replay, receipt_path, artifacts


def build_receipted_pair(
    tmp_path: Path, source_hz: int
) -> tuple[dict, dict, Path, Path]:
    replay, receipt, artifacts = projection_fixture(tmp_path, [INPUT_A, INPUT_B], source_hz, 60)
    hold = 60 // source_hz
    oracle_raw = tmp_path / f"oracle-{source_hz}.jsonl"
    native_raw = tmp_path / f"native-{source_hz}.jsonl"
    write_jsonl(oracle_raw, events_with_inputs([INPUT_A, INPUT_B], [1, 2]))
    native_inputs = [controller for controller in (INPUT_A, INPUT_B) for _ in range(hold)]
    native_ticks = [tick for tick in (1, 2) for _ in range(hold)]
    write_jsonl(native_raw, events_with_inputs(native_inputs, native_ticks))
    common = (
        ORACLE_COMMIT,
        NATIVE_COMMIT,
        artifacts[0],
        replay,
        artifacts[3],
        "mission01-controlled-sortie",
        "controlled-sortie",
        1,
        2,
        source_hz,
        60,
        hold,
        receipt,
    )
    oracle = build_trace(oracle_raw, "oracle", common[0], common[1], common[2], artifacts[1], *common[3:])
    native = build_trace(native_raw, "native", common[0], common[1], common[2], artifacts[2], *common[3:])
    return oracle, native, receipt, native_raw


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


def test_receipted_30_to_60_selects_ticks_two_and_four(tmp_path: Path) -> None:
    oracle, native, _, _ = build_receipted_pair(tmp_path, 30)
    assert native["header"]["window"]["observation"]["native"] == {
        "raw_sample_hz": 60,
        "raw_start_tick": 1,
        "raw_tick_count": 4,
        "selection": "last_native_tick_in_sample",
        "selected_tick_stride": 2,
        "selected_tick_phase": 2,
    }
    assert [
        event["payload"]["tick"]
        for event in native["events"]
        if event["domain"] == "simulation_snapshot"
    ] == [1, 2]
    assert compare_documents(oracle, native)["equal"] is True


def test_receipted_60_to_60_is_identity_and_needs_no_equal_pairs(tmp_path: Path) -> None:
    oracle, native, _, _ = build_receipted_pair(tmp_path, 60)
    assert native["events"][0]["payload"] == INPUT_A
    assert native["events"][len(DOMAINS)]["payload"] == INPUT_B
    assert native["header"]["window"]["observation"]["native"]["selection"] == "identity"
    assert native["header"]["window"]["observation"]["native"]["selected_tick_stride"] == 1
    assert compare_documents(oracle, native)["equal"] is True


def test_projection_receipt_cadence_mutation_fails_closed(tmp_path: Path) -> None:
    _, _, receipt_path, native_raw = build_receipted_pair(tmp_path, 30)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    receipt["cadence"]["hold"] = 1
    mutated = tmp_path / "mutated-receipt.json"
    mutated.write_text(
        json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8"
    )
    replay = tmp_path / "projected-30.ac6rpl"
    with pytest.raises(TraceV2Error, match="projection cadence unsupported"):
        build_trace(
            native_raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            tmp_path / "stack-30",
            tmp_path / "native-binary-30",
            replay,
            tmp_path / "probe-30",
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            2,
            30,
            60,
            2,
            mutated,
        )


def test_native_capture_must_match_every_receipted_held_frame(tmp_path: Path) -> None:
    _, _, receipt, native_raw = build_receipted_pair(tmp_path, 30)
    malformed = events_with_inputs([INPUT_A, INPUT_B, INPUT_B, INPUT_B], [1, 1, 2, 2])
    write_jsonl(native_raw, malformed)
    with pytest.raises(TraceV2Error, match="raw native frame 2 controller mismatch"):
        build_trace(
            native_raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            tmp_path / "stack-30",
            tmp_path / "native-binary-30",
            tmp_path / "projected-30.ac6rpl",
            tmp_path / "probe-30",
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            2,
            30,
            60,
            2,
            receipt,
        )


def test_comparison_rejects_receipt_and_window_mutations(tmp_path: Path) -> None:
    oracle, native, _, _ = build_receipted_pair(tmp_path, 30)
    native["header"]["window"]["controller_replay"]["receipt"]["sha256"] = "f" * 64
    with pytest.raises(ComparisonError, match="capture contract mismatch: window"):
        compare_documents(oracle, native)

    oracle, native, _, _ = build_receipted_pair(tmp_path, 60)
    native["header"]["window"]["start_tick"] = 2
    with pytest.raises(ComparisonError, match="native window bounds|capture contract mismatch: window"):
        compare_documents(oracle, native)


def test_receipted_window_bounds_fail_before_derivation(tmp_path: Path) -> None:
    _, _, receipt, native_raw = build_receipted_pair(tmp_path, 60)
    with pytest.raises(TraceV2Error, match="receipt window bounds"):
        build_trace(
            native_raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            tmp_path / "stack-60",
            tmp_path / "native-binary-60",
            tmp_path / "projected-60.ac6rpl",
            tmp_path / "probe-60",
            "mission01-controlled-sortie",
            "controlled-sortie",
            2,
            2,
            60,
            60,
            1,
            receipt,
        )
