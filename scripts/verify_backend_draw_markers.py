#!/usr/bin/env python3
"""Verify the bounded AC6 backend draw-marker instrumentation."""

from __future__ import annotations

import argparse
from pathlib import Path


def extract_function(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function signature: {signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise AssertionError(f"missing function body: {signature}")
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function body: {signature}")


def verify_backend(path: Path, class_name: str, api_name: str, draw_calls: tuple[str, str]) -> int:
    source = path.read_text(encoding="utf-8")
    body = extract_function(source, f"bool {class_name}::IssueDraw(")
    assertions = 0

    expected_issue = f"ReportHostIssueCalled(ac6::backend::BackendDrawApi::{api_name})"
    assert body.count(expected_issue) == 1
    assertions += 1

    expected_result = f"BackendDrawApi::{api_name}, success"
    assert body.count(expected_result) == 1
    assertions += 1

    assert body.count("return draw_result(IssueCopy());") == 1
    assertions += 1
    assert body.count("return draw_result(true);") == 5
    assertions += 1
    assert "return true;" not in body
    assertions += 1

    expected_emission = f"ReportHostDrawEmitted(ac6::backend::BackendDrawApi::{api_name})"
    assert body.count(expected_emission) == 2
    assertions += 1

    for draw_call in draw_calls:
        call_index = body.find(draw_call)
        assert call_index >= 0
        marker_index = body.find(expected_emission, call_index)
        assert marker_index > call_index
        assert marker_index - call_index < 320
        assertions += 3

    return assertions


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkout", type=Path, required=True)
    args = parser.parse_args()

    root = args.checkout.resolve()
    assertions = 0
    assertions += verify_backend(
        root / "thirdparty/rexglue-sdk/src/graphics/vulkan/command_processor.cpp",
        "VulkanCommandProcessor",
        "kVulkan",
        ("CmdVkDraw(", "CmdVkDrawIndexed("),
    )
    assertions += verify_backend(
        root / "thirdparty/rexglue-sdk/src/graphics/d3d12/command_processor.cpp",
        "D3D12CommandProcessor",
        "kD3D12",
        ("D3DDrawInstanced(", "D3DDrawIndexedInstanced("),
    )
    print(f"AC6_BACKEND_DRAW_MARKER_ASSERTIONS={assertions}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
