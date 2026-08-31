from __future__ import annotations

import importlib.util
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/validate.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_validate", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def test_d3d12_filter_ignores_hexadecimal_nm_addresses() -> None:
    symbols = "0000000000d3d120 t __imp__rex_sub_82171630\n"
    assert MODULE.forbidden_d3d12_symbols(symbols) == []


def test_d3d12_filter_rejects_backend_symbol_text() -> None:
    symbols = "0000000000010000 T rex::graphics::d3d12::D3D12GraphicsSystem\n"
    assert MODULE.forbidden_d3d12_symbols(symbols) == [symbols.rstrip()]


def test_ntsc_validation_includes_campaign_manifest() -> None:
    command = MODULE.campaign_validation_command("ntsc-uj", False)
    assert command is not None
    assert command[-2:] == ["--target", "ntsc-uj"]


def test_release_validation_promotes_campaign_gate() -> None:
    command = MODULE.campaign_validation_command("ntsc-uj", True)
    assert command is not None
    assert command[-1] == "--require-release"


def test_pal_has_no_us_campaign_manifest() -> None:
    assert MODULE.campaign_validation_command("pal", False) is None


def test_native_campaign_validation_selects_native_renderer_contract() -> None:
    command = MODULE.campaign_validation_command("ntsc-uj", False, "native")
    assert command is not None
    assert command[-2:] == ["--runtime", "native"]
