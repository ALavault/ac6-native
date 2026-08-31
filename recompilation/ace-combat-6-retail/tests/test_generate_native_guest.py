from __future__ import annotations

import importlib.util
import json


SPEC = importlib.util.spec_from_file_location(
    "generate_native_guest",
    __import__("pathlib").Path(__file__).resolve().parents[1] / "tools/generate_native_guest.py",
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def test_generator_diagnostics_are_machine_readable() -> None:
    result = MODULE.diagnostics(
        "ERROR: Switch case at 8235A060 is trying to jump outside function\n"
        "Unrecognized instruction at 0x823D32C4: mulhdu\n"
    )
    assert result["count"] == 2
    assert result["unrecognized_instructions"] == [
        {"address": "0x823D32C4", "instruction": "mulhdu"}
    ]


def test_generator_diagnostics_empty_is_clean() -> None:
    assert MODULE.diagnostics("Recompiling functions... 100%\n")["count"] == 0


def test_helper_config_requires_all_eight_entries(tmp_path) -> None:
    path = tmp_path / "config.toml"
    path.write_text("\n".join(f"{key} = 0x{index + 0x82000000:X}" for index, key in enumerate(MODULE.HELPER_KEYS)))
    values = MODULE.qualified_helpers(path)
    assert len(values) == 8


def test_canonical_boundaries_are_identity_checked_and_materialized(tmp_path) -> None:
    document = {
        "schema": "ac6.ghidra-function-boundaries.v1",
        "project": "ac6-us",
        "program": "default.xex",
        "sha256": "abc",
        "language": "PowerPC:BE:64:Xenon",
        "function_count": 1,
        "functions": [{"entry": "0x82000000", "ranges": [["0x82000000", "0x8200000F"]]}],
    }
    boundaries = tmp_path / "boundaries.json"
    boundaries.write_text(json.dumps(document))
    result, owners = MODULE.qualified_boundaries(boundaries, "abc")
    assert result == [(0x82000000, 0x10)]
    assert owners == []
    config = tmp_path / "in.toml"
    output = tmp_path / "out.toml"
    config.write_text("[main]\nfile_path = 'assets/default.xex'\n")
    MODULE.materialize_config(config, output, result)
    assert "address = 0x82000000" in output.read_text()
