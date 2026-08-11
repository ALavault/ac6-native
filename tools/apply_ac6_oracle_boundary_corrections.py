#!/usr/bin/env python3
"""Apply only the Ghidra-qualified AC6_recomp function-boundary corrections."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


class CorrectionError(ValueError):
    pass


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def correct_configuration(payload: bytes, addresses: list[str]) -> bytes:
    """Remove each exact generated-name entry once, preserving every other byte."""
    if not addresses or len(addresses) != len(set(addresses)):
        raise CorrectionError("function-start list must be non-empty and unique")

    try:
        lines = payload.decode("utf-8").splitlines(keepends=True)
    except UnicodeDecodeError as error:
        raise CorrectionError("configuration is not UTF-8") from error

    expected = {
        f'{address} = {{ name = "rex_sub_{address[2:]}" }}\n': address
        for address in addresses
    }
    counts = {address: 0 for address in addresses}
    output: list[str] = []
    for line in lines:
        address = expected.get(line)
        if address is None:
            output.append(line)
        else:
            counts[address] += 1

    invalid = [f"{address}:{count}" for address, count in counts.items() if count != 1]
    if invalid:
        raise CorrectionError("expected each exact entry once: " + ", ".join(invalid))
    return "".join(output).encode("utf-8")


def apply_manifest(manifest_path: Path, input_path: Path, output_path: Path) -> str:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    if document.get("schema") != "ac6.recomp-oracle-manifest.v1":
        raise CorrectionError("manifest schema")
    configuration = document.get("configuration", {})
    correction = document.get("boundary_correction", {})
    addresses = correction.get("removed_function_starts")
    if not isinstance(addresses, list) or not all(isinstance(item, str) for item in addresses):
        raise CorrectionError("manifest removed_function_starts")

    payload = input_path.read_bytes()
    if sha256_bytes(payload) != configuration.get("sha256"):
        raise CorrectionError("input configuration identity")
    corrected = correct_configuration(payload, addresses)
    digest = sha256_bytes(corrected)
    if digest != correction.get("patched_configuration_sha256"):
        raise CorrectionError("corrected configuration identity")
    if input_path.resolve() == output_path.resolve():
        raise CorrectionError("input and output must differ")
    output_path.write_bytes(corrected)
    return digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    try:
        digest = apply_manifest(arguments.manifest, arguments.input, arguments.output)
    except (CorrectionError, OSError, json.JSONDecodeError) as error:
        parser.error(str(error))
    print(f"AC6_ORACLE_BOUNDARY_CORRECTION_PASS sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
