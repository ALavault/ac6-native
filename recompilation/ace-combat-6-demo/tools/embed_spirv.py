#!/usr/bin/env python3
"""Embed a validated SPIR-V module as a deterministic C++ word array."""

import argparse
import hashlib
import pathlib
import struct


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args()
    data = args.input.read_bytes()
    if not data or len(data) % 4:
        raise SystemExit("SPIR-V input must be non-empty aligned dwords")
    words = struct.unpack(f"<{len(data) // 4}I", data)
    lines = [
        "#pragma once",
        "#include <array>",
        "#include <cstdint>",
        "namespace ac6demo::generated {",
        f'inline constexpr char {args.symbol}_sha256[] = "{hashlib.sha256(data).hexdigest()}";',
        f"inline constexpr std::array<std::uint32_t, {len(words)}> {args.symbol}{{{{",
    ]
    for offset in range(0, len(words), 8):
        lines.append("  " + ", ".join(f"0x{word:08X}U" for word in words[offset:offset + 8]) + ",")
    lines.extend(["}};", "} // namespace ac6demo::generated", ""])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
