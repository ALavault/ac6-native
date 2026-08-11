#!/usr/bin/env python3
"""Build the qualified AC6_recomp runtime configuration from sealed evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import tomllib
from pathlib import Path

from generate_ac6_oracle_config import (
    BoundaryError,
    append_switch_tables,
    load_boundaries,
    load_switch_tables,
)

POLICY_SCHEMA = "ac6.oracle-midasm-hook-policy.v1"
DECISION_SCHEMA = "ac6.oracle-runtime-config-decisions.v1"


class RuntimeConfigError(ValueError):
    pass


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def parse_address(value: object) -> int:
    if not isinstance(value, str) or not value.startswith("0x"):
        raise RuntimeConfigError("hook address")
    try:
        address = int(value, 16)
    except ValueError as error:
        raise RuntimeConfigError("hook address") from error
    if not 0 <= address <= 0xFFFFFFFF:
        raise RuntimeConfigError("hook address")
    return address


def load_hook_policy(document: object, input_sha256: str) -> list[dict[str, object]]:
    if not isinstance(document, dict) or document.get("schema") != POLICY_SCHEMA:
        raise RuntimeConfigError("hook policy schema")
    if document.get("input_configuration_sha256") != input_sha256:
        raise RuntimeConfigError("hook policy input identity")
    records = document.get("hooks")
    if not isinstance(records, list) or not records:
        raise RuntimeConfigError("hook policy records")

    result: list[dict[str, object]] = []
    seen: set[tuple[int, str]] = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {
            "address", "name", "disposition", "reason"
        }:
            raise RuntimeConfigError("hook policy record")
        address = parse_address(record["address"])
        name = record["name"]
        disposition = record["disposition"]
        reason = record["reason"]
        if not isinstance(name, str) or not name:
            raise RuntimeConfigError("hook policy name")
        if disposition not in {"retain", "disable"}:
            raise RuntimeConfigError("hook policy disposition")
        if not isinstance(reason, str) or not reason.strip():
            raise RuntimeConfigError("hook policy reason")
        identity = (address, name)
        if identity in seen:
            raise RuntimeConfigError("duplicate hook policy identity")
        seen.add(identity)
        result.append({
            "address": address,
            "name": name,
            "disposition": disposition,
            "reason": reason,
        })
    return result


def configured_hooks(payload: bytes) -> list[tuple[int, str]]:
    try:
        document = tomllib.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, tomllib.TOMLDecodeError) as error:
        raise RuntimeConfigError("input configuration TOML") from error
    raw_hooks = document.get("midasm_hook")
    if not isinstance(raw_hooks, list):
        raise RuntimeConfigError("input configuration midasm hooks")
    hooks: list[tuple[int, str]] = []
    for hook in raw_hooks:
        if not isinstance(hook, dict):
            raise RuntimeConfigError("input configuration midasm hook")
        address = hook.get("address")
        name = hook.get("name")
        if not isinstance(address, int) or not isinstance(name, str):
            raise RuntimeConfigError("input configuration midasm hook identity")
        hooks.append((address, name))
    return hooks


def apply_hook_policy(payload: bytes, policy: list[dict[str, object]]) -> bytes:
    expected = [(int(item["address"]), str(item["name"])) for item in policy]
    if configured_hooks(payload) != expected:
        raise RuntimeConfigError("hook policy does not exhaustively match input")

    lines = payload.splitlines(keepends=True)
    output: list[bytes] = []
    hook_index = 0
    line_index = 0
    while line_index < len(lines):
        line = lines[line_index]
        if line.rstrip(b"\r\n") != b"[[midasm_hook]]":
            output.append(line)
            line_index += 1
            continue
        if hook_index >= len(policy):
            raise RuntimeConfigError("unexpected midasm hook block")
        disable = policy[hook_index]["disposition"] == "disable"
        while line_index < len(lines) and lines[line_index].strip():
            block_line = lines[line_index]
            output.append(b"# oracle-disabled: " + block_line if disable else block_line)
            line_index += 1
        hook_index += 1
    if hook_index != len(policy):
        raise RuntimeConfigError("missing midasm hook block")

    result = b"".join(output)
    retained = [
        (int(item["address"]), str(item["name"]))
        for item in policy if item["disposition"] == "retain"
    ]
    if configured_hooks(result) != retained:
        raise RuntimeConfigError("hook policy output verification")
    return result


def build_configuration(
    input_path: Path,
    policy_path: Path,
    boundaries_path: Path,
    switch_tables_path: Path,
    output_path: Path,
    decisions_path: Path,
    expected_input_sha256: str,
) -> dict[str, object]:
    paths = [input_path, policy_path, boundaries_path, switch_tables_path,
             output_path, decisions_path]
    if len({path.resolve() for path in paths}) != len(paths):
        raise RuntimeConfigError("all input and output paths must differ")

    input_payload = input_path.read_bytes()
    input_digest = sha256_bytes(input_payload)
    if input_digest != expected_input_sha256:
        raise RuntimeConfigError("input configuration identity")
    policy_payload = policy_path.read_bytes()
    policy = load_hook_policy(json.loads(policy_payload), input_digest)
    output_payload = apply_hook_policy(input_payload, policy)

    boundaries_payload = boundaries_path.read_bytes()
    functions = load_boundaries(json.loads(boundaries_payload))
    switches_payload = switch_tables_path.read_bytes()
    switches = load_switch_tables(json.loads(switches_payload), functions)
    output_payload = append_switch_tables(output_payload, switches)

    active_document = tomllib.loads(output_payload.decode("utf-8"))
    retained = [item for item in policy if item["disposition"] == "retain"]
    disabled = [item for item in policy if item["disposition"] == "disable"]
    decision = {
        "schema": DECISION_SCHEMA,
        "input_configuration": input_path.as_posix(),
        "input_configuration_sha256": input_digest,
        "hook_policy": policy_path.as_posix(),
        "hook_policy_sha256": sha256_bytes(policy_payload),
        "boundary_export": boundaries_path.as_posix(),
        "boundary_export_sha256": sha256_bytes(boundaries_payload),
        "switch_tables": switch_tables_path.as_posix(),
        "switch_tables_sha256": sha256_bytes(switches_payload),
        "function_starts": len(active_document.get("functions", {})),
        "retained_midasm_hooks": retained,
        "disabled_midasm_hooks": disabled,
        "runtime_measured_switch_tables": [
            {
                "address": f"0x{table.address:08X}",
                "owner": f"0x{table.owner:08X}",
                "register": table.register,
                "labels": [f"0x{label:08X}" for label in table.labels],
                "evidence": table.evidence,
            }
            for table in switches
        ],
        "output_configuration_sha256": sha256_bytes(output_payload),
    }
    output_path.write_bytes(output_payload)
    decisions_path.write_text(json.dumps(decision, indent=2) + "\n", encoding="utf-8")
    return decision


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("policy", type=Path)
    parser.add_argument("boundaries", type=Path)
    parser.add_argument("switch_tables", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("decisions", type=Path)
    parser.add_argument("--input-sha256", required=True)
    arguments = parser.parse_args()
    try:
        decision = build_configuration(
            arguments.input, arguments.policy, arguments.boundaries,
            arguments.switch_tables, arguments.output, arguments.decisions,
            arguments.input_sha256,
        )
    except (BoundaryError, RuntimeConfigError, OSError, json.JSONDecodeError) as error:
        parser.error(str(error))
    print(
        "AC6_ORACLE_RUNTIME_CONFIG_PASS "
        f"functions={decision['function_starts']} "
        f"retained_hooks={len(decision['retained_midasm_hooks'])} "
        f"disabled_hooks={len(decision['disabled_midasm_hooks'])} "
        f"switch_tables={len(decision['runtime_measured_switch_tables'])} "
        f"sha256={decision['output_configuration_sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
