from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "census_native_imports", ROOT / "tools/census_native_imports.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_census_is_unique_and_network_is_offline(tmp_path: Path) -> None:
    mapping = tmp_path / "mapping.cpp"
    mapping.write_text("{ __imp__VdSwap }, { __imp__NetDll_socket }, { __imp__NetDll_socket }\n")
    result = MODULE.census(mapping, "ABC")
    assert result["import_count"] == 2
    assert result["offline_network"]["socket_creation"] is False
    assert result["categories"]["vd"] == ["VdSwap"]
