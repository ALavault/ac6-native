from __future__ import annotations

import importlib.util
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/run_campaign.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_run_campaign", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def test_plan_rejects_missing_missions_before_file_resolution() -> None:
    plan = {
        "schema": MODULE.PLAN_SCHEMA,
        "target": "ntsc-uj",
        "binary_sha256": "a" * 64,
        "missions": [],
    }
    try:
        MODULE.validate_plan(plan, target="ntsc-uj", binary_sha256="a" * 64)
    except MODULE.CampaignRunError as error:
        assert "15 missions" in str(error)
    else:
        raise AssertionError("partial campaign plan accepted")


def test_orchestrator_preserves_segmented_chain_contract() -> None:
    source = SCRIPT.read_text()
    assert "fresh_process_per_mission" in source
    assert '"--storage-seed", str(previous_output / "storage-root")' in source
    assert '"--cache-seed", str(previous_output)' in source
    assert "self-built-from-empty-mission01-then-carried-forward" in source
    assert 'range(1, 16)' in source
    assert "AUDIT.json" in source and "DEBRIEF.json" in source and "VISUAL.json" in source
