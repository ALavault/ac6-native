#!/usr/bin/env python3
"""Fail-closed source verifier for the PAL renderer audit screencap path."""

from __future__ import annotations

import argparse
import hashlib
import subprocess
from pathlib import Path


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"missing {label}: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--run-test", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    encoder = root / "include/ac6demo/audit_screencap.hpp"
    publisher = root / "src/renderer_audit_screencap.hpp"
    join = root / "src/xenos_guest_present_join.hpp"
    test = root / "tests/audit_screencap_tests.cpp"
    runner = root / "tools/run_audit_screencap_test.sh"
    capture_verifier = root / "tools/verify_audit_screencap.py"
    for path in (encoder, publisher, join, test, runner, capture_verifier):
        if not path.is_file():
            raise SystemExit(f"missing source: {path}")

    encoder_text = encoder.read_text()
    publisher_text = publisher.read_text()
    join_text = join.read_text()

    for needle, label in (
        ("encode_rgba8_audit_png", "lossless RGBA encoder"),
        ("std::byte{6}", "PNG RGBA color type"),
        ("make_stored_zlib_stream", "dependency-free zlib stream"),
        ("\"IDAT\"", "PNG IDAT chunk"),
        ("AuditPixelStats", "pixel audit statistics"),
    ):
        require(encoder_text, needle, label)

    for needle, label in (
        ("AC6_DEMO_AUDIT_SCREENCAP_DIR", "opt-in environment gate"),
        ("guest-linear-after-qualified-writeback", "capture provenance"),
        ("gameplay_screenshot_claim", "non-gameplay classification"),
        ("publish_new_file(json_path", "exclusive JSON publication"),
        ("publish_new_file(png_path", "exclusive PNG publication"),
        ("AC6_AUDIT_SCREENCAP", "bounded runtime witness"),
        ("Sha256::bytes(guest_linear)", "source revalidation"),
    ):
        require(publisher_text, needle, label)

    require(join_text, '#include "renderer_audit_screencap.hpp"',
            "capture integration include")
    require(join_text, "publish_renderer_audit_screencap(session, guest_linear, resolve);",
            "capture invocation")
    writeback = join_text.index("resolve.guest_writeback = true;")
    capture = join_text.index("publish_renderer_audit_screencap(session, guest_linear, resolve);")
    clear = join_text.index("resolve.tiled_bytes.clear();")
    if not writeback < capture < clear:
        raise SystemExit("capture is not ordered after verification and before handoff release")

    if ".github/workflows" in publisher_text or "workflow_dispatch" in publisher_text:
        raise SystemExit("renderer audit capture must not depend on GitHub Actions")

    if args.run_test:
        subprocess.run([str(runner)], cwd=root, check=True)

    for path in (encoder, publisher, join, test, runner, capture_verifier):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        print(f"{path.relative_to(root)} sha256={digest}")
    print("renderer_audit_screencap_integration=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
