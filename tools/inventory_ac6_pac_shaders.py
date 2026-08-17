#!/usr/bin/env python3
"""Inventory NSXR microcodes in recursively decoded AC6 PAC FHM payloads."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
from collections import defaultdict
from pathlib import Path
from typing import Any

TOOLS = Path(__file__).resolve().parent
WORKSPACE = TOOLS.parent
RECOMP_TOOLS = WORKSPACE / "recompilation" / "ace-combat-6-demo" / "tools"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(RECOMP_TOOLS))

from ac6_fhm import parse_fhm
from build_ac6_asset_closure import load_roots
from inventory_demo_nsxr import IMAGE_BASE, inventory


SCHEMA = "ac6-demo-pac-shader-inventory/v1"
DEFAULT_TARGETS = (
    "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
    "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
    "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
    "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
)


class InventoryError(ValueError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def swap32(data: bytes) -> bytes:
    if len(data) % 4:
        raise InventoryError(f"microcode size is not dword-aligned: {len(data)}")
    return b"".join(data[offset : offset + 4][::-1] for offset in range(0, len(data), 4))


def scan_nsxr_leaf(data: bytes, *, entry_index: int, path: str) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    document = inventory(data)
    for wrapper in document["wrappers"]:
        wrapper_offset = int(wrapper["address"], 16) - IMAGE_BASE
        for container in wrapper["containers"]:
            container_offset = int(container["address"], 16) - IMAGE_BASE
            microcode_offset = int(container["microcode_offset"], 16)
            size = int(container["microcode_size"])
            begin = container_offset + microcode_offset
            end = begin + size
            if begin < 0 or end > len(data):
                raise InventoryError(
                    f"microcode outside NSXR leaf entry={entry_index} path={path}"
                )
            microcode = data[begin:end]
            raw_digest = sha256(microcode)
            if raw_digest != container["microcode_sha256"]:
                raise InventoryError(
                    f"microcode hash mismatch entry={entry_index} path={path}"
                )
            result.append(
                {
                    "entry_index": entry_index,
                    "path": path,
                    "wrapper_offset": wrapper_offset,
                    "container_offset": container_offset,
                    "microcode_offset": microcode_offset,
                    "stage": container["stage"],
                    "size": size,
                    "sha256_raw": raw_digest,
                    "sha256_swap32": sha256(swap32(microcode)),
                }
            )
    return result


def build_inventory(manifests: list[Path], targets: set[str]) -> dict[str, Any]:
    roots = load_roots(manifests)
    records: list[dict[str, Any]] = []
    nsxr_occurrences = 0
    fhm_occurrences = 0
    node_occurrences = 0

    def walk(data: bytes, *, entry_index: int, path: str, depth: int) -> None:
        nonlocal nsxr_occurrences, fhm_occurrences, node_occurrences
        node_occurrences += 1
        if node_occurrences > 1_000_000:
            raise InventoryError("FHM occurrence limit exceeded")
        if depth > 32:
            raise InventoryError(f"FHM depth limit exceeded at {path}")
        if data[:4] == b"NSXR":
            nsxr_occurrences += 1
            records.extend(scan_nsxr_leaf(data, entry_index=entry_index, path=path))
            return
        if data[:4] != b"FHM ":
            return
        fhm_occurrences += 1
        children = parse_fhm(data)
        if children is None:
            raise InventoryError(f"invalid FHM container at {path}")
        for child in children:
            if child.notes:
                raise InventoryError(f"FHM parser note at {path}/{child.index:04d}")
            walk(
                child.data,
                entry_index=entry_index,
                path=f"{path}/{child.index:04d}",
                depth=depth + 1,
            )

    for root in roots:
        walk(
            root.payload_path.read_bytes(),
            entry_index=root.entry_index,
            path=f"{root.entry_index:04d}",
            depth=0,
        )

    by_identity: dict[tuple[str, str, str, int], list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        key = (
            record["stage"],
            record["sha256_raw"],
            record["sha256_swap32"],
            record["size"],
        )
        by_identity[key].append(
            {
                "entry_index": record["entry_index"],
                "path": record["path"],
                "wrapper_offset": record["wrapper_offset"],
                "container_offset": record["container_offset"],
                "microcode_offset": record["microcode_offset"],
            }
        )
    microcodes = []
    for (stage, raw_digest, swapped_digest, size), occurrences in sorted(
        by_identity.items()
    ):
        matches = sorted(targets.intersection((raw_digest, swapped_digest)))
        microcodes.append(
            {
                "stage": stage,
                "size": size,
                "sha256_raw": raw_digest,
                "sha256_swap32": swapped_digest,
                "occurrence_count": len(occurrences),
                "occurrences": occurrences,
                "target_matches": matches,
            }
        )
    target_results = {
        target: {
            "found": any(target in item["target_matches"] for item in microcodes),
            "match_count": sum(
                item["occurrence_count"]
                for item in microcodes
                if target in item["target_matches"]
            ),
        }
        for target in sorted(targets)
    }
    return {
        "schema": SCHEMA,
        "policy": {
            "metadata_only": True,
            "proprietary_bytes_published": False,
            "fhm_fail_closed": True,
            "byte_orders": ["raw", "dword_swap32"],
        },
        "roots": [
            {
                "data_tbl_sha256": root.data_tbl_sha256,
                "entry_index": root.entry_index,
                "archive": root.archive,
                "source_offset": root.source_offset,
                "stored_size": root.stored_size,
                "expanded_size": root.expanded_size,
                "payload_sha256": root.payload_sha256,
            }
            for root in roots
        ],
        "stats": {
            "root_count": len(roots),
            "node_occurrence_count": node_occurrences,
            "fhm_occurrence_count": fhm_occurrences,
            "nsxr_occurrence_count": nsxr_occurrences,
            "shader_occurrence_count": len(records),
            "unique_microcode_count": len(microcodes),
        },
        "targets": target_results,
        "microcodes": microcodes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifests", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target-sha256", action="append")
    args = parser.parse_args()
    targets = set(args.target_sha256 or DEFAULT_TARGETS)
    if any(len(target) != 64 for target in targets):
        parser.error("target SHA-256 must contain 64 hexadecimal characters")
    document = build_inventory(args.manifests, targets)
    encoded = (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        dir=args.output.parent, prefix=args.output.name + ".", delete=False
    ) as temporary:
        temporary.write(encoded)
        temporary.flush()
        os.fsync(temporary.fileno())
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, args.output)
    print(
        "pac_shader_inventory=pass "
        f"roots={document['stats']['root_count']} "
        f"nsxr={document['stats']['nsxr_occurrence_count']} "
        f"shaders={document['stats']['shader_occurrence_count']} "
        f"unique={document['stats']['unique_microcode_count']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
