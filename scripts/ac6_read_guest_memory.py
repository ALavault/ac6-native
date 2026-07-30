#!/usr/bin/env python3
"""Read AC6 guest memory from a running ac6recomp process, without a debugger.

Motivated by cycle 322: a guest value was reported as `0` when the measurement
had actually read the high half of a 64-bit big-endian field. Every read here
therefore declares its width explicitly, and the tool refuses to report
anything until the guest base mapping has been validated against a known
instruction word.

Reads go through the shared-memory file that backs guest memory, not through
/proc/<pid>/mem. rexglue allocates guest memory as a `/dev/shm/xenia_memory_*`
mapping whose file offsets equal guest virtual addresses, and reading that file
needs no ptrace capability -- so this works under `yama/ptrace_scope = 1`, where
/proc/<pid>/mem returns EACCES to a non-parent process, and it never stops or
perturbs the guest. /proc/<pid>/maps is still read, to bind the shm file to the
live process rather than to a leftover from an earlier run.

Neither the shm identity nor the offset rule is assumed. Both are *proved* per
invocation by reading an anchor instruction word out of the loaded XEX image and
requiring it to equal the value the generated corpus says is there.

Usage:
  ac6_read_guest_memory.py --pid PID --u64 0x82870828 --u32 0x8287082C
  ac6_read_guest_memory.py --pid PID --dump 0x82870780:176
"""

from __future__ import annotations

import argparse
import json
import re
import sys

# Anchor: guest instruction `ld r11,16(r31)` inside sub_82346108, the 64-bit
# load of the shared condition value. Encoding recomputed from the PPC form
# ld rD,ds(rA): (58 << 26) | (11 << 21) | (31 << 16) | ((16 >> 2) << 2).
ANCHOR_GUEST_ADDRESS = 0x82346140
ANCHOR_BIG_ENDIAN_WORD = 0xE97F0010

SHM_PATH = re.compile(r"(/dev/shm/xenia_memory_\d+)")


class GuestMemory:
    """The shm file backing one live process's guest address space."""

    def __init__(self, pid: int) -> None:
        self.pid = pid
        self.path = self._resolve_path(pid)
        self.handle = open(self.path, "rb", 0)
        if not self._qualify():
            raise SystemExit(
                f"guest memory not qualified: {self.path} did not reproduce the anchor word "
                f"{ANCHOR_BIG_ENDIAN_WORD:#010x} at guest {ANCHOR_GUEST_ADDRESS:#010x}. "
                "Refusing to report values from an unproven mapping."
            )

    @staticmethod
    def _resolve_path(pid: int) -> str:
        found: set[str] = set()
        with open(f"/proc/{pid}/maps", "r") as handle:
            for line in handle:
                match = SHM_PATH.search(line)
                if match:
                    found.add(match.group(1))
        if not found:
            raise SystemExit(
                f"pid {pid} maps no /dev/shm/xenia_memory_* region; it is not a live "
                "ac6recomp guest, or guest memory was not initialised yet"
            )
        if len(found) > 1:
            raise SystemExit(f"pid {pid} maps several guest memory files: {sorted(found)}")
        return found.pop()

    def read(self, guest_address: int, length: int) -> bytes | None:
        """Guest virtual address -> shm file offset is the identity mapping."""
        try:
            self.handle.seek(guest_address)
            data = self.handle.read(length)
        except OSError:
            return None
        if data is None or len(data) != length:
            return None
        return data

    def _qualify(self) -> bool:
        word = self.read(ANCHOR_GUEST_ADDRESS, 4)
        if word is None:
            return False
        return int.from_bytes(word, "big") == ANCHOR_BIG_ENDIAN_WORD


def parse_int(text: str) -> int:
    return int(text, 0)


def parse_dump(text: str) -> tuple[int, int]:
    address, _, length = text.partition(":")
    return parse_int(address), parse_int(length or "16")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--u64", action="append", default=[], metavar="GUEST_ADDRESS")
    parser.add_argument("--u32", action="append", default=[], metavar="GUEST_ADDRESS")
    parser.add_argument("--dump", action="append", default=[], metavar="GUEST_ADDRESS:LENGTH")
    args = parser.parse_args()

    memory = GuestMemory(args.pid)
    result: dict[str, object] = {
        "schema": "ac6.guest-memory-read/v1",
        "pid": args.pid,
        "guest_memory_file": memory.path,
        "read_route": "shm-backing-file (no ptrace, non-perturbing)",
        "anchor": {
            "guest_address": f"{ANCHOR_GUEST_ADDRESS:#010x}",
            "expected_big_endian_u32": f"{ANCHOR_BIG_ENDIAN_WORD:#010x}",
            "qualified": True,
            "meaning": "ld r11,16(r31) in sub_82346108",
        },
        "reads": [],
    }
    reads: list[dict[str, object]] = result["reads"]  # type: ignore[assignment]

    for text in args.u64:
        guest = parse_int(text)
        data = memory.read(guest, 8)
        if data is None:
            reads.append({"guest_address": f"{guest:#010x}", "width_bits": 64, "error": "unreadable"})
            continue
        value = int.from_bytes(data, "big")
        reads.append(
            {
                "guest_address": f"{guest:#010x}",
                "width_bits": 64,
                "endianness": "big",
                "bytes": data.hex(),
                "value": value,
                "value_hex": f"{value:#018x}",
                "high_half_u32_at": f"{guest:#010x}",
                "high_half_u32": int.from_bytes(data[0:4], "big"),
                "low_half_u32_at": f"{guest + 4:#010x}",
                "low_half_u32": int.from_bytes(data[4:8], "big"),
            }
        )

    for text in args.u32:
        guest = parse_int(text)
        data = memory.read(guest, 4)
        if data is None:
            reads.append({"guest_address": f"{guest:#010x}", "width_bits": 32, "error": "unreadable"})
            continue
        value = int.from_bytes(data, "big")
        reads.append(
            {
                "guest_address": f"{guest:#010x}",
                "width_bits": 32,
                "endianness": "big",
                "bytes": data.hex(),
                "value": value,
                "value_hex": f"{value:#010x}",
            }
        )

    for text in args.dump:
        guest, length = parse_dump(text)
        data = memory.read(guest, length)
        if data is None:
            reads.append({"guest_address": f"{guest:#010x}", "length": length, "error": "unreadable"})
            continue
        reads.append(
            {
                "guest_address": f"{guest:#010x}",
                "length": length,
                "bytes": data.hex(),
                "u32_big_endian": [
                    int.from_bytes(data[offset : offset + 4], "big")
                    for offset in range(0, length - 3, 4)
                ],
            }
        )

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
