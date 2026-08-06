#!/usr/bin/env python3
"""Fail-closed audit for the ac6-native CPack tarball."""
from __future__ import annotations

import argparse
import re
import tarfile
from pathlib import Path

FORBIDDEN_NAMES = re.compile(r"(DATA\d*\.PAC|\.xex$|oracle|\.ntxr$|\.f32$|\.ppm$)", re.I)
FORBIDDEN_BYTES = re.compile(rb"xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc", re.I)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    if not args.archive.is_file():
        raise SystemExit("error: archive not found")
    with tarfile.open(args.archive, "r:gz") as archive:
        members = archive.getmembers()
        for member in members:
            if FORBIDDEN_NAMES.search(member.name):
                raise SystemExit(f"error: forbidden package entry: {member.name}")
            if member.isfile() and not member.name.endswith(".py") and member.size <= 256 * 1024 * 1024:
                payload = archive.extractfile(member).read()  # type: ignore[union-attr]
                if FORBIDDEN_BYTES.search(payload):
                    raise SystemExit(f"error: forbidden marker in package entry: {member.name}")
    print(f"package_audit=pass entries={len(members)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
