#!/usr/bin/env python3
"""Refuse a test suite whose assert() calls NDEBUG would erase.

The demo suites state every check as assert(). The configured build type is
RelWithDebInfo, which defines NDEBUG, so for eighty-seven commits ten of the
twelve demo test executables compiled their assertions away and passed
vacuously; a deliberately wrong constant added to ac6-demo-frontier-tests
passed. Only ac6-demo-input-tests and ac6-xbox360-host-xam-input-movie-tests
carried -UNDEBUG.

The build-system fix (-UNDEBUG on every assert-based target) cannot defend
itself: a test target added later is vacuous again, silently. So each such
source carries an "#ifdef NDEBUG / #error" guard, which turns the mistake
into a build failure, and this checks that the guard is present wherever a
runtime assert() is -- the one thing the guard itself cannot do.

static_assert is not affected by NDEBUG and is not counted.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
# assert( not preceded by an identifier character, so static_assert and
# BOOST_assert-style names do not match.
RUNTIME_ASSERT = re.compile(r"(?<![_A-Za-z0-9])assert\s*\(")
GUARD = re.compile(r"#\s*ifdef\s+NDEBUG\b[^#]*#\s*error", re.S)


def strip_comments_and_strings(text: str) -> str:
    text = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r'"(?:\\.|[^"\\])*"', ' ', text)


def audit(roots: list[Path]) -> tuple[int, list[str]]:
    checked = 0
    failures: list[str] = []
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            raw = path.read_text(encoding="utf-8", errors="replace")
            if not RUNTIME_ASSERT.search(strip_comments_and_strings(raw)):
                continue
            checked += 1
            if not GUARD.search(raw):
                failures.append(path.as_posix())
    return checked, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", type=Path,
                        help="test directories to audit")
    arguments = parser.parse_args()
    checked, failures = audit([root.resolve() for root in arguments.roots])
    if failures:
        print("test_assert_liveness=fail")
        for path in failures:
            print(f"error: {path}: uses assert() without an "
                  f"'#ifdef NDEBUG / #error' guard, so NDEBUG would erase "
                  f"every check in it")
        return 1
    print(f"test_assert_liveness=pass suites={checked}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
