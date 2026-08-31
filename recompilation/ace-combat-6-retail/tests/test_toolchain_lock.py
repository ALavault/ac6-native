from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_toolchain", ROOT / "tools/validate_toolchain.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_pinned_tool_checkouts_are_clean() -> None:
    if not (
        (MODULE.PORTFOLIO / ".tools/xenonrecomp-source/XenonRecomp/.git").exists()
        and (MODULE.PORTFOLIO / ".tools/xenosrecomp-source/XenosRecomp/.git").exists()
    ):
        pytest.skip("optional analysis-tool checkouts are not installed")
    result = MODULE.validate_lock()
    assert result["tools"]["XenonRecomp"]["commit"] == (
        "ddd128bcca99fe8bfbb99bea583c972351fa6ace"
    )
    assert result["tools"]["XenosRecomp"]["commit"] == (
        "990d03b28a27b50277ee5d8d942e1c5f873869d1"
    )
    assert result["architecture_catalog"] == "absent"
