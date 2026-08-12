from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from ac6_controller_input_replay import (  # noqa: E402
    ReplayError,
    _atomic_write_new,
    _canonical_controller,
    _publish_atomic_files,
    build_ac6rtply_v3,
    canonical_line,
    compare_runs,
    export_controller_tsv,
    load_replay_bytes,
    main,
    read_cache_identity,
    seal_replay,
    slice_replay,
)
import ac6_controller_input_replay as replay_module  # noqa: E402


def header(source_hz: int | None = None) -> dict:
    measured = source_hz is not None
    return {
        "kind": "header",
        "schema": "ac6.controller-input-replay.v1",
        "producer": {
            "lane": "xenia-canary",
            "implementation_commit": "16e1eb8e28a2935b75c36707b585a4f5e174ad43",
            "binary_sha256": "1" * 64,
            "platform": "wine10-windows-x64-vulkan",
        },
        "target": {
            "title_id": "4E4D07D1",
            "media_id": "0379EFB3",
            "module": "default.xex",
            "xex_sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "xex_version": "v0.0.0.11",
            "base_version": "v0.0.0.11",
            "module_xxh3": "0123456789abcdef",
            "marker_address": "821CA908",
            "marker_code_sha256": "2" * 64,
        },
        "session": {
            "content_manifest_sha256": "3" * 64,
            "runtime_config_sha256": "4" * 64,
            "behavior_config_sha256": "7" * 64,
            "profile_save_manifest_sha256": "5" * 64,
            "route_sha256": "6" * 64,
            "segment_origin": "sealed_retail_save",
        },
        "segment": (
            {
                "kind": "marker_window",
                "parent_replay_sha256": "8" * 64,
                "parent_payload_sha256": "9" * 64,
                "parent_start_marker": 1,
                "parent_marker_count": 2,
            }
            if measured
            else {
                "kind": "full_recording",
                "parent_replay_sha256": None,
                "parent_payload_sha256": None,
                "parent_start_marker": None,
                "parent_marker_count": None,
            }
        ),
        "sync": {
            "primary": "poll_index",
            "portable_guards": [
                "marker_index",
                "poll_in_marker",
                "caller_lr",
                "user_index",
                "flags",
                "state_ptr_null",
            ],
            "lane_local_diagnostics": ["thread_id", "state_ptr"],
            "telemetry": ["guest_tick", "present_index"],
            "marker_role": "ac6_frame_input_stage",
            "marker_phase": "before_input",
            "cadence": {
                "status": "measured" if measured else "unqualified",
                "source_hz": source_hz,
                "native_hz": 60,
                "resampling": ("identity" if source_hz == 60 else "zero_order_hold" if measured else "refuse"),
                "projection": "unique_successful_user0_poll",
            },
        },
    }


def state(buttons: int = 0, lx: int = 0, ly: int = 0) -> dict:
    return {
        "packet_number": 10,
        "buttons": buttons,
        "left_trigger": 0,
        "right_trigger": 127,
        "thumb_lx": lx,
        "thumb_ly": ly,
        "thumb_rx": -20,
        "thumb_ry": 20,
    }


def events() -> list[dict]:
    return [
        {
            "kind": "poll",
            "sequence": 0,
            "poll_index": 0,
            "marker_index": 0,
            "poll_in_marker": 0,
            "guest_tick": 100,
            "present_index": 0,
            "thread_id": 1,
            "caller_lr": 0x8234D418,
            "user_index": 1,
            "flags": 1,
            "state_ptr": 0xA1001000,
            "result": 0,
            "state": state(),
        },
        {
            "kind": "marker",
            "sequence": 1,
            "marker_index": 1,
            "poll_index": 1,
            "guest_tick": 200,
            "present_index": 1,
        },
        {
            "kind": "poll",
            "sequence": 2,
            "poll_index": 1,
            "marker_index": 1,
            "poll_in_marker": 0,
            "guest_tick": 201,
            "present_index": 1,
            "thread_id": 1,
            "caller_lr": 0x8234D418,
            "user_index": 1,
            "flags": 1,
            "state_ptr": 0xA1001000,
            "result": 0,
            "state": state(0x0100, 123, -321),
        },
        {
            "kind": "poll",
            "sequence": 3,
            "poll_index": 2,
            "marker_index": 1,
            "poll_in_marker": 1,
            "guest_tick": 202,
            "present_index": 1,
            "thread_id": 1,
            "caller_lr": 0x8234D418,
            "user_index": 0,
            "flags": 1,
            "state_ptr": 0xA1001000,
            "result": 0,
            "state": state(0x0200, 456, -654),
        },
        {
            "kind": "marker",
            "sequence": 4,
            "marker_index": 2,
            "poll_index": 3,
            "guest_tick": 300,
            "present_index": 2,
        },
        {
            "kind": "poll",
            "sequence": 5,
            "poll_index": 3,
            "marker_index": 2,
            "poll_in_marker": 0,
            "guest_tick": 301,
            "present_index": 2,
            "thread_id": 1,
            "caller_lr": 0x8234D418,
            "user_index": 0,
            "flags": 1,
            "state_ptr": 0xA1001000,
            "result": 0,
            "state": state(0, -111, 222),
        },
    ]


def document(data: bytes | None = None, replay_header: dict | None = None):
    replay_header = replay_header or header()
    return load_replay_bytes(data if data is not None else seal_replay(replay_header, events(), 3))


def make_cache(root: Path) -> tuple[Path, str]:
    cache = root / "cache"
    indices = cache / "indices"
    indices.mkdir(parents=True)
    index = b"sealed metadata-only cache index fixture"
    digest = hashlib.sha256(index).hexdigest()
    (indices / f"{digest}.ac6idx").write_bytes(index)
    (cache / "current").write_bytes(b"AC6RCUR\0" + struct.pack(">II", 2, 48) + bytes.fromhex(digest))
    return cache, digest


def measured_window(source_hz: int = 30) -> tuple[bytes, dict]:
    parent_header = header()
    parent = seal_replay(parent_header, events(), 3)
    window = slice_replay(parent, 1, 2, source_hz, 60, parent_header)
    return window, load_replay_bytes(window).header


def test_round_trip_and_controller_export_requires_qualified_cadence() -> None:
    replay = document(replay_header=header(60))
    assert replay.footer["poll_count"] == 4
    assert replay.footer["marker_count"] == 2
    assert export_controller_tsv(replay, 1, 2) == ("1 -654 456 32767 127 512\n2 222 -111 -20 127 0\n")
    with pytest.raises(ReplayError, match="cadence is unqualified"):
        export_controller_tsv(document(), 1, 2)


def test_30_to_60_export_requires_and_applies_explicit_zero_order_hold() -> None:
    output = export_controller_tsv(document(replay_header=header(30)), 1, 2)
    assert output == (
        "1 -654 456 32767 127 512\n2 -654 456 32767 127 512\n3 222 -111 -20 127 0\n4 222 -111 -20 127 0\n"
    )


def test_corruption_is_rejected_by_payload_sha256() -> None:
    data = seal_replay(header(), events(), 3)
    corrupted = data.replace(b'"guest_tick":201', b'"guest_tick":200', 1)
    with pytest.raises(ReplayError, match="payload_sha256"):
        load_replay_bytes(corrupted)


def test_truncation_without_footer_is_rejected() -> None:
    data = seal_replay(header(), events(), 3)
    truncated = b"".join(data.splitlines(keepends=True)[:-1])
    with pytest.raises(ReplayError, match="footer"):
        load_replay_bytes(truncated)


def test_noncanonical_encoding_is_rejected() -> None:
    data = seal_replay(header(), events(), 3)
    noncanonical = data.replace(b'"kind":"header"', b'"kind": "header"', 1)
    with pytest.raises(ReplayError, match="canonical encoding"):
        load_replay_bytes(noncanonical)


def test_poll_gap_and_marker_gap_are_rejected_before_sealing() -> None:
    bad_poll = events()
    bad_poll[3]["poll_index"] = 9
    with pytest.raises(ReplayError, match="poll_index"):
        seal_replay(header(), bad_poll)
    bad_marker = events()
    bad_marker[4]["marker_index"] = 4
    with pytest.raises(ReplayError, match="marker_index"):
        seal_replay(header(), bad_marker)


def test_runtime_identity_mismatch_is_rejected() -> None:
    expected = header()
    expected["session"]["runtime_config_sha256"] = "f" * 64
    with pytest.raises(ReplayError, match="runtime session mismatch"):
        load_replay_bytes(seal_replay(header(), events()), expected)


def test_state_pointer_and_ranges_fail_closed() -> None:
    bad_pointer = events()
    bad_pointer[0]["state_ptr"] = 0
    with pytest.raises(ReplayError, match="state/result/pointer"):
        seal_replay(header(), bad_pointer)
    bad_range = events()
    bad_range[2]["state"]["thumb_lx"] = 32768
    with pytest.raises(ReplayError, match="thumb_lx range"):
        seal_replay(header(), bad_range)


def test_compare_checks_sync_guards_but_reports_time_and_present_as_telemetry() -> None:
    reference = document()
    shifted = events()
    for event in shifted:
        event["guest_tick"] += 50
        event["present_index"] += 2
    candidate = document(seal_replay(header(), shifted, 5))
    report = compare_runs(reference, candidate)
    assert report == {
        "equal": True,
        "comparison_policy": "same_lane_strict",
        "polls_compared": 4,
        "first_divergence": None,
        "telemetry": {"max_guest_tick_delta": 50, "max_present_index_delta": 2},
    }

    divergent = events()
    divergent[3]["caller_lr"] ^= 4
    report = compare_runs(reference, document(seal_replay(header(), divergent, 3)))
    assert report["equal"] is False
    assert report["first_divergence"]["field"] == "caller_lr"


def test_footer_cannot_claim_fewer_presents_than_observed() -> None:
    with pytest.raises(ReplayError, match="present_count order"):
        seal_replay(header(), events(), 1)


def test_footer_present_count_corruption_is_rejected() -> None:
    data = seal_replay(header(), events(), 3)
    corrupted = data.replace(b'"present_count":3', b'"present_count":4', 1)
    with pytest.raises(ReplayError, match="payload_sha256"):
        load_replay_bytes(corrupted)


def test_compare_rejects_extra_marker_without_a_poll() -> None:
    candidate_events = events()
    extra_poll = {**candidate_events[-1], "sequence": 6, "poll_index": 4, "poll_in_marker": 1}
    candidate_events.append(extra_poll)
    report = compare_runs(document(), document(seal_replay(header(), candidate_events, 3)))
    assert report["equal"] is False
    assert report["first_divergence"]["field"] == "event_count"
    assert report["telemetry"] == {
        "max_guest_tick_delta": 0,
        "max_present_index_delta": 0,
    }


def test_empty_replay_and_marker_only_replay_are_rejected() -> None:
    with pytest.raises(ReplayError, match="empty replay"):
        seal_replay(header(), [])
    marker = {
        "kind": "marker",
        "sequence": 0,
        "marker_index": 1,
        "poll_index": 0,
        "guest_tick": 1,
        "present_index": 0,
    }
    with pytest.raises(ReplayError, match="no polls"):
        seal_replay(header(), [marker])
    with pytest.raises(ReplayError, match="no markers"):
        seal_replay(header(), [events()[0]])


def test_null_success_query_is_preserved_but_error_state_is_rejected() -> None:
    query = events()
    query[0]["state_ptr"] = 0
    query[0]["state"] = None
    assert load_replay_bytes(seal_replay(header(), query)).footer["poll_count"] == 4

    bad_error = events()
    bad_error[0]["result"] = 1167
    with pytest.raises(ReplayError, match="state/result/pointer"):
        seal_replay(header(), bad_error)


def test_export_rejects_ambiguous_or_absent_marker_projection() -> None:
    ambiguous = events()
    ambiguous[1 + 1]["user_index"] = 0
    replay = document(seal_replay(header(60), ambiguous, 3))
    with pytest.raises(ReplayError, match="has 2 successful"):
        export_controller_tsv(replay, 1, 1)
    with pytest.raises(ReplayError, match="marker 3 is absent"):
        export_controller_tsv(document(replay_header=header(60)), 3, 1)


def test_cross_lane_compare_uses_portable_guards_only() -> None:
    candidate_header = header()
    candidate_header["producer"] = {
        "lane": "ac6-recomp",
        "implementation_commit": "dcd41b7457fcac8242f8ef40de83d1719390d5af",
        "binary_sha256": "9" * 64,
        "platform": "linux-x64-vulkan",
    }
    candidate_header["session"]["runtime_config_sha256"] = "8" * 64
    candidate_events = events()
    for event in candidate_events:
        if event["kind"] == "poll":
            event["thread_id"] += 100
            if event["state_ptr"]:
                event["state_ptr"] += 0x1000
    report = compare_runs(
        document(),
        document(seal_replay(candidate_header, candidate_events, 3)),
    )
    assert report["equal"] is True
    assert report["comparison_policy"] == "cross_lane_portable"


def test_header_cadence_and_input_bounds_fail_closed(monkeypatch: pytest.MonkeyPatch) -> None:
    bad_cadence = header()
    bad_cadence["sync"]["cadence"]["source_hz"] = 60
    with pytest.raises(ReplayError, match="unqualified cadence"):
        seal_replay(bad_cadence, events())

    long_platform = header()
    long_platform["producer"]["platform"] = "x" * 129
    with pytest.raises(ReplayError, match="platform string"):
        seal_replay(long_platform, events())

    monkeypatch.setattr(replay_module, "MAX_REPLAY_BYTES", 4)
    with pytest.raises(ReplayError, match="byte bound"):
        load_replay_bytes(b"12345")
    with pytest.raises(ReplayError, match="sealed file byte bound"):
        seal_replay(header(), events())


def test_marker_contract_is_qualified_by_expected_header() -> None:
    bad_phase = header()
    bad_phase["sync"]["marker_phase"] = "during_input"
    with pytest.raises(ReplayError, match="marker phase"):
        seal_replay(bad_phase, events())

    different_revision = header()
    different_revision["target"]["xex_sha256"] = "8" * 64
    different_revision["target"]["module_xxh3"] = "fedcba9876543210"
    different_revision["target"]["marker_address"] = "811CA908"
    different_revision["target"]["marker_code_sha256"] = "9" * 64
    data = seal_replay(different_revision, events())
    assert load_replay_bytes(data).header["target"] == different_revision["target"]
    with pytest.raises(ReplayError, match="runtime target mismatch"):
        load_replay_bytes(data, header())
    with pytest.raises(ReplayError, match="comparison target mismatch"):
        compare_runs(document(), document(data))

    manager_header = header()
    manager_header["target"]["marker_address"] = "8226D1C8"
    manager_header["sync"]["marker_role"] = "mission_manager_tick"
    manager_header["sync"]["marker_phase"] = "after_input"
    manager_events = [
        {
            **events()[0],
            "marker_index": 1,
        },
        {
            "kind": "marker",
            "sequence": 1,
            "marker_index": 1,
            "poll_index": 1,
            "guest_tick": 101,
            "present_index": 0,
        },
    ]
    assert load_replay_bytes(seal_replay(manager_header, manager_events)).footer["marker_count"] == 1


def test_cross_lane_behavior_config_mismatch_is_rejected() -> None:
    candidate_header = header()
    candidate_header["producer"]["lane"] = "ac6-recomp"
    candidate_header["session"]["behavior_config_sha256"] = "8" * 64
    candidate = document(seal_replay(candidate_header, events(), 3))
    with pytest.raises(ReplayError, match="behavior_config_sha256"):
        compare_runs(document(), candidate)


def test_controller_projection_shoulder_yaw_and_precedence() -> None:
    assert _canonical_controller(state(buttons=0x0100))[2] == -32768
    assert _canonical_controller(state(buttons=0x0300))[2] == -32768
    assert _canonical_controller(state(buttons=0x0200))[2] == 32767


def test_verify_cli_labels_structural_and_identity_checks(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    replay_path = tmp_path / "replay.jsonl"
    header_path = tmp_path / "header.json"
    replay_path.write_bytes(seal_replay(header(), events(), 3))
    header_path.write_text(json.dumps(header()), encoding="utf-8")

    monkeypatch.setattr(sys, "argv", ["controller-replay", "verify", str(replay_path)])
    assert main() == 0
    assert "qualification=structural_only" in capsys.readouterr().out

    monkeypatch.setattr(
        sys,
        "argv",
        [
            "controller-replay",
            "verify",
            str(replay_path),
            "--expected-header",
            str(header_path),
        ],
    )
    assert main() == 0
    assert "qualification=identity_checked" in capsys.readouterr().out


def test_controller_export_amplification_is_bounded(monkeypatch: pytest.MonkeyPatch) -> None:
    measured = document(replay_header=header(30))
    monkeypatch.setattr(replay_module, "MAX_PROJECTED_FRAMES", 3)
    with pytest.raises(ReplayError, match="frame bound"):
        export_controller_tsv(measured, 1, 2)

    monkeypatch.setattr(replay_module, "MAX_PROJECTED_FRAMES", 1_000_000)
    monkeypatch.setattr(replay_module, "MAX_PROJECTED_TSV_BYTES", 10)
    with pytest.raises(ReplayError, match="byte bound"):
        export_controller_tsv(measured, 1, 1)

    with pytest.raises(ReplayError, match="marker window"):
        export_controller_tsv(measured, 1, replay_module.MAX_MARKERS + 1)


def test_target_media_and_xex_versions_are_sealed() -> None:
    replay = document()
    assert replay.header["target"]["media_id"] == "0379EFB3"
    assert replay.header["target"]["xex_version"] == "v0.0.0.11"
    assert replay.header["target"]["base_version"] == "v0.0.0.11"

    missing = header()
    del missing["target"]["media_id"]
    with pytest.raises(ReplayError, match="target shape"):
        seal_replay(missing, events())
    malformed = header()
    malformed["target"]["xex_version"] = "0.0.0.11"
    with pytest.raises(ReplayError, match="target xex_version"):
        seal_replay(malformed, events())


def test_slice_reseal_scopes_measured_cadence_and_lineage() -> None:
    parent_header = header()
    parent = seal_replay(parent_header, events(), 3)
    parent_document = load_replay_bytes(parent)
    window = slice_replay(parent, 1, 2, 30, 60, parent_header)
    sliced = load_replay_bytes(window)

    assert parent_document.header["sync"]["cadence"]["status"] == "unqualified"
    assert sliced.header["sync"]["cadence"] == {
        "status": "measured",
        "source_hz": 30,
        "native_hz": 60,
        "resampling": "zero_order_hold",
        "projection": "unique_successful_user0_poll",
    }
    assert sliced.header["segment"] == {
        "kind": "marker_window",
        "parent_replay_sha256": hashlib.sha256(parent).hexdigest(),
        "parent_payload_sha256": parent_document.footer["payload_sha256"],
        "parent_start_marker": 1,
        "parent_marker_count": 2,
    }
    assert [event["sequence"] for event in sliced.events] == list(range(5))
    assert [event["poll_index"] for event in sliced.events if event["kind"] == "poll"] == [0, 1, 2]
    assert [event["marker_index"] for event in sliced.events if event["kind"] == "marker"] == [1, 2]


def test_slice_reseal_fails_closed_on_scope_and_cadence() -> None:
    parent_header = header()
    parent = seal_replay(parent_header, events(), 3)
    with pytest.raises(ReplayError, match="slice cadence"):
        slice_replay(parent, 1, 2, 40, 60, parent_header)
    with pytest.raises(ReplayError, match="marker 3 is absent"):
        slice_replay(parent, 3, 1, 60, 60, parent_header)

    measured = header(60)
    with pytest.raises(ReplayError, match="full recording cadence"):
        measured["segment"] = header()["segment"]
        seal_replay(measured, events())

    window, window_header = measured_window(60)
    with pytest.raises(ReplayError, match="full recording"):
        slice_replay(window, 1, 1, 60, 60, window_header)


def test_ac6rtply_v3_exact_layout_and_metadata_only_receipt(tmp_path: Path) -> None:
    cache, cache_digest = make_cache(tmp_path)
    window, _ = measured_window(30)
    assert read_cache_identity(cache) == cache_digest
    replay, receipt = build_ac6rtply_v3(window, cache_digest)

    assert replay[:9] == b"AC6RTPLY\0"
    assert struct.unpack_from("<IIIIII", replay, 9) == (3, 1, 1, 1, 1, 1)
    assert replay[33:65].hex() == cache_digest
    assert struct.unpack_from("<QI", replay, 65) == (0xAC60000000000001, 0)
    assert struct.unpack_from("<Q", replay, 77) == (4,)
    input_digest = replay[85:117]
    assert struct.unpack_from("<I", replay, 117) == (4,)
    expected_frames = b"".join(
        (
            struct.pack("<hhhBH", -654, 456, 32767, 127, 0x0200) * 2,
            struct.pack("<hhhBH", 222, -111, -20, 127, 0) * 2,
        )
    )
    assert replay[121:] == expected_frames
    assert input_digest == hashlib.sha256(expected_frames).digest()
    assert len(replay) == 121 + 4 * 9

    assert receipt["schema"] == "ac6.native-controller-projection-receipt.v1"
    assert receipt["source"]["raw_replay_sha256"] == hashlib.sha256(window).hexdigest()
    assert receipt["source"]["parent_window"] == {"start_marker": 1, "marker_count": 2}
    assert receipt["cadence"] == {
        "source_hz": 30,
        "native_hz": 60,
        "resampling": "zero_order_hold",
        "hold": 2,
    }
    assert receipt["cache_index_sha256"] == cache_digest
    assert receipt["output"]["frame_count"] == 4
    assert receipt["output"]["final_tick"] == 4
    assert receipt["output"]["checkpoint_count"] == 0
    assert receipt["output"]["input_digest_sha256"] == input_digest.hex()
    assert receipt["output"]["final_digest_sha256"] == input_digest.hex()
    assert receipt["output"]["output_sha256"] == hashlib.sha256(replay).hexdigest()
    assert "frames" not in receipt["output"]
    assert canonical_line(receipt) == canonical_line(json.loads(canonical_line(receipt)))


def test_ac6rtply_projection_refuses_full_recording_and_ambiguous_poll() -> None:
    full = seal_replay(header(), events(), 3)
    with pytest.raises(ReplayError, match="resealed marker window"):
        build_ac6rtply_v3(full, "a" * 64)

    ambiguous = events()
    ambiguous[2]["user_index"] = 0
    measured = header(60)
    raw = seal_replay(measured, ambiguous, 3)
    with pytest.raises(ReplayError, match="has 2 successful"):
        build_ac6rtply_v3(raw, "a" * 64)


def test_cache_identity_and_projection_bounds_fail_closed(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    cache, digest = make_cache(tmp_path)
    index = cache / "indices" / f"{digest}.ac6idx"
    index.write_bytes(b"corrupt")
    with pytest.raises(ReplayError, match="cache index"):
        read_cache_identity(cache)

    window, _ = measured_window(30)
    monkeypatch.setattr(replay_module, "MAX_PROJECTED_FRAMES", 3)
    with pytest.raises(ReplayError, match="frame bound"):
        build_ac6rtply_v3(window, "a" * 64)


def test_atomic_outputs_never_overwrite_and_pair_rolls_back(tmp_path: Path) -> None:
    output = tmp_path / "one.bin"
    _atomic_write_new(output, b"first", 5)
    with pytest.raises(ReplayError, match="already exists"):
        _atomic_write_new(output, b"second", 6)
    assert output.read_bytes() == b"first"

    projected = tmp_path / "projected.ac6rply"
    receipt = tmp_path / "receipt.json"
    receipt.write_bytes(b"owned")
    with pytest.raises(ReplayError, match="already exists"):
        _publish_atomic_files(((projected, b"replay", 6), (receipt, b"new", 3)))
    assert not projected.exists()
    assert receipt.read_bytes() == b"owned"
    assert not list(tmp_path.glob(".*.tmp"))


def test_slice_and_project_cli_write_new_outputs_atomically(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    parent_header = header()
    parent = seal_replay(parent_header, events(), 3)
    parent_path = tmp_path / "parent.jsonl"
    parent_header_path = tmp_path / "parent-header.json"
    window_path = tmp_path / "window.jsonl"
    parent_path.write_bytes(parent)
    parent_header_path.write_text(json.dumps(parent_header), encoding="utf-8")

    monkeypatch.setattr(
        sys,
        "argv",
        [
            "controller-replay",
            "slice-reseal",
            str(parent_path),
            str(window_path),
            "--expected-header",
            str(parent_header_path),
            "--start-marker",
            "1",
            "--marker-count",
            "2",
            "--source-hz",
            "30",
        ],
    )
    assert main() == 0
    assert "controller_replay=sliced" in capsys.readouterr().out
    window = load_replay_bytes(window_path.read_bytes())
    window_header_path = tmp_path / "window-header.json"
    window_header_path.write_text(json.dumps(window.header), encoding="utf-8")
    cache, _ = make_cache(tmp_path)
    output_path = tmp_path / "mission01.ac6rply"
    receipt_path = tmp_path / "mission01.receipt.json"

    monkeypatch.setattr(
        sys,
        "argv",
        [
            "controller-replay",
            "project-ac6rtply-v3",
            str(window_path),
            str(cache),
            str(output_path),
            str(receipt_path),
            "--expected-header",
            str(window_header_path),
        ],
    )
    assert main() == 0
    assert "format=AC6RTPLY version=3 frames=4" in capsys.readouterr().out
    assert output_path.read_bytes()[:9] == b"AC6RTPLY\0"
    receipt_bytes = receipt_path.read_bytes()
    assert receipt_bytes.endswith(b"\n")
    assert canonical_line(json.loads(receipt_bytes)) == receipt_bytes

    assert main() == 1
    assert "already exists" in capsys.readouterr().out
    assert struct.unpack_from("<I", output_path.read_bytes(), 9) == (3,)
