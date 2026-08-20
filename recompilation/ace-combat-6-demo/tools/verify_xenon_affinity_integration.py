#!/usr/bin/env python3
from __future__ import annotations

import hashlib
from pathlib import Path

EXPECTED_ORIGINAL_GIT_BLOB = "957e5f587a9be82f2d7efaf0e44b1a4b840aaf38"


def git_blob_sha(data: bytes) -> str:
    header = f"blob {len(data)}\0".encode()
    return hashlib.sha1(header + data).hexdigest()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    wrapper = root / "src/guest_bridge/kernel_objects_dispatch.hpp"
    original = root / "src/guest_bridge/kernel_objects_dispatch_original.hpp"
    trace = root / "src/guest_bridge/affinity_trace.hpp"
    contract = root / "include/ac6demo/xenon_affinity_contract.hpp"
    workflow = root.parents[1] / ".github/workflows/xaudio-runtime-ab.yml"

    assert wrapper.is_file()
    assert original.is_file()
    assert trace.is_file()
    assert contract.is_file()
    assert not workflow.exists(), "temporary XAudio Actions workflow remains"
    assert git_blob_sha(original.read_bytes()) == EXPECTED_ORIGINAL_GIT_BLOB

    wrapper_text = wrapper.read_text()
    assert wrapper_text.index('name} == "KeSetAffinityThread"') < \
           wrapper_text.index('#include "kernel_objects_dispatch_original.hpp"')
    assert "context.r3.u32 = transition.previous_mask" in wrapper_text
    assert "previous_affinity_pointer" not in wrapper_text
    assert "memory().store_u32" not in wrapper_text

    trace_text = trace.read_text()
    assert "AC6_AFFINITY_TRANSITION" in trace_text
    assert "stale_r5" in trace_text
    print("status=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
