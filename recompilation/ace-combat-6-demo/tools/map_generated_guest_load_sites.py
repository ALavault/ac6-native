#!/usr/bin/env python3
"""Map qualified generated guest memory sites to PAL instruction bytes.

The generated C++ is an ephemeral, read-only input.  The legacy event-handle
log format remains supported, while the post-resume format is deliberately
strict: exactly one instruction handoff and exactly one access are required.
Both formats resolve ``function + generated_line`` through the current
``ppc_func_mapping.cpp`` table to the nearest generated instruction comment
and then read the corresponding PAL basefile bytes.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

EXPECTED_BASEFILE_SHA256 = (
    "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
)
EXPECTED_XEX_SHA256 = (
    "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
)
EXPECTED_XENONRECOMP_COMMIT = (
    "ddd128bcca99fe8bfbb99bea583c972351fa6ace"
)

LEGACY_ACCESS_RE = re.compile(
    r"^AC6_EVENT_HANDLE_READ address=(0x[0-9A-Fa-f]+) "
    r"value=(0x[0-9A-Fa-f]+) tick=(\d+) thread=(\d+) "
    r"lr=(0x[0-9A-Fa-f]+) function=(\S*) generated_line=(\d+)$"
)
HANDOFF_RE = re.compile(
    r"^AC6_POST_RESUME_INSTRUCTION_HANDOFF resume_pc=(0x[0-9A-Fa-f]{8}) "
    r"callsite=(0x[0-9A-Fa-f]{8}) tick=(\d+) thread=(\d+) "
    r"signal_handle=(0x[0-9A-Fa-f]{8}) wait_handle=(0x[0-9A-Fa-f]{8})$"
)
SCALAR_ACCESS_RE = re.compile(
    r"^AC6_POST_RESUME_ACCESS kind=(\S+) address=(0x[0-9A-Fa-f]{8}) "
    r"size=(\d+) value=(0x[0-9A-Fa-f]+) tick=(\d+) thread=(\d+) "
    r"lr=(0x[0-9A-Fa-f]{8}) function=(\S*) generated_line=(\d+)$"
)
BYTES_ACCESS_RE = re.compile(
    r"^AC6_POST_RESUME_ACCESS kind=(\S+) address=(0x[0-9A-Fa-f]{8}) "
    r"size=(\d+) bytes=([0-9A-Fa-f]+) tick=(\d+) thread=(\d+) "
    r"lr=(0x[0-9A-Fa-f]{8}) function=(\S*) generated_line=(\d+)$"
)
REFUSED_ACCESS_PREFIX = "AC6_POST_RESUME_ACCESS_REFUSED "
FUNCTION_RE = re.compile(
    r"PPC_FUNC_IMPL\((__imp__[A-Za-z_][A-Za-z0-9_]*)\)"
)
MAPPING_ENTRY_RE = re.compile(
    r"^\s*\{\s*(0x[0-9A-Fa-f]+)\s*,\s*"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\}\s*,?\s*$"
)
SITE_RE = re.compile(
    r"\bPPC_(LOAD|STORE)_U(8|16|32|64|128)\b|"
    r"\bPPC_MM_(LOAD|STORE)_U(8|16|32|64)\b"
)
VECTOR_LOAD_RE = re.compile(r"\bsimde_mm_load_(?:si|u)128\s*\(")
ATOMIC_SITE_RE = re.compile(r"\bPPC_(LWARX|LDARX|STWCX|STDCX)\s*\(")


class MappingError(RuntimeError):
    """Raised for any missing, ambiguous, malformed, or unqualified join."""


@dataclass(frozen=True)
class FunctionRecord:
    source: Path
    start: int
    end: int
    entry: int
    lines: list[str]


def load_generated_manifest(manifest: Path, xex_sha: str) -> tuple[Path, str]:
    """Require the manifest adjacent to this generated source directory."""
    if manifest.is_symlink() or not manifest.is_file():
        raise MappingError(f"generated manifest is missing or symlinked: {manifest}")
    try:
        document = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise MappingError(f"generated manifest is unreadable: {manifest}") from error
    if not isinstance(document, dict):
        raise MappingError("generated manifest is not an object")
    if document.get("schema") != "ac6-demo-codegen-manifest/v2":
        raise MappingError("generated manifest schema is stale or unknown")
    if document.get("target_id") != "ac6-demo-xbox360-pal":
        raise MappingError("generated manifest target is not the qualified AC6 PAL")
    if document.get("xex_sha256") != xex_sha:
        raise MappingError("generated manifest XEX identity mismatch")
    if document.get("xenonrecomp_commit") != EXPECTED_XENONRECOMP_COMMIT:
        raise MappingError("generated manifest XenonRecomp commit is stale")
    return manifest, sha256(manifest.read_bytes())


def load_function_mapping(generated: Path, basefile_size: int) -> dict[str, int]:
    """Read exact generated symbol aliases from the current mapping table."""
    mapping_source = generated / "ppc_func_mapping.cpp"
    if mapping_source.is_symlink() or not mapping_source.is_file():
        raise MappingError(f"current generated function mapping is missing: {mapping_source}")
    mapping: dict[str, int] = {}
    saw_entry = False
    for line_number, line in enumerate(
            mapping_source.read_text(encoding="utf-8").splitlines(), 1):
        if re.search(r"\{\s*0x", line):
            match = MAPPING_ENTRY_RE.fullmatch(line)
            if match is None:
                raise MappingError(
                    f"malformed generated function mapping at line {line_number}")
            address = int(match.group(1), 16)
            symbol = match.group(2)
            if address < 0x82000000 or address & 3:
                raise MappingError(f"unaligned/out-of-range mapping address: {address:#x}")
            offset = address - 0x82000000
            if offset + 4 > basefile_size:
                raise MappingError(f"mapping address outside PAL basefile: {address:#x}")
            if symbol in mapping:
                raise MappingError(f"ambiguous generated mapping alias: {symbol}")
            mapping[symbol] = address
            saw_entry = True
    if not saw_entry:
        raise MappingError(f"generated function mapping is empty: {mapping_source}")
    return mapping


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_functions(generated: Path, basefile_size: int | None = None) -> dict[str, FunctionRecord]:
    result: dict[str, FunctionRecord] = {}
    mapping = load_function_mapping(generated, basefile_size or 0x100000000)
    for source in sorted(generated.glob("ppc_recomp.*.cpp")):
        if source.is_symlink() or not source.is_file():
            raise MappingError(f"generated source is missing or symlinked: {source}")
        lines = source.read_text(encoding="utf-8").splitlines()
        starts: list[tuple[int, str, int]] = []
        for index, line in enumerate(lines):
            match = FUNCTION_RE.search(line)
            if match is not None:
                name = match.group(1)
                alias = name.removeprefix("__imp__")
                if alias not in mapping:
                    raise MappingError(
                        f"generated function alias is absent from mapping table: {name}")
                entry = mapping[alias]
                starts.append((index, name, entry))
        for position, (start, name, entry) in enumerate(starts):
            if name in result:
                raise MappingError(f"duplicate generated function: {name}")
            end = starts[position + 1][0] - 1 if position + 1 < len(starts) else len(lines) - 1
            result[name] = FunctionRecord(source, start, end, entry, lines)
    if not result:
        raise MappingError("no generated PPC functions found")
    return result


def _site_kind(source_line: str, expected_kind: str) -> None:
    if expected_kind in {"lwarx", "ldarx", "stwcx", "stdcx"}:
        matches = list(ATOMIC_SITE_RE.finditer(source_line))
        if len(matches) != 1 or matches[0].group(1).lower() != expected_kind:
            raise MappingError("ambiguous or mismatched generated atomic site")
        return
    matches = list(SITE_RE.finditer(source_line))
    if expected_kind == "load128" and VECTOR_LOAD_RE.search(source_line):
        if len(matches) != 0:
            raise MappingError("ambiguous vector/generated memory site")
        return
    if len(matches) != 1:
        raise MappingError("ambiguous or missing generated memory site")
    match = matches[0]
    operation = match.group(1) or match.group(3)
    width = match.group(2) or match.group(4)
    if operation is None or width is None:
        raise MappingError("malformed generated memory site")
    actual = operation.lower() + width
    if actual != expected_kind:
        raise MappingError(
            f"observed kind {expected_kind} is not generated site {actual}"
        )


def map_line(functions: dict[str, FunctionRecord], name: str,
             line_number: int, basefile: bytes, kind: str | None = None) -> dict[str, object]:
    try:
        record = functions[name]
    except KeyError as error:
        raise MappingError(f"function is not in generated sources: {name}") from error
    line_index = line_number - 1
    if line_index <= record.start or line_index > record.end:
        raise MappingError(f"line outside function source: {name}:{line_number}")
    source_line = record.lines[line_index]
    if kind is None:
        if "PPC_LOAD_U32" not in source_line:
            raise MappingError(f"observed site is not PPC_LOAD_U32: {name}:{line_number}")
    else:
        _site_kind(source_line, kind)

    comment: str | None = None
    guest_pc: int | None = None
    instruction_count = 0
    for index in range(record.start + 1, line_index + 1):
        candidate = record.lines[index]
        if re.match(r"^\s*//\s+", candidate):
            comment = candidate.strip()[3:]
            guest_pc = record.entry + instruction_count * 4
            instruction_count += 1
    if comment is None or guest_pc is None:
        raise MappingError(f"no instruction comment before site: {name}:{line_number}")
    offset = guest_pc - 0x82000000
    if guest_pc < 0x82000000 or guest_pc & 3 or offset + 4 > len(basefile):
        raise MappingError(f"mapped PC outside PAL basefile: 0x{guest_pc:08X}")
    return {
        "source": record.source.name,
        "function": name,
        "generated_line": line_number,
        "guest_pc": f"0x{guest_pc:08X}",
        "instruction_bytes": basefile[offset:offset + 4].hex(" "),
        "instruction": comment,
    }


def parse_legacy_log(log: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line_number, line in enumerate(log.read_text(encoding="utf-8").splitlines(), 1):
        match = LEGACY_ACCESS_RE.fullmatch(line)
        if match is None:
            if line.startswith("AC6_EVENT_HANDLE_READ "):
                raise MappingError(f"malformed consumer row at log line {line_number}")
            continue
        rows.append({
            "address": int(match.group(1), 16),
            "value": int(match.group(2), 16),
            "tick": int(match.group(3)),
            "thread": int(match.group(4)),
            "lr": int(match.group(5), 16),
            "function": match.group(6),
            "generated_line": int(match.group(7)),
        })
    if not rows:
        raise MappingError("consumer log contains no rows")
    return rows


def parse_post_resume_log(log: Path) -> tuple[dict[str, object], dict[str, object]]:
    handoffs: list[dict[str, object]] = []
    accesses: list[dict[str, object]] = []
    for line_number, line in enumerate(log.read_text(encoding="utf-8").splitlines(), 1):
        match = HANDOFF_RE.fullmatch(line)
        if match is not None:
            handoffs.append({
                "resume_pc": int(match.group(1), 16),
                "callsite": int(match.group(2), 16),
                "tick": int(match.group(3)),
                "thread": int(match.group(4)),
                "signal_handle": int(match.group(5), 16),
                "wait_handle": int(match.group(6), 16),
            })
            continue
        match = SCALAR_ACCESS_RE.fullmatch(line)
        if match is not None:
            accesses.append({
                "kind": match.group(1), "address": int(match.group(2), 16),
                "size": int(match.group(3)), "value": match.group(4),
                "tick": int(match.group(5)), "thread": int(match.group(6)),
                "lr": int(match.group(7), 16), "function": match.group(8),
                "generated_line": int(match.group(9)),
            })
            continue
        match = BYTES_ACCESS_RE.fullmatch(line)
        if match is not None:
            accesses.append({
                "kind": match.group(1), "address": int(match.group(2), 16),
                "size": int(match.group(3)), "bytes": match.group(4).upper(),
                "tick": int(match.group(5)), "thread": int(match.group(6)),
                "lr": int(match.group(7), 16), "function": match.group(8),
                "generated_line": int(match.group(9)),
            })
            continue
        if line.startswith("AC6_POST_RESUME_"):
            if line.startswith(REFUSED_ACCESS_PREFIX):
                raise MappingError("post-resume access route was explicitly refused")
            raise MappingError(f"malformed post-resume row at log line {line_number}")
    if len(handoffs) != 1:
        raise MappingError(f"expected exactly one instruction handoff, got {len(handoffs)}")
    if len(accesses) != 1:
        raise MappingError(f"expected exactly one memory access, got {len(accesses)}")
    handoff = handoffs[0]
    if handoff != {**handoff, "resume_pc": 0x821A69CC,
                   "callsite": 0x821A69C8, "thread": 1,
                   "signal_handle": 0xE0000048, "wait_handle": 0xE000004C}:
        raise MappingError("post-resume handoff does not match the qualified boundary")
    access = accesses[0]
    kind = str(access["kind"])
    size = int(access["size"])
    valid_kinds = {f"{op}{width}" for op in ("load", "store")
                   for width in (8, 16, 32, 64, 128)}
    if kind not in valid_kinds or int(access["thread"]) != 1:
        raise MappingError("post-resume access kind/thread is not qualified")
    if size != int(kind.removeprefix("load").removeprefix("store")) // 8:
        raise MappingError("post-resume access width disagrees with kind")
    if "value" in access and len(str(access["value"])) != 2 + size * 2:
        raise MappingError("scalar post-resume value is not fixed-width")
    if "bytes" in access and len(str(access["bytes"])) != size * 2:
        raise MappingError("vector post-resume bytes are not fixed-width")
    return handoff, access


def hex32(value: int) -> str:
    return f"0x{value:08X}"


def _target(base_sha: str, xex_sha: str) -> dict[str, str]:
    return {
        "id": "ac6-demo-xbox360-pal", "module": "Default.xex",
        "xex_sha256": xex_sha,
        "pal_basefile_sha256": base_sha,
        "architecture": "Xenon big-endian / Xenos",
    }


def build_legacy(args: argparse.Namespace, base_bytes: bytes,
                 base_sha: str, xex_sha: str, rows: list[dict[str, object]],
                 functions: dict[str, FunctionRecord], manifest: Path,
                 manifest_sha: str) -> dict[str, object]:
    sites: dict[tuple[str, int], dict[str, object]] = {}
    for row in rows:
        key = (str(row["function"]), int(row["generated_line"]))
        mapped = map_line(functions, key[0], key[1], base_bytes)
        current = sites.setdefault(key, {**mapped, "count": 0, "addresses": set(),
                                         "values": set(), "ticks": [],
                                         "threads": set(), "context_lrs": set()})
        current["count"] = int(current["count"]) + 1
        current["addresses"].add(int(row["address"]))
        current["values"].add(int(row["value"]))
        current["ticks"].append(int(row["tick"]))
        current["threads"].add(int(row["thread"]))
        current["context_lrs"].add(int(row["lr"]))
    encoded_sites = []
    for key in sorted(sites):
        site = sites[key]
        encoded_sites.append({
            **{name: site[name] for name in
               ("source", "function", "generated_line", "guest_pc",
                "instruction_bytes", "instruction", "count")},
            "addresses": sorted(hex32(x) for x in site["addresses"]),
            "values": sorted(hex32(x) for x in site["values"]),
            "ticks": [min(site["ticks"]), max(site["ticks"])],
            "threads": sorted(site["threads"]),
            "context_lrs": sorted(hex32(x) for x in site["context_lrs"]),
        })
    source_hashes = {}
    for source in sorted({args.generated_dir / str(site["source"])
                          for site in encoded_sites}):
        source_hashes[source.name] = sha256(source.read_bytes())
    return {
        "schema": "ac6-demo-generated-guest-load-map/v1",
        "target": _target(base_sha, xex_sha),
        "inputs": {"generated_dir": str(args.generated_dir),
                   "xex": str(args.xex), "xex_sha256": xex_sha,
                   "generated_manifest": str(manifest),
                   "generated_manifest_sha256": manifest_sha,
                   "generated_source_sha256": source_hashes,
                   "log": str(args.log),
                   "log_sha256": sha256(args.log.read_bytes())},
        "rows": len(rows), "mapped_rows": len(rows),
        "unique_sites": len(encoded_sites), "sites": encoded_sites,
        "policy": {"generated_cpp_modified": False,
                    "microcode_or_shader_tracked": False,
                    "retail_evidence_fused": False, "fail_closed": True},
    }


def build_post_resume(args: argparse.Namespace, base_bytes: bytes,
                      base_sha: str, xex_sha: str, handoff: dict[str, object],
                      access: dict[str, object],
                      functions: dict[str, FunctionRecord], manifest: Path,
                      manifest_sha: str) -> dict[str, object]:
    mapped = map_line(functions, str(access["function"]),
                      int(access["generated_line"]), base_bytes,
                      str(access["kind"]))
    source = args.generated_dir / str(mapped["source"])
    return {
        "schema": "ac6-demo-generated-guest-load-map/v1",
        "mode": "post_resume_one_shot",
        "target": _target(base_sha, xex_sha),
        "inputs": {"generated_dir": str(args.generated_dir),
                   "xex": str(args.xex), "xex_sha256": xex_sha,
                   "generated_manifest": str(manifest),
                   "generated_manifest_sha256": manifest_sha,
                   "generated_source_sha256": {source.name: sha256(source.read_bytes())},
                   "log": str(args.log),
                   "log_sha256": sha256(args.log.read_bytes())},
        "handoff": {**{key: (hex32(value) if key in
                              ("resume_pc", "callsite", "signal_handle", "wait_handle")
                              else value)
                       for key, value in handoff.items()},
                    "lr_is_not_pc": True},
        "access": {**access, **mapped,
                   "address": hex32(int(access["address"])),
                   "lr": hex32(int(access["lr"]))},
        "rows": 1, "mapped_rows": 1, "unique_sites": 1,
        "policy": {"generated_cpp_modified": False,
                    "exactly_one_instruction_handoff": True,
                    "exactly_one_memory_access": True,
                    "pal_bytes_checked": True,
                    "ambiguity_is_failure": True,
                    "fail_closed": True},
    }


def build(args: argparse.Namespace) -> dict[str, object]:
    xex = getattr(args, "xex", None)
    if not isinstance(xex, Path):
        raise MappingError("an explicit --xex path is required")
    try:
        xex_bytes = xex.read_bytes()
    except OSError as error:
        raise MappingError(f"qualified XEX cannot be read: {xex}") from error
    xex_sha = sha256(xex_bytes)
    if xex_sha != EXPECTED_XEX_SHA256:
        raise MappingError(f"XEX identity mismatch: {xex_sha}")
    base_bytes = args.basefile.read_bytes()
    base_sha = sha256(base_bytes)
    if base_sha != EXPECTED_BASEFILE_SHA256:
        raise MappingError(f"PAL basefile identity mismatch: {base_sha}")
    log_text = args.log.read_text(encoding="utf-8")
    manifest = getattr(args, "manifest", None)
    if manifest is None:
        manifest = args.generated_dir.parent / "manifest.json"
    expected_manifest = args.generated_dir.parent / "manifest.json"
    if manifest.resolve() != expected_manifest.resolve():
        raise MappingError("generated manifest is not adjacent to current generated sources")
    manifest, manifest_sha = load_generated_manifest(manifest, xex_sha)
    functions = load_functions(args.generated_dir, len(base_bytes))
    if "AC6_POST_RESUME_" in log_text:
        handoff, access = parse_post_resume_log(args.log)
        return build_post_resume(args, base_bytes, base_sha, xex_sha, handoff,
                                 access, functions, manifest, manifest_sha)
    return build_legacy(args, base_bytes, base_sha,
                        xex_sha, parse_legacy_log(args.log), functions,
                        manifest, manifest_sha)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generated-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--basefile", type=Path, required=True)
    parser.add_argument("--xex", type=Path, required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = build(args)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps({"schema": result["schema"], "rows": result["rows"],
                      "mapped_rows": result["mapped_rows"],
                      "unique_sites": result["unique_sites"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except MappingError as error:
        raise SystemExit(f"mapping_error: {error}")
