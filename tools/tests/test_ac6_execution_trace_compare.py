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
    CADENCE_CENSUS_SCHEMA,
    CADENCE_METHOD,
    CONTROLLER_MAPPING,
    DOMAINS,
    INTEGRITY_ONLY_CENSUS,
    MAX_TRACE_TICKS,
    NATIVE_CLOCK_CONTRACT,
    PROJECTION_RECEIPT_SCHEMA,
    RUNNER_ATTESTED,
    TRACE_SCHEMA,
    TraceV2Error,
    _derive_receipt_events,
    _projected_replay_frames,
    build_trace,
    load_jsonl_bytes,
    load_projection_receipt,
    main as build_trace_main,
    validate_events,
)
import build_ac6_execution_trace_v2 as trace_builder  # noqa: E402
import compare_ac6_execution_traces as compare_module  # noqa: E402
from compare_ac6_execution_traces import (  # noqa: E402
    ComparisonError,
    ORACLE_COMMIT,
    XEX_SHA256,
    _read_trace_snapshot,
    compare_documents,
    main as compare_trace_main,
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
            result.append(
                {
                    "sequence": len(result),
                    "tick": tick,
                    "domain": domain,
                    "payload": payload(domain, tick),
                }
            )
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
                    "payload": (
                        dict(controller)
                        if domain == "controller_input"
                        else payload(domain, logical_tick, f"{logical_tick:x}")
                    ),
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
    assert native_hz == NATIVE_CLOCK_CONTRACT["frequency"]["numerator"]
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
            "marker_code_offset": 0x26D1C8,
            "marker_code_length": 16,
            "marker_code_sha256": "5" * 64,
        },
        "cadence": {
            "integrity_level": INTEGRITY_ONLY_CENSUS,
            "source_hz": source_hz,
            "native_hz": native_hz,
            "resampling": "identity" if hold == 1 else "zero_order_hold",
            "hold": hold,
            "census": {
                "schema": CADENCE_CENSUS_SCHEMA,
                "file_sha256": "6" * 64,
                "payload_sha256": "7" * 64,
                "integrity_level": INTEGRITY_ONLY_CENSUS,
                "method": CADENCE_METHOD,
                "record_count": len(source_inputs),
                "interval_count": len(source_inputs) - 1,
            },
            "marker_contract": {
                "role": "mission_manager_tick",
                "address": "8226D1C8",
                "phase": "after_input",
                "code": {
                    "offset": 0x26D1C8,
                    "length": 16,
                    "sha256": "5" * 64,
                },
            },
            "native_clock": NATIVE_CLOCK_CONTRACT,
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
    receipt_path.write_text(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    artifacts = []
    for name in ("stack", "oracle-binary", "native-binary", "probe"):
        path = tmp_path / f"{name}-{source_hz}"
        path.write_bytes(name.encode())
        artifacts.append(path)
    return replay, receipt_path, artifacts


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


def diagnostic_compare(reference: dict, candidate: dict, domains: tuple[str, ...] = DOMAINS) -> dict:
    return compare_documents(reference, candidate, domains, allow_legacy_diagnostic=True)


def test_comparison_is_disabled_as_a_gate_without_runner_attestation() -> None:
    with pytest.raises(ComparisonError, match="gate is disabled"):
        compare_documents(trace("oracle"), trace("native"))


def test_equal_cross_implementation_trace() -> None:
    report = diagnostic_compare(trace("oracle"), trace("native"))
    assert report["equal"] is True
    assert report["proof_level"] == "structural_diagnostic"
    assert report["compared_events"] == 10
    assert report["first_divergence"] is None


def test_first_domain_divergence_is_precise() -> None:
    candidate = trace("native")
    candidate["events"][6]["payload"]["position"][1] = 9.0
    report = diagnostic_compare(trace("oracle"), candidate)
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
    report = diagnostic_compare(trace("oracle"), candidate, ("simulation_snapshot", "output_hashes"))
    assert report["equal"] is True
    assert report["compared_events"] == 4


def test_wrong_domain_order_is_rejected() -> None:
    malformed = events(1)
    malformed[0]["domain"] = "simulation_snapshot"
    with pytest.raises(TraceV2Error, match="event 0 domain"):
        validate_events(malformed, 1, 1)


@pytest.mark.parametrize("invalid_events", (None, 1, {}))
def test_comparator_rejects_non_list_events(invalid_events: object) -> None:
    malformed = trace("native")
    malformed["events"] = invalid_events
    with pytest.raises(ComparisonError, match="native event count"):
        diagnostic_compare(trace("oracle"), malformed)


def test_capture_contract_mismatch_is_rejected() -> None:
    candidate = trace("native")
    candidate["header"]["replay"]["sha256"] = "8" * 64
    with pytest.raises(ComparisonError, match="capture contract mismatch: replay"):
        diagnostic_compare(trace("oracle"), candidate)


def test_builder_seals_all_artifacts(tmp_path: Path) -> None:
    raw = tmp_path / "raw.jsonl"
    raw.write_text("".join(json.dumps(event) + "\n" for event in events(1)), encoding="utf-8")
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)
    document = build_trace(
        raw,
        "oracle",
        ORACLE_COMMIT,
        NATIVE_COMMIT,
        artifacts[0],
        artifacts[1],
        artifacts[2],
        artifacts[3],
        "mission01-controlled-sortie",
        "controlled-sortie",
        1,
        1,
        30,
    )
    assert document["event_count"] == len(DOMAINS)
    assert document["header"]["capture"]["sha256"] == hashlib.sha256(raw.read_bytes()).hexdigest()
    assert document["header"]["binary"]["sha256"] == hashlib.sha256(b"binary").hexdigest()


def test_builder_refuses_unreceipted_native_capture(tmp_path: Path) -> None:
    raw = tmp_path / "native-raw.jsonl"
    raw.write_text("".join(json.dumps(event) + "\n" for event in events(1)), encoding="utf-8")
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)
    with pytest.raises(TraceV2Error, match="native trace requires a projection receipt"):
        build_trace(
            raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            artifacts[0],
            artifacts[1],
            artifacts[2],
            artifacts[3],
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            1,
            30,
        )


def test_trace_windows_and_payloads_are_bounded_before_comparison(tmp_path: Path) -> None:
    raw = tmp_path / "raw.jsonl"
    write_jsonl(raw, events(2))
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)
    with pytest.raises(TraceV2Error, match="window bounds"):
        build_trace(
            raw,
            "oracle",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            *artifacts,
            "mission01-controlled-sortie",
            "controlled-sortie",
            MAX_TRACE_TICKS,
            2,
            30,
        )

    out_of_range = trace("oracle")
    out_of_range["header"]["window"]["start_tick"] = MAX_TRACE_TICKS
    with pytest.raises(ComparisonError, match="oracle window"):
        diagnostic_compare(out_of_range, trace("native"))

    non_finite = events(1)
    non_finite[0]["payload"]["value"] = float("nan")
    with pytest.raises(TraceV2Error, match="non-finite"):
        validate_events(non_finite, 1, 1)
    raw_nan = "".join(json.dumps(event) + "\n" for event in non_finite).encode()
    with pytest.raises(TraceV2Error, match="non-finite"):
        load_jsonl_bytes(raw_nan, 1, 1)

    huge_integer = (
        json.dumps(events(1)[0]).replace('"tick": 1', '"tick": ' + "9" * 5000)
        + "\n"
        + "".join(json.dumps(event) + "\n" for event in events(1)[1:])
    ).encode()
    with pytest.raises(TraceV2Error, match="JSON integer bound"):
        load_jsonl_bytes(huge_integer, 1, 1)

    nested: object = 0
    for _ in range(65):
        nested = [nested]
    deep = trace("native", 1)
    deep["events"][0]["payload"] = {"nested": nested}
    with pytest.raises(ComparisonError, match="depth bound"):
        diagnostic_compare(trace("oracle", 1), deep)

    encoded = b"[" * 65 + b"0" + b"]" * 65
    deep_path = tmp_path / "deep.json"
    deep_path.write_bytes(encoded)
    with pytest.raises(ComparisonError, match="depth bound"):
        _read_trace_snapshot(deep_path, "deep")

    integer_path = tmp_path / "huge-integer.json"
    integer_path.write_bytes(b'{"x":' + b"9" * 5000 + b"}")
    with pytest.raises(ComparisonError, match="JSON integer bound"):
        _read_trace_snapshot(integer_path, "integer")


def test_receipted_30_to_60_selects_ticks_two_and_four(tmp_path: Path) -> None:
    replay_path, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    receipt = load_projection_receipt(receipt_path)
    projected_frames = _projected_replay_frames(replay_path, receipt)
    native_raw = tmp_path / "native-30.jsonl"
    write_jsonl(
        native_raw,
        events_with_inputs([INPUT_A, INPUT_A, INPUT_B, INPUT_B], [1, 2, 3, 4]),
    )

    selected, observation = _derive_receipt_events(
        native_raw,
        "native",
        receipt,
        1,
        2,
        60,
        projected_frames,
    )
    assert observation["native"] == {
        "raw_sample_hz": 60,
        "raw_start_tick": 1,
        "raw_tick_count": 4,
        "selection": "last_native_tick_in_sample",
        "selected_tick_stride": 2,
        "selected_tick_phase": 2,
    }
    assert [event["payload"]["tick"] for event in selected if event["domain"] == "simulation_snapshot"] == [
        2,
        4,
    ]
    selected_hashes = [event["payload"]["simulation"] for event in selected if event["domain"] == "output_hashes"]
    assert selected_hashes == ["2" * 64, "4" * 64]
    assert not {"1" * 64, "3" * 64}.intersection(selected_hashes)


def test_receipted_60_to_60_is_identity_and_needs_no_equal_pairs(tmp_path: Path) -> None:
    replay_path, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 60, 60)
    receipt = load_projection_receipt(receipt_path)
    raw = tmp_path / "native-60.jsonl"
    write_jsonl(raw, events_with_inputs([INPUT_A, INPUT_B], [1, 2]))
    selected, observation = _derive_receipt_events(
        raw,
        "native",
        receipt,
        1,
        2,
        60,
        _projected_replay_frames(replay_path, receipt),
    )
    assert selected[0]["payload"] == INPUT_A
    assert selected[len(DOMAINS)]["payload"] == INPUT_B
    assert observation["native"]["selection"] == "identity"
    assert observation["native"]["selected_tick_stride"] == 1


def test_projection_receipt_cadence_mutation_fails_closed(tmp_path: Path) -> None:
    _, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    receipt["cadence"]["hold"] = 1
    mutated = tmp_path / "mutated-receipt.json"
    mutated.write_text(json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    with pytest.raises(TraceV2Error, match="projection cadence unsupported"):
        load_projection_receipt(mutated)


@pytest.mark.parametrize(
    ("mutation", "message"),
    (
        (
            lambda receipt: receipt.update(schema="ac6.native-controller-projection-receipt.v1"),
            "projection receipt identity",
        ),
        (
            lambda receipt: receipt.update(schema="ac6.native-controller-projection-receipt.v2"),
            "projection receipt identity",
        ),
        (
            lambda receipt: receipt["cadence"]["census"].update(method="asserted_hz"),
            "projection cadence census identity",
        ),
        (
            lambda receipt: receipt["cadence"]["census"].update(interval_count=2),
            "projection cadence census identity",
        ),
        (
            lambda receipt: receipt["cadence"]["marker_contract"].update(address="821CA908"),
            "projection marker contract identity",
        ),
        (
            lambda receipt: receipt["cadence"]["marker_contract"]["code"].update(sha256="8" * 64),
            "projection marker contract code mismatch",
        ),
        (
            lambda receipt: receipt["cadence"]["native_clock"]["frequency"].update(numerator=30),
            "projection native clock contract",
        ),
        (
            lambda receipt: receipt["cadence"]["native_clock"]["frequency"].update(denominator=True),
            "projection native clock denominator range",
        ),
    ),
)
def test_projection_receipt_v3_contract_mutations_fail_closed(
    tmp_path: Path,
    mutation: object,
    message: str,
) -> None:
    _, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    mutation(receipt)
    receipt_path.write_text(
        json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    with pytest.raises(TraceV2Error, match=message):
        load_projection_receipt(receipt_path)


def test_integrity_only_receipt_is_not_trace_grade(tmp_path: Path) -> None:
    replay, receipt_path, artifacts = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    raw = tmp_path / "native-integrity-only.jsonl"
    write_jsonl(raw, events_with_inputs([INPUT_A, INPUT_A, INPUT_B, INPUT_B], [1, 2, 3, 4]))
    with pytest.raises(TraceV2Error, match="integrity-only runtime census is not trace-grade evidence"):
        build_trace(
            raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            artifacts[0],
            artifacts[2],
            replay,
            artifacts[3],
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            2,
            30,
            receipt_path,
        )


def test_relabel_reseal_cannot_forge_trace_grade_receipt(tmp_path: Path) -> None:
    replay, receipt_path, artifacts = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    receipt = json.loads(receipt_path.read_bytes())
    receipt["cadence"]["integrity_level"] = RUNNER_ATTESTED
    receipt_path.write_bytes((json.dumps(receipt, sort_keys=True, separators=(",", ":")) + "\n").encode())
    with pytest.raises(TraceV2Error, match="runner attestation verification is unavailable"):
        load_projection_receipt(receipt_path)

    raw = tmp_path / "native-relabelled.jsonl"
    write_jsonl(raw, events_with_inputs([INPUT_A, INPUT_A, INPUT_B, INPUT_B], [1, 2, 3, 4]))
    with pytest.raises(TraceV2Error, match="runner attestation verification is unavailable"):
        build_trace(
            raw,
            "native",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            artifacts[0],
            artifacts[2],
            replay,
            artifacts[3],
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            2,
            30,
            receipt_path,
        )


def test_native_capture_must_match_every_receipted_held_frame(tmp_path: Path) -> None:
    replay_path, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 30, 60)
    receipt = load_projection_receipt(receipt_path)
    native_raw = tmp_path / "native-malformed.jsonl"
    malformed = events_with_inputs([INPUT_A, INPUT_B, INPUT_B, INPUT_B], [1, 1, 2, 2])
    write_jsonl(native_raw, malformed)
    with pytest.raises(TraceV2Error, match="raw native frame 2 controller mismatch"):
        _derive_receipt_events(
            native_raw,
            "native",
            receipt,
            1,
            2,
            60,
            _projected_replay_frames(replay_path, receipt),
        )


def test_receipted_window_bounds_fail_before_derivation(tmp_path: Path) -> None:
    replay_path, receipt_path, _ = projection_fixture(tmp_path, [INPUT_A, INPUT_B], 60, 60)
    receipt = load_projection_receipt(receipt_path)
    native_raw = tmp_path / "native-window.jsonl"
    write_jsonl(native_raw, events_with_inputs([INPUT_A, INPUT_B], [1, 2]))
    with pytest.raises(TraceV2Error, match="receipt window bounds"):
        _derive_receipt_events(
            native_raw,
            "native",
            receipt,
            2,
            2,
            60,
            _projected_replay_frames(replay_path, receipt),
        )


def test_builder_bounds_each_artifact_and_total_input(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    raw = tmp_path / "bounded.jsonl"
    write_jsonl(raw, events(1))
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)

    monkeypatch.setattr(trace_builder, "MAX_ARTIFACT_BYTES", 4)
    with pytest.raises(TraceV2Error, match="patch stack byte bound"):
        build_trace(
            raw,
            "oracle",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            *artifacts,
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            1,
            30,
        )

    monkeypatch.setattr(trace_builder, "MAX_ARTIFACT_BYTES", 1024 * 1024)
    total = len(raw.read_bytes()) + sum(len(path.read_bytes()) for path in artifacts)
    monkeypatch.setattr(trace_builder, "MAX_TOTAL_INPUT_BYTES", total - 1)
    with pytest.raises(TraceV2Error, match="total input byte bound"):
        build_trace(
            raw,
            "oracle",
            ORACLE_COMMIT,
            NATIVE_COMMIT,
            *artifacts,
            "mission01-controlled-sortie",
            "controlled-sortie",
            1,
            1,
            30,
        )


def test_comparator_bounds_input_and_refuses_report_alias(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    reference_path = tmp_path / "oracle.json"
    candidate_path = tmp_path / "native.json"
    reference_data = (json.dumps(trace("oracle"), sort_keys=True) + "\n").encode()
    candidate_data = (json.dumps(trace("native"), sort_keys=True) + "\n").encode()
    reference_path.write_bytes(reference_data)
    candidate_path.write_bytes(candidate_data)

    monkeypatch.setattr(compare_module, "MAX_TRACE_BYTES", len(reference_data) - 1)
    monkeypatch.setattr(
        sys,
        "argv",
        ["compare_ac6_execution_traces.py", str(reference_path), str(candidate_path)],
    )
    assert compare_trace_main() == 2
    assert "reference byte bound" in capsys.readouterr().out

    monkeypatch.setattr(compare_module, "MAX_TRACE_BYTES", 1024 * 1024)
    monkeypatch.setattr(
        compare_module,
        "MAX_TOTAL_TRACE_BYTES",
        len(reference_data) + len(candidate_data) - 1,
    )
    monkeypatch.setattr(
        sys,
        "argv",
        ["compare_ac6_execution_traces.py", str(reference_path), str(candidate_path)],
    )
    assert compare_trace_main() == 2
    assert "comparison total byte bound" in capsys.readouterr().out

    monkeypatch.setattr(compare_module, "MAX_TOTAL_TRACE_BYTES", 1024 * 1024)
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "compare_ac6_execution_traces.py",
            str(reference_path),
            str(candidate_path),
            "--report",
            str(reference_path),
        ],
    )
    assert compare_trace_main() == 2
    assert "report aliases an input" in capsys.readouterr().out
    assert reference_path.read_bytes() == reference_data

    report_alias = tmp_path / "report-hardlink.json"
    report_alias.hardlink_to(reference_path)
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "compare_ac6_execution_traces.py",
            str(reference_path),
            str(candidate_path),
            "--report",
            str(report_alias),
        ],
    )
    assert compare_trace_main() == 2
    assert "report aliases an input" in capsys.readouterr().out
    assert reference_path.read_bytes() == reference_data


def test_cli_outputs_are_atomic_and_builder_refuses_input_alias(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    reference_path = tmp_path / "oracle.json"
    candidate_path = tmp_path / "native.json"
    report_path = tmp_path / "report.json"
    reference_path.write_text(json.dumps(trace("oracle")), encoding="utf-8")
    candidate_path.write_text(json.dumps(trace("native")), encoding="utf-8")
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "compare_ac6_execution_traces.py",
            str(reference_path),
            str(candidate_path),
            "--allow-legacy-diagnostic",
            "--report",
            str(report_path),
        ],
    )
    assert compare_trace_main() == 0
    assert json.loads(report_path.read_bytes())["equal"] is True
    assert not list(tmp_path.glob(".*.tmp"))
    capsys.readouterr()

    raw = tmp_path / "raw.jsonl"
    raw_data = "".join(json.dumps(event) + "\n" for event in events(1)).encode()
    raw.write_bytes(raw_data)
    artifacts = []
    for name in ("stack", "binary", "replay", "probe"):
        path = tmp_path / name
        path.write_bytes(name.encode())
        artifacts.append(path)
    build_arguments = [
        "build_ac6_execution_trace_v2.py",
        str(raw),
        str(raw),
        "--role",
        "oracle",
        "--oracle-commit",
        ORACLE_COMMIT,
        "--native-commit",
        NATIVE_COMMIT,
        "--patch-stack",
        str(artifacts[0]),
        "--binary",
        str(artifacts[1]),
        "--replay",
        str(artifacts[2]),
        "--probe",
        str(artifacts[3]),
        "--probe-id",
        "mission01-controlled-sortie",
        "--window",
        "controlled-sortie",
        "--start-tick",
        "1",
        "--tick-count",
        "1",
    ]
    monkeypatch.setattr(sys, "argv", build_arguments)
    assert build_trace_main() == 1
    assert "output aliases an input" in capsys.readouterr().out
    assert raw.read_bytes() == raw_data

    raw_alias = tmp_path / "raw-hardlink.jsonl"
    raw_alias.hardlink_to(raw)
    build_arguments[2] = str(raw_alias)
    monkeypatch.setattr(sys, "argv", build_arguments)
    assert build_trace_main() == 1
    assert "output aliases an input" in capsys.readouterr().out
    assert raw.read_bytes() == raw_data

    output_path = tmp_path / "trace.json"
    build_arguments[2] = str(output_path)
    monkeypatch.setattr(sys, "argv", build_arguments)
    assert build_trace_main() == 0
    assert json.loads(output_path.read_bytes())["schema"] == TRACE_SCHEMA
    assert not list(tmp_path.glob(".*.tmp"))
