#!/usr/bin/env python3
"""Fail-closed controls for the bounded 0x82262A28 selector work."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
XEX_SHA256 = (
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
)
SNAPSHOT_SCHEMA = "ac6.function-snapshot.v1"


class CameraSelectorEvidenceError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CameraSelectorEvidenceError(message)


def load_snapshot(path: Path) -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(document.get("schema") == SNAPSHOT_SCHEMA, f"{path.name} schema")
    provenance = document.get("provenance", {})
    require(
        provenance.get("xex_sha256") == XEX_SHA256
        and provenance.get("asserted_semantics_enabled") is False
        and provenance.get("asserted_semantics") == {}
        and provenance.get("register_file_bridge") is False,
        f"{path.name} provenance",
    )
    require(document.get("exit") == {"kind": "return"}, f"{path.name} exit")
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
        direct_provenance.get("steps") == 217
        and direct_provenance.get("callee_entries") == 20,
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
        curve_provenance.get("steps") == 32
        and curve_provenance.get("callee_entries") == 0,
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


def main() -> int:
    try:
        audit()
    except (OSError, json.JSONDecodeError, KeyError, ValueError) as error:
        print(f"camera_selector_microexec=fail error={error}", file=sys.stderr)
        return 1
    print(
        "camera_selector_microexec=pass direct=217 curve=32 "
        "indirect_tail=38 substituted=0"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
