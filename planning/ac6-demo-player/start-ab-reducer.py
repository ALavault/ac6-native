#!/usr/bin/env python3
"""CLI skeleton for a qualified neutral/buttons-16 trace reducer.

The implementation intentionally refuses to emit a receipt until trace
identity, tick ordering, domain comparison and content-addressed output are
implemented.
"""

from __future__ import annotations

import argparse


EXIT_SCAFFOLD_ONLY = 3


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    result.add_argument("--neutral-trace", required=True)
    result.add_argument("--input-trace", required=True)
    result.add_argument("--neutral-report", required=True)
    result.add_argument("--input-report", required=True)
    result.add_argument("--output", required=True)
    return result


def main() -> int:
    parser().parse_args()
    print(
        "start-ab-reducer: scaffold-only; TODO identity, ordering, domain "
        "comparison and atomic receipt output"
    )
    return EXIT_SCAFFOLD_ONLY


if __name__ == "__main__":
    raise SystemExit(main())
