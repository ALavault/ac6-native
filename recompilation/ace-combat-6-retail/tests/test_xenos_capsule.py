from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("xenos_capsule", ROOT / "tools/xenos_capsule.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
CAPTURE_SPEC = importlib.util.spec_from_file_location(
    "capture_xenos_capsule", ROOT / "tools/capture_xenos_capsule.py"
)
assert CAPTURE_SPEC and CAPTURE_SPEC.loader
CAPTURE = importlib.util.module_from_spec(CAPTURE_SPEC)
CAPTURE_SPEC.loader.exec_module(CAPTURE)


def capsule() -> dict:
    return json.loads((ROOT / "native/fixtures/xenos-capsule-minimal.json").read_text())


def test_minimal_capsule_is_canonical_and_valid() -> None:
    result = MODULE.validate_capsule(capsule())
    assert result["schema"] == "ac6.xenos-capsule.v1"
    assert result["validated"] == {"event_count": 6, "pm4_dword_count": 7}


def test_unknown_opcode_reports_first_dword_offset() -> None:
    document = capsule()
    document["events"] = [{"kind": "pm4", "dwords": [0xC0EE0000, 0]}]
    with pytest.raises(MODULE.CapsuleError) as raised:
        MODULE.validate_capsule(document)
    assert raised.value.offset == 0


def test_capsule_rejects_retail_payload_field() -> None:
    document = capsule()
    document["events"][0]["retail_bytes"] = "deadbeef"
    # The field is intentionally not part of the schema; reject it before any
    # future producer can accidentally smuggle a tracked payload into a capsule.
    with pytest.raises(MODULE.CapsuleError):
        MODULE.validate_capsule(document)


def test_read_only_capture_rejects_guest_effects(tmp_path: Path) -> None:
    event_log = tmp_path / "oracle.jsonl"
    event_log.write_text(json.dumps({"kind": "guest_write", "address": 4}) + "\n")
    with pytest.raises(CAPTURE.CaptureError):
        CAPTURE.read_events(event_log)


def test_read_only_capture_writes_canonical_capsule(tmp_path: Path) -> None:
    event_log = tmp_path / "oracle.jsonl"
    event_log.write_text(
        "".join(json.dumps(event) + "\n" for event in capsule()["events"])
    )
    output = tmp_path / "nested/capsule.json"
    result = CAPTURE.capture(
        event_log,
        output,
        capsule()["source"]["xex_sha256"],
        capsule()["source"]["iso_sha256"],
        capsule()["source"]["ghidra_project"],
        capsule()["source"]["route_sha256"],
        64,
    )
    assert output.is_file()
    assert result["validated"]["event_count"] == 6
