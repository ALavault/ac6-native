#!/usr/bin/env python3
"""Map bounded payload writer traces to exact PAL guest store PCs.

The generated C++ is read-only ephemeral input.  The mapper checks the
qualified PAL basefile identity, requires a store macro at every observed
source line, and refuses malformed or ambiguous rows.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED_BASEFILE_SHA256 = (
    "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
)
SCALAR_RE = re.compile(
    r"^AC6_EVENT_HANDLE_PAYLOAD_WRITE address=(0x[0-9A-F]+) size=(\d+) "
    r"value=(0x[0-9A-F]+) tick=(\d+) thread=(\d+) lr=(0x[0-9A-F]+) "
    r"function=(\S*) generated_line=(\d+)$"
)
BYTES_RE = re.compile(
    r"^AC6_EVENT_HANDLE_PAYLOAD_WRITE address=(0x[0-9A-F]+) size=(\d+) "
    r"bytes=([0-9A-F]*) tick=(\d+) thread=(\d+) lr=(0x[0-9A-F]+) "
    r"function=(\S*) generated_line=(\d+)$"
)
FUNCTION_RE = re.compile(
    r"PPC_FUNC_IMPL\((__imp__(?:sub_[0-9A-Fa-f]+|_xstart))\)"
)
STORE_MACRO_RE = re.compile(r"PPC_STORE_U(8|16|32|64|128)")


class MappingError(RuntimeError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_functions(generated: Path):
    result = {}
    for source in sorted(generated.glob("ppc_recomp.*.cpp")):
        lines = source.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            match = FUNCTION_RE.search(line)
            if match is None:
                continue
            name = match.group(1)
            if name in result:
                raise MappingError(f"duplicate generated function: {name}")
            entry = 0x821A7160 if name == "__imp___xstart" else int(name.rsplit("_", 1)[1], 16)
            result[name] = (source, index, entry, lines)
    if not result:
        raise MappingError("no generated PPC functions found")
    return result


def map_line(functions, name: str, line_number: int, basefile: bytes):
    try:
        source, start, entry, lines = functions[name]
    except KeyError as error:
        raise MappingError(f"function is not in generated sources: {name}") from error
    if line_number < start + 1 or line_number > len(lines):
        raise MappingError(f"line outside function source: {name}:{line_number}")
    source_line = lines[line_number - 1]
    if STORE_MACRO_RE.search(source_line) is None:
        raise MappingError(f"observed site is not a PPC store: {name}:{line_number}")
    comment = None
    guest_pc = None
    instruction_count = 0
    for index in range(start + 1, line_number + 1):
        candidate = lines[index - 1]
        if re.match(r"^\s*//\s+", candidate):
            comment = candidate.strip()[3:]
            guest_pc = entry + instruction_count * 4
            instruction_count += 1
    if comment is None or guest_pc is None:
        raise MappingError(f"no instruction comment before site: {name}:{line_number}")
    offset = guest_pc - 0x82000000
    if offset < 0 or offset + 4 > len(basefile):
        raise MappingError(f"mapped PC outside PAL basefile: 0x{guest_pc:08X}")
    return {
        "source": source.name,
        "function": name,
        "generated_line": line_number,
        "guest_pc": f"0x{guest_pc:08X}",
        "instruction_bytes": basefile[offset:offset + 4].hex(" "),
        "instruction": comment,
    }


def parse_log(log: Path):
    rows = []
    for line_number, line in enumerate(log.read_text(encoding="utf-8").splitlines(), 1):
        match = SCALAR_RE.fullmatch(line)
        if match is not None:
            rows.append({
                "address": int(match.group(1), 16),
                "size": int(match.group(2)),
                "value": match.group(3),
                "tick": int(match.group(4)),
                "thread": int(match.group(5)),
                "lr": int(match.group(6), 16),
                "function": match.group(7),
                "generated_line": int(match.group(8)),
            })
            continue
        match = BYTES_RE.fullmatch(line)
        if match is not None:
            rows.append({
                "address": int(match.group(1), 16),
                "size": int(match.group(2)),
                "bytes": match.group(3),
                "tick": int(match.group(4)),
                "thread": int(match.group(5)),
                "lr": int(match.group(6), 16),
                "function": match.group(7),
                "generated_line": int(match.group(8)),
            })
            continue
        if line.startswith("AC6_EVENT_HANDLE_PAYLOAD_WRITE "):
            raise MappingError(f"malformed writer row at log line {line_number}")
    if not rows:
        raise MappingError("payload writer log contains no rows")
    return rows


def build(args: argparse.Namespace):
    base_bytes = args.basefile.read_bytes()
    base_sha = sha256(base_bytes)
    if base_sha != EXPECTED_BASEFILE_SHA256:
        raise MappingError(f"PAL basefile identity mismatch: {base_sha}")
    rows = parse_log(args.log)
    functions = load_functions(args.generated_dir)
    sites = {}
    for row in rows:
        key = (str(row["function"]), int(row["generated_line"]))
        mapped = map_line(functions, key[0], key[1], base_bytes)
        current = sites.setdefault(key, {**mapped, "count": 0, "addresses": set(),
                                         "sizes": set(), "values": set(),
                                         "bytes": set(), "ticks": [],
                                         "threads": set(), "context_lrs": set()})
        current["count"] += 1
        current["addresses"].add(row["address"])
        current["sizes"].add(row["size"])
        if "value" in row:
            current["values"].add(row["value"])
        if "bytes" in row:
            current["bytes"].add(row["bytes"])
        current["ticks"].append(row["tick"])
        current["threads"].add(row["thread"])
        current["context_lrs"].add(row["lr"])
    encoded_sites = []
    for key in sorted(sites):
        site = sites[key]
        encoded = {name: site[name] for name in
                   ("source", "function", "generated_line", "guest_pc",
                    "instruction_bytes", "instruction", "count")}
        encoded.update({
            "addresses": [f"0x{x:08X}" for x in sorted(site["addresses"])],
            "sizes": sorted(site["sizes"]),
            "values": sorted(site["values"]),
            "bytes": sorted(site["bytes"]),
            "ticks": [min(site["ticks"]), max(site["ticks"])],
            "threads": sorted(site["threads"]),
            "context_lrs": [f"0x{x:08X}" for x in sorted(site["context_lrs"])],
        })
        encoded_sites.append(encoded)
    source_hashes = {}
    for source in sorted({args.generated_dir / str(site["source"])
                          for site in encoded_sites}):
        source_hashes[source.name] = sha256(source.read_bytes())
    return {
        "schema": "ac6-demo-generated-guest-payload-writer-map/v1",
        "target": {
            "id": "ac6-demo-xbox360-pal",
            "module": "Default.xex",
            "xex_sha256": "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8",
            "pal_basefile_sha256": base_sha,
            "architecture": "Xenon big-endian / Xenos",
        },
        "inputs": {
            "generated_dir": str(args.generated_dir),
            "generated_source_sha256": source_hashes,
            "log": str(args.log),
            "log_sha256": sha256(args.log.read_bytes()),
        },
        "rows": len(rows),
        "mapped_rows": len(rows),
        "unique_sites": len(encoded_sites),
        "sites": encoded_sites,
        "policy": {
            "generated_cpp_modified": False,
            "microcode_or_shader_tracked": False,
            "retail_evidence_fused": False,
            "fail_closed": True,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generated-dir", type=Path, required=True)
    parser.add_argument("--basefile", type=Path, required=True)
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
