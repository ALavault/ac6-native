#!/usr/bin/env python3
"""Generate an AC6_recomp config from a canonical Ghidra boundary export."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

SCHEMA = "ac6.ghidra-function-boundaries.v1"
DECISION_SCHEMA = "ac6.oracle-config-boundary-decisions.v1"
PROJECT = "ace-combat-6"
PROGRAM = "default.xex"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
LANGUAGE = "PowerPC:BE:64:A2ALT-32addr"
INTERCEPTION_SCHEMA = "ac6.oracle-host-interceptions.v1"
SWITCH_TABLE_SCHEMA = "ac6.oracle-switch-tables.v1"
FUNCTION_LINE = re.compile(
    rb'^(0x[0-9A-F]{8}) = \{ name = "rex_sub_([0-9A-F]{8})" \}\n$'
)
ADDRESS = re.compile(r"^0x[0-9A-F]{8}$")


class BoundaryError(ValueError):
    pass


@dataclass(frozen=True)
class FunctionBody:
    entry: int
    ranges: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class SwitchTable:
    address: int
    owner: int
    register: int
    labels: tuple[int, ...]
    evidence: str


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def parse_address(value: object) -> int:
    if not isinstance(value, str) or ADDRESS.fullmatch(value) is None:
        raise BoundaryError(f"invalid address: {value!r}")
    return int(value, 16)


def load_boundaries(document: object) -> list[FunctionBody]:
    if not isinstance(document, dict) or document.get("schema") != SCHEMA:
        raise BoundaryError("boundary schema")
    if document.get("project") != PROJECT or document.get("program") != PROGRAM:
        raise BoundaryError("canonical Ghidra identity")
    if document.get("sha256") != XEX_SHA256 or document.get("language") != LANGUAGE:
        raise BoundaryError("PAL XEX identity")
    records = document.get("functions")
    if not isinstance(records, list) or document.get("function_count") != len(records):
        raise BoundaryError("function count")

    functions: list[FunctionBody] = []
    entries: set[int] = set()
    occupied: list[tuple[int, int, int]] = []
    previous_entry = -1
    for record in records:
        if not isinstance(record, dict):
            raise BoundaryError("function record")
        entry = parse_address(record.get("entry"))
        if entry <= previous_entry or entry in entries:
            raise BoundaryError("function entries are not strictly ordered")
        previous_entry = entry
        entries.add(entry)
        raw_ranges = record.get("ranges")
        if not isinstance(raw_ranges, list) or not raw_ranges:
            raise BoundaryError(f"empty function body: 0x{entry:08X}")
        ranges: list[tuple[int, int]] = []
        for raw_range in raw_ranges:
            if not isinstance(raw_range, list) or len(raw_range) != 2:
                raise BoundaryError("function range")
            start, end = map(parse_address, raw_range)
            if start > end:
                raise BoundaryError("reversed function range")
            ranges.append((start, end))
            occupied.append((start, end, entry))
        if not any(start <= entry <= end for start, end in ranges):
            raise BoundaryError(f"entry outside function body: 0x{entry:08X}")
        functions.append(FunctionBody(entry, tuple(ranges)))

    occupied.sort()
    for left, right in zip(occupied, occupied[1:]):
        if right[0] <= left[1] and right[2] != left[2]:
            raise BoundaryError("overlapping canonical function bodies")
    return functions


def function_owners(functions: list[FunctionBody]) -> dict[int, int]:
    owners: dict[int, int] = {}
    for function in functions:
        for start, end in function.ranges:
            for candidate in range(start, end + 1, 4):
                owners[candidate] = function.entry
    return owners


def load_interceptions(document: object, functions: list[FunctionBody]) -> dict[int, dict[str, str]]:
    if not isinstance(document, dict) or document.get("schema") != INTERCEPTION_SCHEMA:
        raise BoundaryError("host interception schema")
    if document.get("project") != PROJECT or document.get("program") != PROGRAM:
        raise BoundaryError("host interception Ghidra identity")
    if document.get("sha256") != XEX_SHA256:
        raise BoundaryError("host interception XEX identity")
    records = document.get("interceptions")
    if not isinstance(records, list) or not records:
        raise BoundaryError("host interception records")
    owners = function_owners(functions)
    result: dict[int, dict[str, str]] = {}
    for record in records:
        if not isinstance(record, dict) or set(record) != {"candidate", "owner", "symbol"}:
            raise BoundaryError("host interception record")
        candidate = parse_address(record["candidate"])
        owner = parse_address(record["owner"])
        symbol = record["symbol"]
        if not isinstance(symbol, str) or symbol != f"rex_sub_{candidate:08X}":
            raise BoundaryError("host interception symbol")
        if candidate == owner or owners.get(candidate) != owner:
            raise BoundaryError("host interception is not inside its canonical owner")
        if candidate in result:
            raise BoundaryError("duplicate host interception")
        result[candidate] = {
            "candidate": f"0x{candidate:08X}",
            "owner": f"0x{owner:08X}",
            "symbol": symbol,
        }
    return result


def load_switch_tables(document: object, functions: list[FunctionBody]) -> list[SwitchTable]:
    if not isinstance(document, dict) or document.get("schema") != SWITCH_TABLE_SCHEMA:
        raise BoundaryError("switch table schema")
    if document.get("project") != PROJECT or document.get("program") != PROGRAM:
        raise BoundaryError("switch table Ghidra identity")
    if document.get("sha256") != XEX_SHA256:
        raise BoundaryError("switch table XEX identity")
    records = document.get("switch_tables")
    if not isinstance(records, list) or not records:
        raise BoundaryError("switch table records")

    owners = function_owners(functions)
    entries = {function.entry for function in functions}
    tables: list[SwitchTable] = []
    seen: set[int] = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {
            "address", "owner", "register", "labels", "evidence"
        }:
            raise BoundaryError("switch table record")
        address = parse_address(record["address"])
        owner = parse_address(record["owner"])
        register = record["register"]
        raw_labels = record["labels"]
        evidence = record["evidence"]
        if address in seen:
            raise BoundaryError("duplicate switch table")
        if owner not in entries or owners.get(address) != owner:
            raise BoundaryError("switch table outside canonical owner")
        if not isinstance(register, int) or not 0 <= register <= 31:
            raise BoundaryError("switch table register")
        if not isinstance(raw_labels, list) or len(raw_labels) < 2:
            raise BoundaryError("switch table labels")
        labels = tuple(parse_address(label) for label in raw_labels)
        if any(label not in owners for label in labels):
            raise BoundaryError("switch table label outside canonical code")
        if not isinstance(evidence, str) or not evidence.strip():
            raise BoundaryError("switch table evidence")
        seen.add(address)
        tables.append(SwitchTable(address, owner, register, labels, evidence))
    if [table.address for table in tables] != sorted(table.address for table in tables):
        raise BoundaryError("switch tables are not ordered")
    return tables


def append_switch_tables(payload: bytes, tables: list[SwitchTable]) -> bytes:
    if b"[[switch_tables]]" in payload:
        raise BoundaryError("input configuration already contains switch tables")
    lines = ["", "# Runtime-measured switch tables; see the qualified manifest."]
    for table in tables:
        lines.extend([
            "[[switch_tables]]",
            f"address = 0x{table.address:08X}",
            f"register = {table.register}",
            "labels = [",
            *(f"  0x{label:08X}," for label in table.labels),
            "]",
            "",
        ])
    separator = b"" if payload.endswith(b"\n") else b"\n"
    return payload + separator + ("\n".join(lines).rstrip() + "\n").encode()


def derive_configuration(
    payload: bytes,
    functions: list[FunctionBody],
    interceptions: dict[int, dict[str, str]] | None = None,
) -> tuple[bytes, list[dict[str, str]], int, list[dict[str, str]]]:
    entries = {function.entry for function in functions}
    owners = function_owners(functions)
    interceptions = interceptions or {}

    removed: list[dict[str, str]] = []
    preserved: list[dict[str, str]] = []
    retained = 0
    output: list[bytes] = []
    for line in payload.splitlines(keepends=True):
        match = FUNCTION_LINE.fullmatch(line)
        if match is None:
            output.append(line)
            continue
        candidate_text, generated_name = match.groups()
        if candidate_text[2:] != generated_name:
            raise BoundaryError("generated function name/address mismatch")
        candidate = int(candidate_text, 16)
        owner = owners.get(candidate)
        if owner is not None and candidate not in entries:
            if candidate in interceptions:
                preserved.append(interceptions[candidate])
                retained += 1
                output.append(line)
            else:
                removed.append({
                    "candidate": f"0x{candidate:08X}",
                    "owner": f"0x{owner:08X}",
                })
        else:
            retained += 1
            output.append(line)
    if not removed:
        raise BoundaryError("no false function starts found")
    if {int(item["candidate"], 16) for item in preserved} != set(interceptions):
        raise BoundaryError("host interception absent from input configuration")
    return b"".join(output), removed, retained, preserved


def generate(
    boundaries_path: Path,
    input_path: Path,
    output_path: Path,
    decisions_path: Path,
    expected_input_sha256: str,
    interceptions_path: Path | None = None,
    switch_tables_path: Path | None = None,
) -> dict[str, object]:
    paths = [boundaries_path, input_path, output_path, decisions_path]
    if interceptions_path is not None:
        paths.append(interceptions_path)
    if switch_tables_path is not None:
        paths.append(switch_tables_path)
    distinct = {path.resolve() for path in paths}
    if len(distinct) != len(paths):
        raise BoundaryError("all input and output paths must differ")
    boundary_payload = boundaries_path.read_bytes()
    document = json.loads(boundary_payload)
    functions = load_boundaries(document)
    input_payload = input_path.read_bytes()
    input_digest = sha256_bytes(input_payload)
    if input_digest != expected_input_sha256:
        raise BoundaryError("input configuration identity")
    interception_payload = b""
    interceptions: dict[int, dict[str, str]] = {}
    if interceptions_path is not None:
        interception_payload = interceptions_path.read_bytes()
        interceptions = load_interceptions(json.loads(interception_payload), functions)
    output_payload, removed, retained, preserved = derive_configuration(
        input_payload, functions, interceptions
    )
    switch_table_payload = b""
    switch_tables: list[SwitchTable] = []
    if switch_tables_path is not None:
        switch_table_payload = switch_tables_path.read_bytes()
        switch_tables = load_switch_tables(json.loads(switch_table_payload), functions)
        output_payload = append_switch_tables(output_payload, switch_tables)
    decision = {
        "schema": DECISION_SCHEMA,
        "boundary_export": boundaries_path.as_posix(),
        "boundary_export_sha256": sha256_bytes(boundary_payload),
        "input_configuration_sha256": input_digest,
        "output_configuration_sha256": sha256_bytes(output_payload),
        "canonical_function_count": len(functions),
        "retained_function_starts": retained,
        "preserved_internal_host_interceptions": preserved,
        "runtime_measured_switch_tables": [
            {
                "address": f"0x{table.address:08X}",
                "owner": f"0x{table.owner:08X}",
                "register": table.register,
                "labels": [f"0x{label:08X}" for label in table.labels],
                "evidence": table.evidence,
            }
            for table in switch_tables
        ],
        "removed_function_starts": removed,
    }
    if interceptions_path is not None:
        decision["host_interceptions"] = interceptions_path.as_posix()
        decision["host_interceptions_sha256"] = sha256_bytes(interception_payload)
    if switch_tables_path is not None:
        decision["switch_tables"] = switch_tables_path.as_posix()
        decision["switch_tables_sha256"] = sha256_bytes(switch_table_payload)
    output_path.write_bytes(output_payload)
    decisions_path.write_text(json.dumps(decision, indent=2) + "\n", encoding="utf-8")
    return decision


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("boundaries", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("decisions", type=Path)
    parser.add_argument("--input-sha256", required=True)
    parser.add_argument("--host-interceptions", type=Path)
    parser.add_argument("--switch-tables", type=Path)
    arguments = parser.parse_args()
    try:
        decision = generate(
            arguments.boundaries,
            arguments.input,
            arguments.output,
            arguments.decisions,
            arguments.input_sha256,
            arguments.host_interceptions,
            arguments.switch_tables,
        )
    except (BoundaryError, OSError, json.JSONDecodeError) as error:
        parser.error(str(error))
    print(
        "AC6_ORACLE_CONFIG_GENERATION_PASS "
        f"canonical={decision['canonical_function_count']} "
        f"removed={len(decision['removed_function_starts'])} "
        f"interceptions={len(decision['preserved_internal_host_interceptions'])} "
        f"switch_tables={len(decision['runtime_measured_switch_tables'])} "
        f"retained={decision['retained_function_starts']} "
        f"sha256={decision['output_configuration_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
