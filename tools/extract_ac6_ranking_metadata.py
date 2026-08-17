#!/usr/bin/env python3
"""Extract AC6 demo leaderboard metadata from a reconstructed flat PE image.

The script emits only labels and small structural tables. It does not copy code,
resources, XEX data or leaderboard payloads.
"""

from __future__ import annotations

import argparse
import csv
import json
import struct
from pathlib import Path

BASE = 0x82000000
STRING_ADDRS = {
    "difficulty_ace": 0x8200E81C,
    "difficulty_expert": 0x8200E820,
    "difficulty_hard": 0x8200E828,
    "difficulty_normal": 0x8200E830,
    "difficulty_easy": 0x8200E838,
    "column_board_id": 0x8200E840,
    "column_special_weapon": 0x8200E84C,
    "column_aircraft": 0x8200E85C,
    "column_time": 0x8200E868,
    "column_kills": 0x8200E870,
    "column_score": 0x8200E878,
    "column_country": 0x8200E880,
    "offline": 0x8200EB98,
    "online": 0x8200EBA0,
    "mission_score": 0x8200EBA8,
    "battle_royale": 0x8200EBBC,
    "team_battle": 0x8200EBCC,
    "coop_battle": 0x8200EBD8,
    "siege_battle": 0x8200EBE8,
    "loading": 0x8200EBF8,
    "receiving": 0x8200EC04,
    "sending": 0x8200EC10,
    "ranking_display": 0x8200EC1C,
    "ranking_end": 0x8200EC54,
}
COLUMN_TABLE = 0x82391988
COLUMN_COUNT = 7
COLUMN_STRIDE = 24


def read_cstr(data: bytes, address: int, limit: int = 512) -> str:
    offset = address - BASE
    if offset < 0 or offset >= len(data):
        raise ValueError(f"address outside image: 0x{address:08X}")
    raw = data[offset:offset + limit].split(b"\0", 1)[0]
    return raw.decode("shift_jis", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("flat_pe", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    data = args.flat_pe.read_bytes()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    strings = {name: read_cstr(data, address) for name, address in STRING_ADDRS.items()}
    (args.output_dir / "ranking_strings.json").write_text(
        json.dumps(strings, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    rows = []
    table_offset = COLUMN_TABLE - BASE
    for ordinal in range(COLUMN_COUNT):
        raw = data[
            table_offset + ordinal * COLUMN_STRIDE:
            table_offset + (ordinal + 1) * COLUMN_STRIDE
        ]
        if len(raw) != COLUMN_STRIDE:
            raise ValueError("truncated column table")
        label_ptr, column_id, flags, aux0, aux1, aux2 = struct.unpack(">6I", raw)
        rows.append({
            "ordinal": ordinal,
            "label_address": f"0x{label_ptr:08X}",
            "label": read_cstr(data, label_ptr),
            "column_id": column_id,
            "flags": f"0x{flags:08X}",
            "aux0": aux0,
            "aux1": aux1,
            "aux2": aux2,
        })

    with (args.output_dir / "ranking_columns.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    print(f"strings={len(strings)} columns={len(rows)} output={args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
