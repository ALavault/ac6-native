#!/usr/bin/env python3
"""Fail-closed scaffold for AC6 content-addressed receipt verification."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


EXIT_SCAFFOLD_ONLY = 3


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--expected-schema", required=True)
    args = parser.parse_args()
    document = json.loads(args.receipt.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("schema") != args.expected_schema:
        raise SystemExit("receipt-verifier: schema mismatch")
    print(
        "receipt-verifier: scaffold-only; schema name matched but content, "
        "identity, artifact closure and canonical encoding are not verified"
    )
    return EXIT_SCAFFOLD_ONLY


if __name__ == "__main__":
    raise SystemExit(main())
