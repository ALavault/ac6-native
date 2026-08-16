#!/usr/bin/env python3
"""Build the exhaustive, deterministic AC6 demo static-decomp atlas.

The chunk manifest and .pdata bytes are the only accepted boundary sources.
Optional semantic records come from the canonical Ghidra exporter and can
enrich records, but can never add or resize a function.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import subprocess
import tempfile
import tomllib
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
EXPECTED_XEX = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
TARGET_ID = "ac6-demo-xbox360-pal"
TEXT_START, TEXT_SIZE = 0x82090000, 0x2E67C4
PDATA_START, PDATA_SIZE = 0x82077200, 0x10438
PDATA_SHA256 = "82c68b78f3256dd0c2bdd0df40e97daf6f3cf6dd1e162d5ddee4a47d1d14e50b"


class AtlasError(RuntimeError):
    pass


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_digest(path: Path) -> str:
    return digest(path.read_bytes())


def number(value: object) -> int:
    return int(value, 0) if isinstance(value, str) else int(value)


def hx(value: int) -> str:
    return f"0x{value:08X}"


def canonical(document: object) -> bytes:
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode()


def qualify_manifest(path: Path) -> dict[str, object]:
    document = json.loads(path.read_text())
    required = {
        "schema": "ac6-demo-ghidra-chunks.v2", "target_id": TARGET_ID,
        "xex_sha256": EXPECTED_XEX, "project": "ace-combat-6-demo",
        "project_path": "ghidra-projects/ace-combat-6-demo",
        "program": "Default.xex", "module": "Default.xex",
        "language": "PowerPC:BE:64:Xenon",
    }
    for key, expected in required.items():
        if document.get(key) != expected:
            raise AtlasError(f"unqualified manifest {key}")
    if document.get("ghidra", {}).get("version") != "12.1.2":
        raise AtlasError("unqualified Ghidra version")
    for name, address, size, sha in (
        ("text", TEXT_START, TEXT_SIZE, None),
        ("pdata", PDATA_START, PDATA_SIZE, PDATA_SHA256),
    ):
        record = document.get(name, {})
        if number(record.get("address", -1)) != address or number(record.get("size", -1)) != size:
            raise AtlasError(f"unqualified {name} range")
        if sha is not None and record.get("byte_sha256") != sha:
            raise AtlasError(f"unqualified {name} hash")
    return document


def extract_basefile(xex: Path, xex1tool: Path) -> bytes:
    if digest(xex.read_bytes()) != EXPECTED_XEX:
        raise AtlasError("Default.xex identity mismatch")
    tmp_root = os.environ.get("TMPDIR")
    if tmp_root != "/fastdata/lavaulta/tmp":
        raise AtlasError("TMPDIR must be /fastdata/lavaulta/tmp")
    Path(tmp_root).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ac6-demo-atlas-", dir=tmp_root) as directory:
        output = Path(directory) / "basefile.bin"
        completed = subprocess.run([str(xex1tool), "-b", str(output), str(xex)],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, check=False)
        if completed.returncode != 0 or not output.is_file():
            raise AtlasError("xex1tool basefile extraction failed")
        return output.read_bytes()


def pdata_records(basefile: bytes) -> list[tuple[int, int, str]]:
    offset = PDATA_START - 0x82000000
    pdata = basefile[offset:offset + PDATA_SIZE]
    if len(pdata) != PDATA_SIZE or digest(pdata) != PDATA_SHA256:
        raise AtlasError(".pdata identity mismatch")
    result: dict[int, tuple[int, int, str]] = {}
    for row in range(0, len(pdata), 8):
        entry, packed = struct.unpack_from(">II", pdata, row)
        size = ((packed >> 8) & 0x3FFFFF) * 4
        if entry and size:
            start = entry - 0x82000000
            body = basefile[start:start + size]
            if len(body) != size:
                raise AtlasError(f"function outside basefile: {hx(entry)}")
            result[entry] = (size, size, digest(body))
    if len(result) != 8327:
        raise AtlasError(f"unexpected .pdata function count: {len(result)}")
    return [(entry, *result[entry]) for entry in sorted(result)]


def configured_records(basefile: bytes) -> list[tuple[int, int, str]]:
    with (PROJECT / "config/confirmed-chunks.toml").open("rb") as stream:
        rows = tomllib.load(stream).get("function", [])
    result = []
    for row in rows:
        entry, size = int(row["address"]), int(row["size"])
        body = basefile[entry - 0x82000000:entry - 0x82000000 + size]
        actual = digest(body)
        if actual != row["byte_sha256"]:
            raise AtlasError(f"confirmed range hash mismatch: {hx(entry)}")
        result.append((entry, size, actual))
    if len(result) != 154:
        raise AtlasError(f"unexpected configured record count: {len(result)}")
    return result


def configured_data_records(basefile: bytes) -> list[dict[str, str]]:
    with (PROJECT / "config/confirmed-data.toml").open("rb") as stream:
        rows = tomllib.load(stream).get("range", [])
    result = []
    for row in rows:
        address, size = int(row["address"]), int(row["size"])
        body = basefile[address - 0x82000000:address - 0x82000000 + size]
        actual = digest(body)
        if actual != row["byte_sha256"]:
            raise AtlasError(f"confirmed data hash mismatch: {hx(address)}")
        result.append({"address": hx(address), "size": hx(size),
                       "byte_sha256": actual})
    return result


def semantic_records(path: Path | None) -> dict[int, dict[str, object]]:
    if path is None:
        return {}
    document = json.loads(path.read_text())
    if document.get("schema") != "ac6-demo-static-semantics.export/v1":
        raise AtlasError("unqualified semantic export schema")
    if document.get("xex_sha256") != EXPECTED_XEX or document.get("project") != "ace-combat-6-demo":
        raise AtlasError("unqualified semantic export identity")
    return {number(row["entry"]): row for row in document.get("functions", [])}


def rtti_records(path: Path | None) -> tuple[list[dict[str, object]], list[str]]:
    if path is None:
        return [], []
    document = json.loads(path.read_text())
    required = {"schema": "ac6-demo-rtti-atlas.export/v1",
                "target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX,
                "project": "ace-combat-6-demo",
                "language": "PowerPC:BE:64:Xenon",
                "type_descriptor_count": 772}
    for key, expected in required.items():
        if document.get(key) != expected:
            raise AtlasError(f"unqualified RTTI export {key}")
    tables = document.get("vtables")
    if not isinstance(tables, list) or len(tables) != 801:
        raise AtlasError("RTTI export must contain exactly 801 vtables")
    return tables, list(document.get("rejections", []))


def indirect_records(path: Path | None) -> list[dict[str, object]]:
    if path is None:
        return []
    document = json.loads(path.read_text())
    required = {"schema": "ac6-demo-indirect-sites.export/v1",
                "target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX,
                "project": "ace-combat-6-demo",
                "language": "PowerPC:BE:64:Xenon"}
    for key, expected in required.items():
        if document.get(key) != expected:
            raise AtlasError(f"unqualified indirect export {key}")
    sites = document.get("sites")
    if not isinstance(sites, list):
        raise AtlasError("indirect export sites must be an array")
    return sites


def static_references(path: Path | None) -> list[dict[str, object]]:
    if path is None:
        return []
    document = json.loads(path.read_text())
    required = {"schema": "ac6-demo-static-references.export/v1",
                "target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX,
                "project": "ace-combat-6-demo", "language": "PowerPC:BE:64:Xenon"}
    for key, expected in required.items():
        if document.get(key) != expected:
            raise AtlasError(f"unqualified reference export {key}")
    references = document.get("references")
    if not isinstance(references, list):
        raise AtlasError("reference export references must be an array")
    return references


def import_thunks(path: Path | None) -> dict[str, str]:
    if path is None:
        return {}
    document = json.loads(path.read_text())
    required = {"schema": "ac6-demo-import-thunks/v1",
                "target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX}
    for key, expected in required.items():
        if document.get(key) != expected:
            raise AtlasError(f"unqualified import map {key}")
    records = document.get("imports")
    if not isinstance(records, list) or len(records) != 228:
        raise AtlasError("import map must contain exactly 228 callable records")
    result = {}
    for row in records:
        address = hx(number(row["address"]))
        result[address] = f"{row['module']}:{row['name']}:{row['ordinal']}"
    return result


def build(args: argparse.Namespace) -> dict[str, object]:
    manifest = qualify_manifest(args.manifest)
    basefile = extract_basefile(args.xex, args.xex1tool)
    records: dict[int, tuple[int, str, str]] = {}
    for entry, size, _, sha in pdata_records(basefile):
        records[entry] = (size, sha, "pdata")
    configured = configured_records(basefile)
    configured_entries = {entry for entry, _, _ in configured}
    for row in manifest["chunks"]:
        entry, size = number(row["address"]), number(row["size"])
        if entry in configured_entries or any(owner <= entry and entry + size <= owner + owner_size for owner, owner_size, _ in configured):
            continue
        records[entry] = (size, row["byte_sha256"], "ghidra-chunk")
    for entry, size, sha in configured:
        records[entry] = (size, sha, "confirmed-range")
    if len(records) != 12876:
        raise AtlasError(f"unexpected exhaustive function count: {len(records)}")
    semantics = semantic_records(args.semantics)
    vtables, rtti_rejections = rtti_records(args.rtti)
    indirect_sites = indirect_records(args.indirect)
    references = static_references(args.references)
    imports_by_address = import_thunks(args.imports)
    function_addresses = {hx(entry) for entry in records}
    for table in vtables:
        for slot in table.get("slots", []):
            target = str(slot["target"])
            if target in imports_by_address:
                slot["target_kind"] = "import"
                slot["import"] = imports_by_address[target]
            elif target in function_addresses:
                slot["target_kind"] = "function"
            else:
                slot["target_kind"] = "unresolved"
    unknown = {"symbol": None, "decompilation": {"status": "unavailable", "pseudocode_sha256": None}, "direct_calls": [], "imports": [], "globals": [], "strings": [], "rtti_vtables": [], "role": "unknown", "confidence": "unknown"}
    references_by_entry: dict[int, dict[str, set[str]]] = {}
    vtables_by_target: dict[str, set[str]] = {}
    for table in vtables:
        table_address = str(table["address"])
        for slot in table.get("slots", []):
            target = str(slot["target"])
            vtables_by_target.setdefault(target, set()).add(table_address)
    owners_by_word: dict[int, int] = {}
    # Small independently confirmed inner entries own their words before an
    # overlapping parent.  This builds a bounded O(.text words) join table.
    for entry, (size, _, _) in sorted(records.items(), key=lambda item: item[1][0]):
        for address in range(entry, entry + size, 4):
            owners_by_word.setdefault(address, entry)
    for reference in references:
        source = number(reference["source"])
        owner = owners_by_word.get(source & ~3)
        if owner is None:
            continue
        kind = reference.get("kind")
        target_key = {"global": "globals", "string": "strings", "import": "imports"}.get(kind)
        if target_key is None:
            raise AtlasError(f"unknown static reference kind: {kind}")
        references_by_entry.setdefault(owner, {"globals": set(), "strings": set(), "imports": set()})[target_key].add(str(reference["value"]))
    functions = []
    for entry, (size, sha, provenance) in sorted(records.items()):
        semantic = dict(unknown)
        if entry in semantics:
            candidate = semantics[entry]
            # A stale Ghidra body may be larger than the independently proven
            # .pdata/codegen boundary.  It is never allowed to resize or taint
            # the atlas record; only exact boundary/hash matches are admitted.
            if (number(candidate.get("size", -1)) == size and
                    candidate.get("byte_sha256") == sha):
                semantic.update({key: candidate[key] for key in unknown
                                 if key in candidate})
        if entry in references_by_entry:
            for key in ("globals", "strings", "imports"):
                semantic[key] = sorted(references_by_entry[entry][key])
        if imports_by_address:
            imported = [imports_by_address[target] for target in semantic["direct_calls"]
                        if target in imports_by_address]
            semantic["imports"] = sorted(set(semantic["imports"] + imported))
        semantic["rtti_vtables"] = sorted(vtables_by_target.get(hx(entry), set()))
        functions.append({"entry": hx(entry), "ranges": [[hx(entry), hx(entry + size - 1)]], "byte_sha256": sha, "boundary_provenance": provenance, **semantic})
    data_by_address = {number(row["address"]): {"address": hx(number(row["address"])), "size": hx(number(row["size"])), "byte_sha256": row["byte_sha256"]} for row in manifest["data_ranges"]}
    for row in configured_data_records(basefile):
        data_by_address[number(row["address"])] = row
    data_ranges = [data_by_address[address] for address in sorted(data_by_address)]
    # Function ranges may overlap only for independently confirmed inner entries;
    # coverage therefore uses a byte union rather than a sum.
    intervals = [(entry, entry + size) for entry, (size, _, _) in records.items()] + [(number(row["address"]), number(row["address"]) + number(row["size"])) for row in data_ranges]
    cursor = TEXT_START
    classified = 0
    for start, end in sorted(intervals):
        if end <= cursor or start >= TEXT_START + TEXT_SIZE:
            continue
        if end > cursor:
            classified += end - max(start, cursor)
            cursor = end
    provenance_paths = {
        "tools/build_static_decomp_atlas.py": Path(__file__).resolve(),
        "tools/build_import_thunk_map.py": PROJECT / "tools/build_import_thunk_map.py",
        "tools/ghidra/ExportDemoStaticSemantics.java": PROJECT / "tools/ghidra/ExportDemoStaticSemantics.java",
        "tools/ghidra/ExportDemoRttiAtlas.java": PROJECT / "tools/ghidra/ExportDemoRttiAtlas.java",
        "tools/ghidra/ExportDemoIndirectSites.java": PROJECT / "tools/ghidra/ExportDemoIndirectSites.java",
        "tools/ghidra/ExportDemoStaticReferences.java": PROJECT / "tools/ghidra/ExportDemoStaticReferences.java",
        "config/confirmed-chunks.toml": PROJECT / "config/confirmed-chunks.toml",
        "config/confirmed-data.toml": PROJECT / "config/confirmed-data.toml",
        "input/ghidra-manifest.json": args.manifest,
    }
    for key, path in (("input/semantics.json", args.semantics),
                      ("input/rtti.json", args.rtti),
                      ("input/indirect.json", args.indirect),
                      ("input/references.json", args.references),
                      ("input/import-thunks.json", args.imports)):
        if path is not None:
            provenance_paths[key] = path
    return {
        "schema": "ac6-demo-static-decomp-atlas/v1",
        "identity": {"target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX, "project": "ace-combat-6-demo", "project_path": "ghidra-projects/ace-combat-6-demo", "program": "Default.xex", "module": "Default.xex", "language": "PowerPC:BE:64:Xenon", "ghidra_version": "12.1.2"},
        "provenance": {key: file_digest(path) for key, path in sorted(provenance_paths.items())},
        "sections": {name: {"address": hx(number(manifest[name]["address"])), "size": hx(number(manifest[name]["size"])), "byte_sha256": manifest[name]["byte_sha256"]} for name in ("text", "pdata")},
        "coverage": {"function_count": len(functions), "pdata_functions": 8327, "ghidra_chunks": 4431, "confirmed_functions": 118, "classified_text_bytes": classified, "text_bytes": TEXT_SIZE, "complete": classified == TEXT_SIZE},
        "functions": functions, "data_ranges": data_ranges,
        "rejections": sorted(list(manifest["skipped_chunks"]) + rtti_rejections),
        "vtables": vtables, "indirect_sites": indirect_sites}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xex", type=Path, required=True)
    parser.add_argument("--xex1tool", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--semantics", type=Path)
    parser.add_argument("--rtti", type=Path)
    parser.add_argument("--indirect", type=Path)
    parser.add_argument("--references", type=Path)
    parser.add_argument("--imports", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    document = build(args)
    payload = canonical(document)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_name(args.output.name + ".new")
    if args.output.exists() or temporary.exists():
        raise AtlasError("refusing atlas output collision")
    temporary.write_bytes(payload)
    temporary.replace(args.output)
    print(f"AC6_DEMO_STATIC_ATLAS_PASS functions={len(document['functions'])} complete={str(document['coverage']['complete']).lower()} sha256={digest(payload)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
