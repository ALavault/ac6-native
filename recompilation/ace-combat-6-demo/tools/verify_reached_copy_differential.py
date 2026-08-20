#!/usr/bin/env python3
"""Fail-closed source audit for the reached copy differential certificate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

REQUIRED = {
    "include/ac6demo/reached_copy_differential.hpp": [
        "enum class ReachedCopyFailureStage",
        "diagnose_reached_copy(",
        "EdramMaterialization",
        "CopyPixels",
        "DestinationPadding",
        "first_pixel_difference",
        "first_padding_difference",
        "require_exact_reached_copy",
    ],
    "tests/reached_copy_differential_tests.cpp": [
        "kBadX = 1111U",
        "first_padding_offset()",
        "EdramMaterialization",
        "DestinationPadding",
    ],
    "tools/diagnose_reached_copy.py": [
        "ac6-demo-reached-copy-differential/v1",
        "edram_materialization",
        "copy_pixels",
        "destination_padding",
        "--tiled-resolve",
    ],
}

FORBIDDEN = {
    "include/ac6demo/reached_copy_differential.hpp": [
        "AC6_DEMO_EXPERIMENTAL_NONBLACK_RESOLVE",
        "store_guest_bytes",
    ],
    "tools/diagnose_reached_copy.py": [
        "github.com",
        "requests.",
        "urllib.request",
    ],
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, nargs="?", default=Path(__file__).resolve().parents[1])
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    failures: list[str] = []

    for relative, needles in REQUIRED.items():
        path = root / relative
        if not path.is_file():
            failures.append(f"missing {relative}")
            continue
        text = path.read_text(encoding="utf-8")
        for needle in needles:
            if needle not in text:
                failures.append(f"{relative}: missing {needle!r}")
        for needle in FORBIDDEN.get(relative, []):
            if needle in text:
                failures.append(f"{relative}: forbidden {needle!r}")

    contract_path = root.parents[1] / "analysis/demo/ac6-demo-nonblack-resolve-20260820/contract.json"
    if not contract_path.is_file():
        failures.append("missing contract.json")
    else:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        if contract.get("schema") != "ac6-demo-reached-copy-differential/v1":
            failures.append("contract schema mismatch")
        policy = contract.get("policy", {})
        if policy.get("runtime_default_changed") is not False:
            failures.append("runtime_default_changed must remain false")
        if policy.get("nonblack_guest_writeback_enabled") is not False:
            failures.append("nonblack_guest_writeback_enabled must remain false")
        if policy.get("github_actions_used") is not False:
            failures.append("github_actions_used must remain false")
        if policy.get("pull_request_used") is not False:
            failures.append("pull_request_used must remain false")

    result = {
        "schema": "ac6-demo-reached-copy-differential-source-audit/v1",
        "status": "PASS" if not failures else "FAIL",
        "failures": failures,
    }
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json is not None:
        args.json.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
