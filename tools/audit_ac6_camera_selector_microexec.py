#!/usr/bin/env python3
"""Fail-closed controls for the bounded 0x82262A28 selector work."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
SNAPSHOT_SCHEMA = "ac6.function-snapshot.v1"


class CameraSelectorEvidenceError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CameraSelectorEvidenceError(message)


def load_snapshot(path: Path, expected_exit: str = "return") -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(document.get("schema") == SNAPSHOT_SCHEMA, f"{path.name} schema")
    provenance = document.get("provenance", {})
    require(
        provenance.get("xex_sha256") == XEX_SHA256
        and provenance.get("asserted_semantics_enabled") is False
        and provenance.get("asserted_semantics") == {}
        and provenance.get("hint_noops") == {}
        and provenance.get("register_file_bridge") is False
        and provenance.get("alias_copies") == 0,
        f"{path.name} provenance",
    )
    require(document.get("exit") == {"kind": expected_exit}, f"{path.name} exit")
    return document


def audit(root: Path = ROOT) -> None:
    evidence = root / "analysis/microexec/camera"
    direct = load_snapshot(evidence / "mode2-target-selector-direct.ppc.json")
    require(
        direct.get("identity")
        == {
            "implementation": "ppc-pcode",
            "function": "0x82262A28",
            "case": "camera-mode2-direct-selector",
        },
        "direct selector identity",
    )
    direct_provenance = direct["provenance"]
    require(
        direct_provenance.get("steps") == 217 and direct_provenance.get("callee_entries") == 20,
        "direct selector execution census",
    )
    dumps = direct.get("region_dumps")
    require(isinstance(dumps, list) and len(dumps) == 1, "direct selector dump")
    require(
        dumps[0]
        == {
            "name": "manager_rotations",
            "base": "0xb40003a0",
            "size": 12,
            "after_hex": "3e800000bd0000003e800000",
            "after_hex_b": "3e800000bd0000003e800000",
        },
        "direct selector retail result",
    )

    curve = load_snapshot(evidence / "mode3-gain-curve.ppc.json")
    require(
        curve.get("identity")
        == {
            "implementation": "ppc-pcode",
            "function": "0x8225D660",
            "case": "camera-mode3-gain-curve-quarter",
        },
        "mode3 curve identity",
    )
    curve_provenance = curve["provenance"]
    require(
        curve_provenance.get("steps") == 32 and curve_provenance.get("callee_entries") == 0,
        "mode3 curve execution census",
    )
    require(
        curve.get("registers") == {"f1": "0x3fff400000000000"},
        "mode3 curve retail result",
    )

    indirect = load_snapshot(evidence / "mode2-indirect-scalar-tail.ppc.json")
    require(
        indirect.get("identity")
        == {
            "implementation": "ppc-pcode",
            "function": "0x8226283C",
            "case": "camera-mode2-indirect-scalar-tail",
        },
        "indirect scalar-tail identity",
    )
    indirect_provenance = indirect["provenance"]
    require(
        indirect_provenance.get("function_name") == "<no function>"
        and indirect_provenance.get("steps") == 38
        and indirect_provenance.get("callee_entries") == 0,
        "indirect scalar-tail execution census",
    )
    require(
        indirect.get("calls")
        == [
            {
                "target": "0x82262a10",
                "ordinal": 0,
                "note": "bounded scalar tail return, arg 0x00000000",
            }
        ],
        "indirect scalar-tail bounded return",
    )
    require(
        indirect.get("memory_writes")
        == [
            {
                "address": "0xb6000000",
                "size": 4,
                "after_hex": "3f000000",
                "after_hex_b": "3f000000",
            },
            {
                "address": "0xb6000010",
                "size": 4,
                "after_hex": "bf000000",
                "after_hex_b": "bf000000",
            },
        ],
        "indirect scalar-tail retail result",
    )

    small_direction = load_snapshot(evidence / "mode2-indirect-small-direction.ppc.json", "step_limit")
    require(
        small_direction.get("identity")
        == {
            "implementation": "ppc-pcode",
            "function": "0x82262738",
            "case": "camera-mode2-indirect-small-direction",
        },
        "indirect small-direction identity",
    )
    small_provenance = small_direction["provenance"]
    require(
        small_provenance.get("function_name") == "<no function>"
        and small_provenance.get("steps") == 134
        and small_provenance.get("callee_entries") == 2
        and small_provenance.get("written") == "output 16 bytes",
        "indirect small-direction execution census",
    )
    require(
        [
            (region.get("name"), region.get("base"), region.get("kind"), region.get("size"))
            for region in small_provenance.get("regions", [])
        ]
        == [
            ("stack_pre", "0xc0000000", "zero", 3920),
            ("output", "0xc0000f50", "poison", 16),
            ("direction", "0xc0000f60", "bytes", 16),
            ("stack_tail", "0xc0000f70", "zero", 144),
        ],
        "indirect small-direction regions",
    )
    small_direction_bytes = "3680000036000000b680000000000000"
    require(
        small_direction.get("registers")
        == {
            "f28": "0x3ec0000000000000",
            "f31": "0xbfe921fb60000000",
        },
        "indirect small-direction scalar result",
    )
    require(
        small_direction.get("calls") == []
        and small_direction.get("memory_writes")
        == [
            {
                "address": "0xc0000f50",
                "size": 16,
                "after_hex": small_direction_bytes,
                "after_hex_b": small_direction_bytes,
            }
        ]
        and small_direction.get("region_dumps")
        == [
            {
                "name": "output",
                "base": "0xc0000f50",
                "size": 16,
                "after_hex": small_direction_bytes,
                "after_hex_b": small_direction_bytes,
            }
        ],
        "indirect small-direction copied output",
    )

    normalizer = load_snapshot(evidence / "mode3-axis-normalizer.ppc.json")
    require(
        normalizer.get("identity")
        == {
            "implementation": "ppc-pcode",
            "function": "0x8225C680",
            "case": "camera-mode3-normalizer-axis-x",
        },
        "mode3 axis normalizer identity",
    )
    normalizer_provenance = normalizer["provenance"]
    require(
        normalizer_provenance.get("function_name") == "Function_8225C680"
        and normalizer_provenance.get("steps") == 208
        and normalizer_provenance.get("callee_entries") == 8
        and normalizer_provenance.get("written") == "",
        "mode3 axis normalizer execution census",
    )
    require(
        [
            (region.get("name"), region.get("base"), region.get("kind"), region.get("size"))
            for region in normalizer_provenance.get("regions", [])
        ]
        == [
            ("constant_zero", "0x8200082c", "bytes", 4),
            ("constant_one", "0x82001348", "bytes", 4),
            ("default_scale", "0x8206a030", "bytes", 4),
            ("global_root_pointer", "0x826e4eb4", "bytes", 4),
            ("manager_slot", "0xb402f9a0", "bytes", 4),
            ("alternate_scale", "0xb50004a8", "bytes", 1),
            ("axes", "0xb6000000", "bytes", 8),
            ("stack", "0xc0000000", "zero", 8192),
        ],
        "mode3 axis normalizer regions",
    )
    require(
        normalizer.get("calls") == []
        and normalizer.get("memory_writes") == []
        and normalizer.get("region_dumps")
        == [
            {
                "name": "axes",
                "base": "0xb6000000",
                "size": 8,
                "after_hex": "3f800000b33bbd2e",
                "after_hex_b": "3f800000b33bbd2e",
            }
        ],
        "mode3 axis normalizer retail result",
    )


def main() -> int:
    try:
        audit()
    except (OSError, json.JSONDecodeError, KeyError, ValueError) as error:
        print(f"camera_selector_microexec=fail error={error}", file=sys.stderr)
        return 1
    print(
        "camera_selector_microexec=pass direct=217 curve=32 "
        "indirect_tail=38 small_direction=134 normalizer=208 substituted=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
