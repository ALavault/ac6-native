#!/usr/bin/env python3
"""Generate native_pinned_shaders_data.cpp from the pinned capsule fixture.

Run by native/CMakeLists.txt at configure time. The generated file lives in
the build tree (ignored working copy) and only wraps the fixture bytes as a
base64 string literal; the single source of truth stays the fixture.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
from pathlib import Path

HEADER = """// GENERATED from native/fixtures/pinned-shader-registry.v1.bin
// (sha256 {sha256}) by native/tools/materialize_pinned_shader_data.py.
// Do not edit: regenerate the fixture instead.
#include "ac6/native_pinned_shaders.h"

namespace ac6::native {{

const char* pinned_shader_registry_base64() noexcept {{
  static const char kPayload[] = R"AC6PINNED({payload})AC6PINNED";
  return kPayload;
}}

}}  // namespace ac6::native
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capsule", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    data = arguments.capsule.read_bytes()
    if data[:8] != b"AC6PSR1\x00":
        raise SystemExit("not a pinned shader registry capsule")
    payload = base64.b64encode(data).decode("ascii")
    text = HEADER.format(sha256=hashlib.sha256(data).hexdigest(), payload=payload)
    arguments.output.write_text(text, encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
