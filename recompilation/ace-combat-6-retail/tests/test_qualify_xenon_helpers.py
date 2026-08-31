from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "qualify_xenon_helpers", ROOT / "tools/qualify_xenon_helpers.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_helper_signatures_require_unique_aligned_hits(tmp_path: Path) -> None:
    payload = bytearray(256)
    for index, encoded in enumerate(MODULE.HELPER_SIGNATURES.values()):
        payload[index * 16 : index * 16 + len(bytes.fromhex(encoded))] = bytes.fromhex(encoded)
    basefile = tmp_path / "base.bin"
    basefile.write_bytes(payload)
    result = MODULE.qualify(basefile, 0x82000000)
    assert result["status"] == "qualified"
    assert len(result["helpers"]) == 8


def test_helper_qualification_fails_on_missing_signature(tmp_path: Path) -> None:
    basefile = tmp_path / "base.bin"
    basefile.write_bytes(b"\x00" * 256)
    with pytest.raises(MODULE.HelperQualificationError, match="savegprlr_14"):
        MODULE.qualify(basefile)
