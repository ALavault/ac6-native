from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "audit_native_release", ROOT / "tools/audit_native_release.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_clean_prefix_passes(tmp_path: Path) -> None:
    prefix = tmp_path / "install"
    (prefix / "bin").mkdir(parents=True)
    (prefix / "bin/ac6recomp").write_bytes(b"native")
    result = MODULE.audit(prefix)
    assert result["status"] == "pass"
    assert result["forbidden"] == 0


def test_oracle_and_retail_payloads_fail_closed(tmp_path: Path) -> None:
    prefix = tmp_path / "install"
    (prefix / "bin").mkdir(parents=True)
    (prefix / "bin/ac6recomp").write_bytes(b"native")
    (prefix / "librexglue.so").write_bytes(b"oracle")
    with pytest.raises(MODULE.ReleaseAuditError, match="forbidden"):
        MODULE.audit(prefix)


def test_missing_executable_fails_closed(tmp_path: Path) -> None:
    with pytest.raises(MODULE.ReleaseAuditError, match="missing"):
        MODULE.audit(tmp_path)


def test_external_symlink_fails_closed(tmp_path: Path) -> None:
    prefix = tmp_path / "install"
    (prefix / "bin").mkdir(parents=True)
    (prefix / "bin/ac6recomp").write_bytes(b"native")
    (prefix / "librexglue.so").symlink_to("/etc/hostname")
    with pytest.raises(MODULE.ReleaseAuditError, match="forbidden"):
        MODULE.audit(prefix)
