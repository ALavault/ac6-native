#!/usr/bin/env python3
"""Fail-closed source verifier for the PAL copy differential runtime wrapper."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

EXPECTED_ORIGINAL_BLOB = "e9bbe0b7c6c6a2fc145d77a24b7b98554e527e09"


def git_blob_sha1(payload: bytes) -> str:
    header = f"blob {len(payload)}\0".encode("ascii")
    return hashlib.sha1(header + payload).hexdigest()


def require(text: str, needle: str, failures: list[str]) -> None:
    if needle not in text:
        failures.append(f"missing source anchor: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("repository", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    root = args.repository.resolve()
    demo = root / "recompilation/ace-combat-6-demo"
    wrapper = demo / "src/vulkan_neutral_resolve.cpp"
    original = demo / "src/vulkan_neutral_resolve_original.cpp"
    header = demo / "include/ac6demo/reached_copy_runtime_certificate.hpp"
    test = demo / "tests/reached_copy_runtime_certificate_tests.cpp"
    runner = demo / "tools/run_reached_copy_runtime_certificate_test.sh"

    failures: list[str] = []
    for path in (wrapper, original, header, test, runner):
        if not path.is_file():
            failures.append(f"missing file: {path.relative_to(root)}")

    if original.is_file():
        observed_blob = git_blob_sha1(original.read_bytes())
        if observed_blob != EXPECTED_ORIGINAL_BLOB:
            failures.append(
                f"original Vulkan resolve blob changed: {observed_blob}"
            )
    else:
        observed_blob = ""

    if wrapper.is_file():
        source = wrapper.read_text(encoding="utf-8")
        for needle in (
            "#define execute_vulkan_neutral_resolve",
            'include "vulkan_neutral_resolve_original.cpp"',
            "execute_vulkan_neutral_resolve_uncertified(",
            "certify_reached_copy_runtime(",
            "require_reached_copy_runtime_writeback(certificate);",
            "AC6_DEMO_WATCH_COPY_DIFFERENTIAL",
        ):
            require(source, needle, failures)
        if "AC6_DEMO_EXPERIMENTAL" in source:
            failures.append("runtime certificate contains an experimental bypass")
        if source.find("certify_reached_copy_runtime(") > source.find(
            "require_reached_copy_runtime_writeback(certificate);"
        ):
            failures.append("runtime certificate is required before it is built")
        if source.find("require_reached_copy_runtime_writeback(certificate);") > source.find(
            "return result;"
        ):
            failures.append("result returns before the certificate is enforced")

    if header.is_file():
        source = header.read_text(encoding="utf-8")
        for needle in (
            "struct ReachedCopyRuntimeCertificate",
            "writeback_allowed()",
            "AC6_COPY_DIFFERENTIAL",
            "diagnose_reached_copy(",
            "reached copy runtime certificate refused writeback",
        ):
            require(source, needle, failures)

    workflow = root / ".github/workflows/pm4-source-export.yml"
    if workflow.exists():
        failures.append("temporary GitHub Actions source-export workflow remains")

    result = {
        "schema": "ac6-demo-copy-runtime-certificate-source-audit/v1",
        "status": "PASS" if not failures else "FAIL",
        "original_git_blob_sha1": observed_blob,
        "expected_original_git_blob_sha1": EXPECTED_ORIGINAL_BLOB,
        "certificate_enforced_before_return": not failures,
        "failures": failures,
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
