#!/usr/bin/env python3
"""Fail-closed source verifier for the AC6 demo XAudio CPU frontier."""

from __future__ import annotations

import argparse
from pathlib import Path

REQUIRED = {
    "recompilation/ace-combat-6-demo/include/ac6demo/guest_bridge.hpp": [
        "pin_guest_thread_processor",
        "std::uint8_t processor{};",
        "bool processor_pinned{};",
    ],
    "recompilation/ace-combat-6-demo/src/guest_bridge/scheduler.hpp": [
        "memory_.store_u8(processor_field, thread.processor);",
        "found->processor = index;",
    ],
    "recompilation/ace-combat-6-demo/src/guest_bridge/kernel_objects_dispatch.hpp": [
        "pin_guest_thread_processor(context.r3.u32, context.r4.u32)",
    ],
    "recompilation/ace-combat-6-demo/src/guest_bridge/lifecycle.hpp": [
        "const auto dispatch_xaudio_frame = [&]()",
        "current_guest_thread_id = 2U;",
        "AC6_PPC_CALL_INDIRECT(frame, memory_.raw_base(), xaudio_callback_);",
    ],
}

FORBIDDEN_IN_XAUDIO_SCOPE = [
    "memory_.store_u8(active_cpu_address, 4U);",
    "memory_.store_u8(active_cpu_address, 5U);",
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    failures: list[str] = []
    texts: dict[str, str] = {}
    for relative, needles in REQUIRED.items():
        path = args.repo / relative
        if not path.is_file():
            failures.append(f"missing {relative}")
            continue
        text = path.read_text(encoding="utf-8")
        texts[relative] = text
        for needle in needles:
            if needle not in text:
                failures.append(f"{relative}: missing {needle!r}")

    lifecycle = texts.get(
        "recompilation/ace-combat-6-demo/src/guest_bridge/lifecycle.hpp", ""
    )
    start = lifecycle.find("const auto dispatch_xaudio_frame = [&]()")
    end = lifecycle.find("if (xenos_cp_interrupts_pending_", start)
    scope = lifecycle[start:end] if start >= 0 and end > start else ""
    if not scope:
        failures.append("cannot isolate dispatch_xaudio_frame")
    for forbidden in FORBIDDEN_IN_XAUDIO_SCOPE:
        if forbidden in scope:
            failures.append(f"hard-coded callback CPU found: {forbidden}")

    if failures:
        for failure in failures:
            print(f"FAIL {failure}")
        return 1
    print("thread_affinity_state=present")
    print("fiber_pcr_publication=present")
    print("xaudio_cpu_hardcode=absent")
    print("xaudio_callback_cpu_frontier=OPEN")
    print("status=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
