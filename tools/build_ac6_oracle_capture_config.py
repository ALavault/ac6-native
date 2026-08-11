#!/usr/bin/env python3
"""Derive the AC6 oracle capture config from the sealed runtime config."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import tomllib
from pathlib import Path


SCHEMA = "ac6.oracle-capture-function-overrides.v1"
DECISION_SCHEMA = "ac6.oracle-capture-config-decisions.v1"
FUNCTION = re.compile(
    rb'^(0x[0-9A-Fa-f]+)\s*=\s*\{\s*name\s*=\s*"([A-Za-z0-9_]+)"\s*\}\s*$'
)


class CaptureConfigError(RuntimeError):
    pass


def digest(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def load_boundaries(payload: bytes) -> set[int]:
    document = json.loads(payload)
    functions = document.get("functions")
    if (
        document.get("schema") != "ac6.ghidra-function-boundaries.v1"
        or document.get("project") != "ace-combat-6"
        or document.get("program") != "default.xex"
        or not isinstance(functions, list)
        or document.get("function_count") != len(functions)
    ):
        raise CaptureConfigError("canonical boundary export identity")
    return {int(record["entry"], 16) for record in functions}


def load_policy(payload: bytes, input_sha256: str, boundary_sha256: str,
                canonical: set[int]) -> list[tuple[int, str, str]]:
    document = json.loads(payload)
    if document.get("schema") != SCHEMA:
        raise CaptureConfigError("capture override schema")
    if document.get("input_configuration_sha256") != input_sha256:
        raise CaptureConfigError("capture override input identity")
    if document.get("boundary_export_sha256") != boundary_sha256:
        raise CaptureConfigError("capture override boundary identity")
    records = document.get("overrides")
    if not isinstance(records, list) or not records:
        raise CaptureConfigError("empty capture override policy")
    result: list[tuple[int, str, str]] = []
    seen: set[int] = set()
    for record in records:
        try:
            address = int(record["address"], 16)
            name = record["name"]
            evidence = record["evidence"]
        except (KeyError, TypeError, ValueError) as error:
            raise CaptureConfigError("malformed capture override") from error
        if (
            address in seen
            or address not in canonical
            or re.fullmatch(r"rex_sub_[0-9A-F]{8}", name) is None
            or name != f"rex_sub_{address:08X}"
            or not isinstance(evidence, str)
            or not evidence
        ):
            raise CaptureConfigError("invalid capture override")
        seen.add(address)
        result.append((address, name, evidence))
    return sorted(result)


def apply_overrides(payload: bytes,
                    overrides: list[tuple[int, str, str]]) -> bytes:
    lines = payload.splitlines(keepends=True)
    try:
        section = next(index for index, line in enumerate(lines)
                       if line.rstrip(b"\r\n") == b"[functions]")
    except StopIteration as error:
        raise CaptureConfigError("missing functions section") from error
    end = len(lines)
    for index in range(section + 1, len(lines)):
        stripped = lines[index].lstrip()
        if stripped.startswith(b"[") and not stripped.startswith(b"[functions]"):
            end = index
            break

    configured: dict[int, tuple[int, str]] = {}
    for index in range(section + 1, end):
        match = FUNCTION.match(lines[index].rstrip(b"\r\n"))
        if match:
            configured[int(match.group(1), 16)] = (index, match.group(2).decode())

    additions: list[tuple[int, int, bytes]] = []
    for address, name, _ in overrides:
        existing = configured.get(address)
        if existing:
            if existing[1] != name:
                raise CaptureConfigError("capture override conflicts with runtime config")
            continue
        insertion = end
        for candidate, (index, _) in sorted(configured.items()):
            if candidate > address:
                insertion = index
                break
        additions.append((
            insertion,
            address,
            f'0x{address:08X} = {{ name = "{name}" }}\n'.encode(),
        ))
        configured[address] = (insertion, name)

    for insertion, _, line in sorted(
        additions, key=lambda item: (item[0], item[1]), reverse=True
    ):
        lines.insert(insertion, line)
    result = b"".join(lines)
    tomllib.loads(result.decode("utf-8"))
    return result


def build(input_path: Path, policy_path: Path, boundaries_path: Path,
          output_path: Path, decisions_path: Path,
          expected_input_sha256: str) -> dict[str, object]:
    paths = [input_path, policy_path, boundaries_path, output_path, decisions_path]
    if len({path.resolve() for path in paths}) != len(paths):
        raise CaptureConfigError("all input and output paths must differ")
    input_payload = input_path.read_bytes()
    input_sha256 = digest(input_payload)
    if input_sha256 != expected_input_sha256:
        raise CaptureConfigError("runtime configuration identity")
    boundary_payload = boundaries_path.read_bytes()
    boundary_sha256 = digest(boundary_payload)
    canonical = load_boundaries(boundary_payload)
    policy_payload = policy_path.read_bytes()
    overrides = load_policy(policy_payload, input_sha256, boundary_sha256, canonical)
    output_payload = apply_overrides(input_payload, overrides)
    output_path.write_bytes(output_payload)
    decision = {
        "schema": DECISION_SCHEMA,
        "input_configuration_sha256": input_sha256,
        "boundary_export_sha256": boundary_sha256,
        "policy_sha256": digest(policy_payload),
        "overrides": [
            {"address": f"0x{address:08X}", "name": name, "evidence": evidence}
            for address, name, evidence in overrides
        ],
        "output_configuration_sha256": digest(output_payload),
    }
    decisions_path.write_text(json.dumps(decision, indent=2) + "\n", encoding="utf-8")
    return decision


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("policy", type=Path)
    parser.add_argument("boundaries", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("decisions", type=Path)
    parser.add_argument("--input-sha256", required=True)
    arguments = parser.parse_args()
    try:
        decision = build(arguments.input, arguments.policy, arguments.boundaries,
                         arguments.output, arguments.decisions,
                         arguments.input_sha256)
    except (CaptureConfigError, OSError, json.JSONDecodeError) as error:
        parser.error(str(error))
    print("AC6_ORACLE_CAPTURE_CONFIG_PASS "
          f"overrides={len(decision['overrides'])} "
          f"sha256={decision['output_configuration_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
