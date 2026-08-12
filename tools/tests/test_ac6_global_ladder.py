import importlib.util
import json
from pathlib import Path

import pytest


SCRIPT = Path(__file__).resolve().parents[1] / "audit_ac6_global_ladder.py"
SPEC = importlib.util.spec_from_file_location("audit_ac6_global_ladder", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def test_global_ladder_contract() -> None:
    MODULE.audit()


def test_runtime_phase_cannot_pass_without_qualified_oracle_capture(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    document = json.loads(MODULE.SPINE.read_text(encoding="utf-8"))
    document["phases"][1]["status"] = "passed"
    document["phases"][1]["evidence"] = document["phases"][0]["evidence"]
    path = tmp_path / "spine.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    monkeypatch.setattr(MODULE, "SPINE", path)
    with pytest.raises(ValueError, match="unqualified oracle pass"):
        MODULE.audit_spine({"JF": "passed", "JV": "open", "JP": "open", "JG": "open",
                            "supported": "no"})


def test_capability_matrix_cannot_outrun_spine() -> None:
    with pytest.raises(ValueError, match="matrix outruns execution spine"):
        MODULE.audit_spine({"JF": "passed", "JV": "passed", "JP": "open", "JG": "open",
                            "supported": "no"})


def test_rexglue_provisional_semantics_cannot_close_a_gate(
        tmp_path: Path) -> None:
    document = json.loads(MODULE.REXGLUE_TRUST.read_text(encoding="utf-8"))
    document["policy"]["provisional_allows_lane_closure"] = True
    path = tmp_path / "rexglue-trust.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    with pytest.raises(ValueError, match="negative policy"):
        MODULE.audit_rexglue_trust(path)


def test_known_rexglue_divergence_cannot_become_provisional(
        tmp_path: Path) -> None:
    document = json.loads(MODULE.REXGLUE_TRUST.read_text(encoding="utf-8"))
    document["known_divergences"][0]["status"] = "provisional-rexglue"
    path = tmp_path / "rexglue-trust.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    with pytest.raises(ValueError, match="divergence contract"):
        MODULE.audit_rexglue_trust(path)


def test_unreviewed_rexglue_revision_cannot_inherit_trust(
        tmp_path: Path) -> None:
    document = json.loads(MODULE.REXGLUE_TRUST.read_text(encoding="utf-8"))
    document["revision_scope"]["unreviewed_revision_inherits"] = "provisional-rexglue"
    path = tmp_path / "rexglue-trust.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    with pytest.raises(ValueError, match="revision scope"):
        MODULE.audit_rexglue_trust(path)


def test_rexglue_pal_binary_identity_is_bound_to_manifest(
        tmp_path: Path) -> None:
    document = json.loads(MODULE.REXGLUE_TRUST.read_text(encoding="utf-8"))
    document["source_pins"]["pal_oracle"]["oracle_binary_sha256"] = "0" * 64
    path = tmp_path / "rexglue-trust.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    with pytest.raises(ValueError, match="PAL revision identity"):
        MODULE.audit_rexglue_trust(path)
