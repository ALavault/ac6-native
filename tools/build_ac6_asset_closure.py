#!/usr/bin/env python3
"""Build a deterministic content-addressed graph from AC6 extraction manifests.

The input manifests are produced by ``extract_ac6_pac.py``. Payload bytes stay
outside the repository; this tool verifies every referenced payload by size and
SHA-256, recursively walks bounded FHM containers, and emits only metadata.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ac6_fhm import parse_fhm


SCHEMA = "ac6.asset-closure.v1"
MAX_DEFAULT_DEPTH = 32
MAX_DEFAULT_OCCURRENCES = 1_000_000


class ClosureError(ValueError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def magic_of(data: bytes) -> str:
    return data[:4].decode("latin-1", errors="replace") if data else ""


def display_magic(value: str) -> str:
    return "".join(ch if 32 <= ord(ch) < 127 else f"\\x{ord(ch):02x}" for ch in value)


@dataclass(frozen=True)
class RootPayload:
    manifest_path: Path
    manifest_sha256: str
    data_tbl_sha256: str
    entry_index: int
    payload_path: Path
    payload_size: int
    payload_sha256: str
    archive: str
    source_offset: int
    stored_size: int
    expanded_size: int
    decode_status: str
    decode_codec: str | None

    @property
    def root_name(self) -> str:
        return f"{self.data_tbl_sha256[:12]}-entry-{self.entry_index:04d}"


def require_dict(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ClosureError(f"{label} must be an object")
    return value


def require_int(value: Any, label: str, minimum: int = 0) -> int:
    if type(value) is not int or value < minimum:
        raise ClosureError(f"{label} must be an integer >= {minimum}")
    return value


def require_str(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise ClosureError(f"{label} must be a non-empty string")
    return value


def load_roots(manifest_paths: list[Path]) -> list[RootPayload]:
    roots: list[RootPayload] = []
    seen: set[tuple[str, int]] = set()
    for manifest_path in manifest_paths:
        resolved = manifest_path.resolve()
        try:
            raw = resolved.read_bytes()
            document = json.loads(raw)
        except (OSError, json.JSONDecodeError) as exc:
            raise ClosureError(f"cannot read manifest {resolved}: {exc}") from exc
        if document.get("schema_version") != 1:
            raise ClosureError(f"unsupported manifest schema in {resolved}")
        data_tbl = require_dict(document.get("data_tbl"), f"{resolved}:data_tbl")
        data_tbl_sha256 = require_str(data_tbl.get("sha256"), f"{resolved}:data_tbl.sha256")
        entries = document.get("entries")
        if not isinstance(entries, list) or not entries:
            raise ClosureError(f"{resolved}:entries must be a non-empty list")
        manifest_digest = hashlib.sha256(raw).hexdigest()
        for entry in entries:
            record = require_dict(entry, f"{resolved}:entry")
            index = require_int(record.get("index"), f"{resolved}:entry.index")
            key = (data_tbl_sha256, index)
            if key in seen:
                raise ClosureError(
                    f"duplicate DATA.TBL root: sha256={data_tbl_sha256} index={index}"
                )
            seen.add(key)
            payload = require_dict(record.get("payload"), f"{resolved}:entry[{index}].payload")
            payload_size = require_int(
                payload.get("size"), f"{resolved}:entry[{index}].payload.size"
            )
            payload_sha256 = require_str(
                payload.get("sha256"), f"{resolved}:entry[{index}].payload.sha256"
            )
            relative_payload = Path(
                require_str(
                    record.get("payload_path"),
                    f"{resolved}:entry[{index}].payload_path",
                )
            )
            if relative_payload.is_absolute() or ".." in relative_payload.parts:
                raise ClosureError(f"unsafe payload path for entry {index}: {relative_payload}")
            payload_path = (resolved.parent / relative_payload).resolve()
            try:
                actual_size = payload_path.stat().st_size
            except OSError as exc:
                raise ClosureError(f"missing payload for entry {index}: {payload_path}") from exc
            if actual_size != payload_size:
                raise ClosureError(
                    f"payload size mismatch for entry {index}: {actual_size} != {payload_size}"
                )
            actual_hash = sha256_path(payload_path)
            if actual_hash != payload_sha256:
                raise ClosureError(
                    f"payload hash mismatch for entry {index}: {actual_hash} != {payload_sha256}"
                )
            decode = require_dict(record.get("decode"), f"{resolved}:entry[{index}].decode")
            roots.append(
                RootPayload(
                    manifest_path=resolved,
                    manifest_sha256=manifest_digest,
                    data_tbl_sha256=data_tbl_sha256,
                    entry_index=index,
                    payload_path=payload_path,
                    payload_size=payload_size,
                    payload_sha256=payload_sha256,
                    archive=require_str(record.get("pac_name"), f"entry[{index}].pac_name"),
                    source_offset=require_int(record.get("offset"), f"entry[{index}].offset"),
                    stored_size=require_int(
                        record.get("stored_size"), f"entry[{index}].stored_size"
                    ),
                    expanded_size=require_int(
                        record.get("expanded_size"), f"entry[{index}].expanded_size"
                    ),
                    decode_status=require_str(
                        decode.get("status"), f"entry[{index}].decode.status"
                    ),
                    decode_codec=decode.get("codec")
                    if isinstance(decode.get("codec"), str)
                    else None,
                )
            )
    return sorted(roots, key=lambda root: (root.data_tbl_sha256, root.entry_index))


class ClosureBuilder:
    def __init__(
        self,
        *,
        max_depth: int,
        max_occurrences: int,
        allow_parser_notes: bool,
    ) -> None:
        self.max_depth = max_depth
        self.max_occurrences = max_occurrences
        self.allow_parser_notes = allow_parser_notes
        self.nodes: dict[str, dict[str, Any]] = {}
        self.occurrences: list[dict[str, Any]] = []
        self.magic_counts: Counter[str] = Counter()
        self.root_magic_counts: dict[str, Counter[str]] = defaultdict(Counter)
        self.parser_note_count = 0
        self.occurrence_count = 0

    def _register_node(self, data: bytes) -> tuple[str, dict[str, Any], bool]:
        digest = sha256_bytes(data)
        existing = self.nodes.get(digest)
        if existing is not None:
            if existing["size"] != len(data) or existing["magic"] != magic_of(data):
                raise ClosureError(f"SHA-256 identity collision for {digest}")
            return digest, existing, False
        record: dict[str, Any] = {
            "sha256": digest,
            "size": len(data),
            "magic": magic_of(data),
            "magic_display": display_magic(magic_of(data)),
            "kind": "fhm" if data[:4] == b"FHM " else "leaf",
            "children": [],
            "occurrence_count": 0,
            "root_entries": [],
        }
        self.nodes[digest] = record
        return digest, record, True

    def visit(
        self,
        data: bytes,
        *,
        root: RootPayload,
        path: str,
        depth: int,
        parent_sha256: str | None,
        child_index: int | None,
        offset: int,
        declared_size: int,
        notes: list[str],
        ancestry: tuple[str, ...],
    ) -> str:
        if self.occurrence_count >= self.max_occurrences:
            raise ClosureError("FHM occurrence limit exceeded")
        if depth > self.max_depth:
            raise ClosureError(f"FHM depth limit exceeded at {path}")
        digest, node, is_new = self._register_node(data)
        if digest in ancestry:
            raise ClosureError(f"recursive FHM cycle detected at {path}: {digest}")
        if notes:
            self.parser_note_count += len(notes)
            if not self.allow_parser_notes:
                raise ClosureError(f"parser note at {path}: {'; '.join(notes)}")
        self.occurrence_count += 1
        self.magic_counts[node["magic"]] += 1
        self.root_magic_counts[root.root_name][node["magic"]] += 1
        occurrence = {
            "root": root.root_name,
            "data_tbl_index": root.entry_index,
            "path": path,
            "depth": depth,
            "sha256": digest,
            "magic": node["magic"],
            "size": len(data),
            "parent_sha256": parent_sha256,
            "child_index": child_index,
            "offset": offset,
            "declared_size": declared_size,
            "notes": notes,
        }
        self.occurrences.append(occurrence)
        node["occurrence_count"] += 1
        if root.entry_index not in node["root_entries"]:
            node["root_entries"].append(root.entry_index)

        if data[:4] != b"FHM ":
            return digest
        children = parse_fhm(data)
        if children is None:
            raise ClosureError(f"invalid FHM container at {path}")
        edge_records: list[dict[str, Any]] = []
        for child in children:
            child_path = f"{path}/{child.index:04d}"
            child_digest = self.visit(
                child.data,
                root=root,
                path=child_path,
                depth=depth + 1,
                parent_sha256=digest,
                child_index=child.index,
                offset=child.offset,
                declared_size=child.declared_size,
                notes=list(child.notes),
                ancestry=ancestry + (digest,),
            )
            edge_records.append(
                {
                    "index": child.index,
                    "offset": child.offset,
                    "declared_size": child.declared_size,
                    "actual_size": child.size,
                    "sha256": child_digest,
                    "magic": child.magic,
                    "notes": list(child.notes),
                }
            )
        if is_new:
            node["children"] = edge_records
        elif node["children"] != edge_records:
            raise ClosureError(f"non-deterministic parse for node {digest}")
        return digest


def write_occurrences(path: Path, occurrences: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(
            [
                "root",
                "data_tbl_index",
                "path",
                "depth",
                "sha256",
                "magic_hex",
                "magic_display",
                "size",
                "parent_sha256",
                "child_index",
                "offset",
                "declared_size",
                "notes",
            ]
        )
        for record in occurrences:
            magic = record["magic"]
            writer.writerow(
                [
                    record["root"],
                    record["data_tbl_index"],
                    record["path"],
                    record["depth"],
                    record["sha256"],
                    magic.encode("latin-1", errors="replace").hex(),
                    display_magic(magic),
                    record["size"],
                    record["parent_sha256"] or "-",
                    record["child_index"] if record["child_index"] is not None else "-",
                    record["offset"],
                    record["declared_size"],
                    " | ".join(record["notes"]) if record["notes"] else "-",
                ]
            )


def write_shared_nodes(path: Path, nodes: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(
            [
                "sha256",
                "magic_hex",
                "magic_display",
                "size",
                "occurrence_count",
                "root_entries",
            ]
        )
        for node in nodes:
            magic = node["magic"]
            writer.writerow(
                [
                    node["sha256"],
                    magic.encode("latin-1", errors="replace").hex(),
                    node["magic_display"],
                    node["size"],
                    node["occurrence_count"],
                    ",".join(str(value) for value in node["root_entries"]),
                ]
            )


def build_closure(
    manifest_paths: list[Path],
    output_dir: Path,
    *,
    max_depth: int = MAX_DEFAULT_DEPTH,
    max_occurrences: int = MAX_DEFAULT_OCCURRENCES,
    allow_parser_notes: bool = False,
) -> dict[str, Any]:
    if max_depth < 0:
        raise ClosureError("max_depth must be non-negative")
    if max_occurrences <= 0:
        raise ClosureError("max_occurrences must be positive")
    roots = load_roots(manifest_paths)
    if not roots:
        raise ClosureError("no payload roots")
    builder = ClosureBuilder(
        max_depth=max_depth,
        max_occurrences=max_occurrences,
        allow_parser_notes=allow_parser_notes,
    )
    root_records: list[dict[str, Any]] = []
    for root in roots:
        data = root.payload_path.read_bytes()
        root_digest = builder.visit(
            data,
            root=root,
            path=root.root_name,
            depth=0,
            parent_sha256=None,
            child_index=None,
            offset=0,
            declared_size=len(data),
            notes=[],
            ancestry=(),
        )
        root_records.append(
            {
                "name": root.root_name,
                "data_tbl_index": root.entry_index,
                "data_tbl_sha256": root.data_tbl_sha256,
                "archive": root.archive,
                "source_offset": root.source_offset,
                "stored_size": root.stored_size,
                "expanded_size": root.expanded_size,
                "decode_status": root.decode_status,
                "decode_codec": root.decode_codec,
                "payload_size": root.payload_size,
                "payload_sha256": root.payload_sha256,
                "root_node_sha256": root_digest,
                "manifest_path": str(root.manifest_path),
                "manifest_sha256": root.manifest_sha256,
                "magic_counts": dict(sorted(builder.root_magic_counts[root.root_name].items())),
            }
        )

    node_records = sorted(builder.nodes.values(), key=lambda node: node["sha256"])
    for node in node_records:
        node["root_entries"].sort()
    occurrences = sorted(
        builder.occurrences,
        key=lambda record: (record["data_tbl_index"], record["path"]),
    )
    shared_nodes = [node for node in node_records if node["occurrence_count"] > 1]
    cross_root_shared = [node for node in shared_nodes if len(node["root_entries"]) > 1]
    closure = {
        "schema": SCHEMA,
        "roots": root_records,
        "nodes": node_records,
        "stats": {
            "root_count": len(root_records),
            "unique_node_count": len(node_records),
            "occurrence_count": len(occurrences),
            "shared_node_count": len(shared_nodes),
            "cross_root_shared_node_count": len(cross_root_shared),
            "fhm_node_count": sum(node["kind"] == "fhm" for node in node_records),
            "leaf_node_count": sum(node["kind"] == "leaf" for node in node_records),
            "parser_note_count": builder.parser_note_count,
            "magic_occurrence_counts": dict(sorted(builder.magic_counts.items())),
        },
        "artifacts": {
            "occurrences_tsv": "occurrences.tsv",
            "shared_nodes_tsv": "shared_nodes.tsv",
        },
        "policy": {
            "payload_bytes_copied": False,
            "payload_size_and_sha256_verified": True,
            "content_addressed_by_sha256": True,
            "unknown_leaf_magic_preserved": True,
            "fhm_parse_fail_closed": True,
            "parser_notes_allowed": allow_parser_notes,
            "max_depth": max_depth,
            "max_occurrences": max_occurrences,
        },
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    write_occurrences(output_dir / "occurrences.tsv", occurrences)
    write_shared_nodes(output_dir / "shared_nodes.tsv", shared_nodes)
    (output_dir / "closure.json").write_text(
        json.dumps(closure, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return closure


def fail(message: str) -> int:
    print(f"asset_closure=fail reason={message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifests", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-depth", type=int, default=MAX_DEFAULT_DEPTH)
    parser.add_argument("--max-occurrences", type=int, default=MAX_DEFAULT_OCCURRENCES)
    parser.add_argument("--allow-parser-notes", action="store_true")
    args = parser.parse_args()
    try:
        closure = build_closure(
            args.manifests,
            args.output.resolve(),
            max_depth=args.max_depth,
            max_occurrences=args.max_occurrences,
            allow_parser_notes=args.allow_parser_notes,
        )
    except (ClosureError, OSError) as exc:
        return fail(str(exc))
    print(
        "asset_closure=pass "
        f"roots={closure['stats']['root_count']} "
        f"unique_nodes={closure['stats']['unique_node_count']} "
        f"occurrences={closure['stats']['occurrence_count']} "
        f"shared={closure['stats']['shared_node_count']} "
        f"output={args.output.resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
