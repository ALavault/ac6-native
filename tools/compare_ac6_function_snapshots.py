#!/usr/bin/env python3
"""Compare PPC, generated and native function snapshots.

Snapshots are intentionally backend-neutral JSON documents. The comparator
ignores provenance-only metadata, validates that all three cases name the same
function and test case, and classifies which implementation diverges.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


SCHEMA = "ac6.function-snapshot.v1"
REPORT_SCHEMA = "ac6.function-snapshot-comparison.v1"
IGNORED_TOP_LEVEL = {"identity", "provenance", "notes"}


class SnapshotError(ValueError):
    pass


def load_snapshot(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SnapshotError(f"cannot read {path}: {exc}") from exc
    if document.get("schema") != SCHEMA:
        raise SnapshotError(f"unsupported snapshot schema in {path}")
    identity = document.get("identity")
    if not isinstance(identity, dict):
        raise SnapshotError(f"missing identity in {path}")
    for field in ("function", "case", "implementation"):
        if not isinstance(identity.get(field), str) or not identity[field]:
            raise SnapshotError(f"invalid identity.{field} in {path}")
    return document


def semantic_payload(document: dict[str, Any]) -> dict[str, Any]:
    return {
        key: value
        for key, value in document.items()
        if key != "schema" and key not in IGNORED_TOP_LEVEL
    }


def values_equal(left: Any, right: Any, *, abs_tol: float, rel_tol: float) -> bool:
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        if isinstance(left, float) or isinstance(right, float):
            return math.isclose(float(left), float(right), abs_tol=abs_tol, rel_tol=rel_tol)
        return left == right
    return left == right


def diff_values(
    left: Any,
    right: Any,
    *,
    path: str = "$",
    abs_tol: float,
    rel_tol: float,
    limit: int = 256,
) -> list[dict[str, Any]]:
    diffs: list[dict[str, Any]] = []

    def append(record: dict[str, Any]) -> None:
        if len(diffs) < limit:
            diffs.append(record)

    def walk(a: Any, b: Any, current: str) -> None:
        if len(diffs) >= limit:
            return
        if isinstance(a, dict) and isinstance(b, dict):
            for key in sorted(set(a) | set(b)):
                child_path = f"{current}.{key}"
                if key not in a:
                    append({"path": child_path, "kind": "missing_left", "right": b[key]})
                elif key not in b:
                    append({"path": child_path, "kind": "missing_right", "left": a[key]})
                else:
                    walk(a[key], b[key], child_path)
            return
        if isinstance(a, list) and isinstance(b, list):
            common = min(len(a), len(b))
            for index in range(common):
                walk(a[index], b[index], f"{current}[{index}]")
                if len(diffs) >= limit:
                    return
            if len(a) != len(b):
                append(
                    {
                        "path": current,
                        "kind": "length",
                        "left": len(a),
                        "right": len(b),
                    }
                )
            return
        if type(a) is not type(b) and not (
            isinstance(a, (int, float)) and isinstance(b, (int, float))
        ):
            append(
                {
                    "path": current,
                    "kind": "type",
                    "left_type": type(a).__name__,
                    "right_type": type(b).__name__,
                    "left": a,
                    "right": b,
                }
            )
            return
        if not values_equal(a, b, abs_tol=abs_tol, rel_tol=rel_tol):
            append({"path": current, "kind": "value", "left": a, "right": b})

    walk(left, right, path)
    return diffs


def compare_pair(
    left: dict[str, Any],
    right: dict[str, Any],
    *,
    abs_tol: float,
    rel_tol: float,
) -> dict[str, Any]:
    diffs = diff_values(
        semantic_payload(left),
        semantic_payload(right),
        abs_tol=abs_tol,
        rel_tol=rel_tol,
    )
    return {"equal": not diffs, "difference_count": len(diffs), "differences": diffs}


def compare_three(
    ppc: dict[str, Any],
    generated: dict[str, Any],
    native: dict[str, Any],
    *,
    abs_tol: float = 0.0,
    rel_tol: float = 0.0,
) -> dict[str, Any]:
    identities = [document["identity"] for document in (ppc, generated, native)]
    functions = {identity["function"] for identity in identities}
    cases = {identity["case"] for identity in identities}
    if len(functions) != 1 or len(cases) != 1:
        raise SnapshotError(
            f"snapshots do not describe one function/case: functions={sorted(functions)} "
            f"cases={sorted(cases)}"
        )
    implementations = [identity["implementation"] for identity in identities]
    if len(set(implementations)) != 3:
        raise SnapshotError(f"implementation identities must be distinct: {implementations}")

    ppc_generated = compare_pair(ppc, generated, abs_tol=abs_tol, rel_tol=rel_tol)
    ppc_native = compare_pair(ppc, native, abs_tol=abs_tol, rel_tol=rel_tol)
    generated_native = compare_pair(
        generated, native, abs_tol=abs_tol, rel_tol=rel_tol
    )

    pg = ppc_generated["equal"]
    pn = ppc_native["equal"]
    gn = generated_native["equal"]
    if pg and pn:
        classification = "all_equal"
    elif pn and not pg:
        classification = "generated_diverges"
    elif pg and not pn:
        classification = "native_diverges"
    elif gn and not pg and not pn:
        classification = "ppc_or_microexec_diverges"
    else:
        classification = "all_diverge"

    return {
        "schema": REPORT_SCHEMA,
        "function": next(iter(functions)),
        "case": next(iter(cases)),
        "implementations": {
            "ppc": ppc["identity"]["implementation"],
            "generated": generated["identity"]["implementation"],
            "native": native["identity"]["implementation"],
        },
        "classification": classification,
        "equal": classification == "all_equal",
        "tolerances": {"absolute": abs_tol, "relative": rel_tol},
        "pairs": {
            "ppc_generated": ppc_generated,
            "ppc_native": ppc_native,
            "generated_native": generated_native,
        },
        "policy": {
            "ignored_top_level": sorted(IGNORED_TOP_LEVEL),
            "list_order_significant": True,
            "memory_write_order_significant": True,
            "float_comparison": "math.isclose",
        },
    }


def fail(message: str) -> int:
    print(f"function_snapshot_compare=fail reason={message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc", type=Path, required=True)
    parser.add_argument("--generated", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--float-abs", type=float, default=0.0)
    parser.add_argument("--float-rel", type=float, default=0.0)
    parser.add_argument(
        "--allow-difference",
        action="store_true",
        help="return success even when the snapshots diverge",
    )
    args = parser.parse_args()
    if args.float_abs < 0 or args.float_rel < 0:
        parser.error("float tolerances must be non-negative")
    try:
        report = compare_three(
            load_snapshot(args.ppc),
            load_snapshot(args.generated),
            load_snapshot(args.native),
            abs_tol=args.float_abs,
            rel_tol=args.float_rel,
        )
    except SnapshotError as exc:
        return fail(str(exc))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(
        "function_snapshot_compare=pass "
        f"classification={report['classification']} "
        f"function={report['function']} case={report['case']}"
    )
    if not report["equal"] and not args.allow_difference:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
