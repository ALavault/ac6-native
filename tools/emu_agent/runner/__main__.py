"""Command-line entry point for the local deterministic runner."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .local import run_local, run_safe
from tools.emu_agent.protocol import canonical_json
from tools.emu_agent.protocol.errors import ProtocolError


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="run one emu-agent/v1 request")
    parser.add_argument("request", nargs="?", type=Path, help="request JSON (default: stdin)")
    parser.add_argument("--safe", action="store_true", help="emit an error result for backend failures")
    args = parser.parse_args(argv)
    source = args.request if args.request is not None else sys.stdin.buffer.read()
    try:
        result = run_safe(source) if args.safe else run_local(source)
    except (ProtocolError, OSError, ValueError) as error:
        print(f"emu-agent: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(canonical_json(result) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
