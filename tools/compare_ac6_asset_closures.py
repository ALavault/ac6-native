#!/usr/bin/env python3
"""Compare two content-addressed AC6 asset closures deterministically."""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


SCHEMA = "ac6.asset-closure.v1"
REPORT_SCHEMA = "ac6.asset-closure-comparison.v1"


class ComparisonError(ValueError):
    pass


def load_closure(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ComparisonError(f"cannot read {path}: {exc}") from exc
    if document.get("schema") != SCHEMA:
        raise ComparisonError(f"unsupported closure schema in {path}")
    if not isinstance(document.get("nodes"), list) or not isinstance(document.get("roots"), list):
        raise ComparisonError(f"malformed closure in {path}")
    return document


def node_map(document: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for node in document["nodes"]:
        if not isinstance(node, dict):
            raise ComparisonError("node must be an object")
        digest = node.get("sha256")
        if not isinstance(digest, str) or len(digest) != 64:
            raise ComparisonError("invalid node SHA-256")
        if digest in result:
            raise ComparisonError(f"duplicate node SHA-256: {digest}")
        result[digest] = node
    return result


def root_map(document: dict[str, Any]) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}
    for root in document["roots"]:
        if not isinstance(root, dict) or type(root.get("data_tbl_index")) is not int:
            raise ComparisonError("invalid root record")
        index = root["data_tbl_index"]
        if index in result:
            raise ComparisonError(f"duplicate root index: {index}")
        result[index] = root
    return result


def signature(node: dict[str, Any]) -> tuple[str, int]:
    magic = node.get("magic")
    size = node.get("size")
    if not isinstance(magic, str) or type(size) is not int or size < 0:
        raise ComparisonError("invalid node signature")
    return magic, size


def magic_counts(nodes: dict[str, dict[str, Any]]) -> Counter[str]:
    return Counter(node["magic"] for node in nodes.values())


def compare(base_path: Path, candidate_path: Path) -> dict[str, Any]:
    base = load_closure(base_path)
    candidate = load_closure(candidate_path)
    base_nodes = node_map(base)
    candidate_nodes = node_map(candidate)
    base_hashes = set(base_nodes)
    candidate_hashes = set(candidate_nodes)
    shared_hashes = sorted(base_hashes & candidate_hashes)
    base_only = sorted(base_hashes - candidate_hashes)
    candidate_only = sorted(candidate_hashes - base_hashes)

    base_by_signature: dict[tuple[str, int], set[str]] = defaultdict(set)
    candidate_by_signature: dict[tuple[str, int], set[str]] = defaultdict(set)
    for digest, node in base_nodes.items():
        base_by_signature[signature(node)].add(digest)
    for digest, node in candidate_nodes.items():
        candidate_by_signature[signature(node)].add(digest)

    changed_signatures: list[dict[str, Any]] = []
    for key in sorted(set(base_by_signature) & set(candidate_by_signature)):
        base_for_signature = base_by_signature[key]
        candidate_for_signature = candidate_by_signature[key]
        if base_for_signature == candidate_for_signature:
            continue
        magic, size = key
        changed_signatures.append(
            {
                "magic": magic,
                "size": size,
                "base_hashes": sorted(base_for_signature),
                "candidate_hashes": sorted(candidate_for_signature),
                "shared_hashes": sorted(base_for_signature & candidate_for_signature),
            }
        )

    base_roots = root_map(base)
    candidate_roots = root_map(candidate)
    root_records: list[dict[str, Any]] = []
    for index in sorted(set(base_roots) | set(candidate_roots)):
        left = base_roots.get(index)
        right = candidate_roots.get(index)
        root_records.append(
            {
                "data_tbl_index": index,
                "base_present": left is not None,
                "candidate_present": right is not None,
                "base_root_sha256": left.get("root_node_sha256") if left else None,
                "candidate_root_sha256": right.get("root_node_sha256") if right else None,
                "equal": bool(
                    left
                    and right
                    and left.get("root_node_sha256") == right.get("root_node_sha256")
                ),
            }
        )

    base_magic = magic_counts(base_nodes)
    candidate_magic = magic_counts(candidate_nodes)
    magic_deltas = []
    for magic in sorted(set(base_magic) | set(candidate_magic)):
        before = base_magic.get(magic, 0)
        after = candidate_magic.get(magic, 0)
        if before != after:
            magic_deltas.append(
                {
                    "magic": magic,
                    "base_unique_nodes": before,
                    "candidate_unique_nodes": after,
                    "delta": after - before,
                }
            )

    exact_equal = not base_only and not candidate_only and all(
        record["equal"] for record in root_records
    )
    report = {
        "schema": REPORT_SCHEMA,
        "base": str(base_path.resolve()),
        "candidate": str(candidate_path.resolve()),
        "classification": "equal" if exact_equal else "different",
        "stats": {
            "base_unique_nodes": len(base_nodes),
            "candidate_unique_nodes": len(candidate_nodes),
            "shared_unique_nodes": len(shared_hashes),
            "base_only_unique_nodes": len(base_only),
            "candidate_only_unique_nodes": len(candidate_only),
            "changed_shape_signature_count": len(changed_signatures),
        },
        "roots": root_records,
        "base_only_hashes": base_only,
        "candidate_only_hashes": candidate_only,
        "shared_hashes": shared_hashes,
        "changed_shape_signatures": changed_signatures,
        "magic_deltas": magic_deltas,
        "policy": {
            "hash_identity": "sha256",
            "shape_signature": ["magic", "size"],
            "shape_match_is_not_semantic_identity": True,
        },
    }
    return report


def fail(message: str) -> int:
    print(f"asset_closure_compare=fail reason={message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--fail-on-difference", action="store_true")
    args = parser.parse_args()
    try:
        report = compare(args.base, args.candidate)
    except (ComparisonError, OSError) as exc:
        return fail(str(exc))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
    print(
        "asset_closure_compare=pass "
        f"classification={report['classification']} "
        f"shared={report['stats']['shared_unique_nodes']} "
        f"base_only={report['stats']['base_only_unique_nodes']} "
        f"candidate_only={report['stats']['candidate_only_unique_nodes']}"
    )
    if args.fail_on_difference and report["classification"] != "equal":
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
