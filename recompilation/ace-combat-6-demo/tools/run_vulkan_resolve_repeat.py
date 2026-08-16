#!/usr/bin/env python3
"""Run the reached Vulkan resolve oracle in two fresh processes."""

from __future__ import annotations

import subprocess
import sys
import hashlib
import os
from pathlib import Path
import re
import struct
import tempfile


HEADER_SHA256 = "de24d6b23367da6b2fa3b5d1d843d920cbdf4a5170cf49544412c4bebcb1eb11"
SPIRV_SHA256 = "e8cfb0d6981476118cefbf797d33092ccf09281f728d34220c1514dd79487b32"
VALIDATOR_SHA256 = "2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406"


def run(executable: str) -> str:
    completed = subprocess.run(
        [executable], text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            completed.stderr.strip() or
            f"Vulkan resolve oracle exited {completed.returncode}"
        )
    return completed.stdout.strip()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_spirv(header: Path, validator: Path) -> None:
    if digest(header) != HEADER_SHA256 or digest(validator) != VALIDATOR_SHA256:
        raise RuntimeError("pinned resolve shader or spirv-val hash mismatch")
    text = header.read_text()
    match = re.search(
        r"const uint32_t resolve_fast_32bpp_1x2xmsaa_cs\[\] = \{(.*?)\n\};",
        text, re.DOTALL,
    )
    if match is None:
        raise RuntimeError("embedded resolve SPIR-V array not found")
    words = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]+", match.group(1))]
    blob = struct.pack(f"<{len(words)}I", *words)
    if len(blob) != 8380 or hashlib.sha256(blob).hexdigest() != SPIRV_SHA256:
        raise RuntimeError("embedded resolve SPIR-V identity mismatch")
    temporary_root = os.environ.get("TMPDIR")
    if temporary_root != "/fastdata/lavaulta/tmp":
        raise RuntimeError("TMPDIR must be /fastdata/lavaulta/tmp")
    with tempfile.TemporaryDirectory(prefix="ac6-resolve-spirv-", dir=temporary_root) as directory:
        module = Path(directory) / "resolve.spv"
        module.write_bytes(blob)
        completed = subprocess.run(
            [str(validator), "--target-env", "vulkan1.1", str(module)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        if completed.returncode != 0:
            raise RuntimeError(completed.stderr.strip() or "spirv-val failed")


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: run_vulkan_resolve_repeat.py EXECUTABLE HEADER SPIRV_VAL"
        )
    validate_spirv(Path(sys.argv[2]), Path(sys.argv[3]))
    first = run(sys.argv[1])
    second = run(sys.argv[1])
    if first != second:
        raise RuntimeError("fresh Vulkan resolve outputs differ")
    print(first)
    print(f"fresh_processes=2 deterministic=true spirv_sha256={SPIRV_SHA256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
