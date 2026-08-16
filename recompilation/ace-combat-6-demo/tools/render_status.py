#!/usr/bin/env python3
"""Render the manifest-derived AC6 demo foundation status block."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
BEGIN = "<!-- BEGIN MANIFEST-DERIVED FOUNDATION -->"
END = "<!-- END MANIFEST-DERIVED FOUNDATION -->"


def render(evidence: dict[str, object]) -> str:
    ghidra = evidence["ghidra"]
    codegen = evidence["codegen"]
    reproducibility = evidence["reproducibility"]
    return "\n".join([
        BEGIN,
        f"- Canonical Ghidra: `{ghidra['project_path']}`, `{ghidra['language']}`, "
        f"{ghidra['chunks']:,} chunks and {ghidra['data_ranges']:,} data ranges.",
        f"- Strict codegen: {codegen['pdata_functions']:,} `.pdata` functions, "
        f"{codegen['configured_function_records']:,} explicit boundary records, "
        f"{codegen['confirmed_functions']:,} total functions, "
        f"{codegen['switch_tables']:,} switches and "
        f"{codegen['generated_cpp_files']:,} generated C++ units.",
        f"- Diagnostics: {codegen['boundary_diagnostics']} boundary, "
        f"{codegen['unsupported_instructions']} unsupported instruction; "
        f"two clean codegens byte-identical: "
        f"`{str(reproducibility['byte_identical']).lower()}`.",
        "- Product support remains `false`; CPU/runtime, frontend, graphics, "
        "audio, input/replay and mission gates are not all closed.",
        END,
    ])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--evidence", type=Path,
        default=WORKSPACE / "analysis/demo/ac6-demo-foundation-evidence.json",
    )
    parser.add_argument("--status", type=Path, default=PROJECT / "STATUS.md")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    evidence = json.loads(args.evidence.read_text(encoding="utf-8"))
    block = render(evidence)
    text = args.status.read_text(encoding="utf-8")
    start = text.find(BEGIN)
    end = text.find(END)
    if start < 0 or end < start:
        raise ValueError("STATUS.md has no manifest-derived block")
    end += len(END)
    expected = text[:start] + block + text[end:]
    if args.check:
        if expected != text:
            raise ValueError("STATUS.md manifest-derived block is stale")
        print("AC6_DEMO_STATUS_PASS")
        return 0
    args.status.write_text(expected, encoding="utf-8")
    print("AC6_DEMO_STATUS_UPDATED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
