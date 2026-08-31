from __future__ import annotations

import importlib.util
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/audit_capture.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_audit_capture", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def test_capture_name_recovers_route_label() -> None:
    assert MODULE.capture_name({"path": "step-101-flight-throttle.png"}) == "flight-throttle"


def test_release_audit_is_automated_and_replay_bound() -> None:
    source = SCRIPT.read_text()
    assert "ac6.retail-gameplay-audit.v3" in source
    assert "ac6.retail-mission-debrief.v2" in source
    assert "ac6.retail-mission-visual-audit.v2" in source
    assert 'result.get("execution_mode") == "strict-replay"' in source
    assert "all five control observables must exceed 5000 changed pixels" in source
    assert "pixel_parity_claimed" in source
    assert "--reviewer" not in source
