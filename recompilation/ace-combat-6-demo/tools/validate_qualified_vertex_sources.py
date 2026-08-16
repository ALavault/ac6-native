#!/usr/bin/env python3
"""Validate reached PAL shaders statically without retaining proprietary bytes."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


BASEFILE_SHA256 = "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
SPIRV_VAL_SHA256 = "2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406"
SOURCES = (
    ("099625", "vertex", 0x13E20, 96, 15,
     "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
     7800, "944fd75222b6de743b9ce1cd18440b8497230e3813bb105c655cd6cfba123ce6"),
    ("93488c", "vertex", 0x140A0, 108, 2,
     "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
     12496, "ba9b97cceb816059cd21ff6abfda6c59160363155d5b270a3e315b215adb0576"),
    ("586168", "vertex", 0x14140, 60, 3,
     "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
     9288, "4913cadb00aef0bba3f42c25e25919b6403e2de654e8165748337df331cdc920"),
    ("491360", "pixel", 0x13E80, 36, 1,
     "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
     7008, "f6422d60ff48b5ed43292db838655199322a6d439fea10d39302deda69ece9fe"),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if len(sys.argv) != 4:
        raise RuntimeError("usage: validate_qualified_vertex_sources.py BASEFILE PROBE SPIRV_VAL")
    basefile, probe, validator = map(Path, sys.argv[1:])
    temporary = os.environ.get("TMPDIR")
    if not temporary or not Path(temporary).is_dir():
        raise RuntimeError("TMPDIR must name an existing directory")
    if digest(basefile) != BASEFILE_SHA256:
        raise RuntimeError("PAL basefile identity mismatch")
    if digest(validator) != SPIRV_VAL_SHA256:
        raise RuntimeError("pinned spirv-val identity mismatch")
    image = basefile.read_bytes()
    with tempfile.TemporaryDirectory(prefix="ac6-demo-qualified-vs.", dir=temporary) as directory:
        root = Path(directory)
        for (name, stage, offset, size, registers, source_sha, spirv_size,
             spirv_sha) in SOURCES:
            source = image[offset:offset + size]
            if len(source) != size or hashlib.sha256(source).hexdigest() != source_sha:
                raise RuntimeError(f"{name}: qualified image range mismatch")
            source_path = root / f"{name}.bin"
            output_path = root / f"{name}.spv"
            source_path.write_bytes(source)
            subprocess.run((str(probe), stage, str(registers), str(source_path),
                            source_sha, str(output_path)), check=True)
            output = output_path.read_bytes()
            if len(output) != spirv_size or hashlib.sha256(output).hexdigest() != spirv_sha:
                raise RuntimeError(f"{name}: translated SPIR-V golden mismatch")
            subprocess.run((str(validator), "--target-env", "vulkan1.1",
                            "--scalar-block-layout", str(output_path)), check=True)
    print("qualified_shader_sources=4/4 spirv_val=4/4 temporary_only=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"fail-closed: {error}", file=sys.stderr)
        raise SystemExit(2) from error
