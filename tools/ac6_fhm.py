#!/usr/bin/env python3
"""Dependency-free bounded parser for AC6 FHM containers."""

from __future__ import annotations

import re
import struct
from dataclasses import dataclass, field


@dataclass(frozen=True)
class FhmChild:
    index: int
    offset: int
    declared_size: int
    size: int
    magic: str
    data: bytes
    notes: list[str] = field(default_factory=list)


def _magic(data: bytes) -> str:
    return data[:4].decode("latin-1", errors="replace") if data else ""


def parse_fhm(blob: bytes) -> list[FhmChild] | None:
    if len(blob) < 0x18 or blob[:4] != b"FHM ":
        return None
    count = struct.unpack_from(">I", blob, 0x10)[0]
    if count > 1_000_000:
        return None
    offsets_begin = 0x14
    sizes_begin = offsets_begin + 4 * count
    tables_end = sizes_begin + 4 * count
    if tables_end > len(blob):
        return None
    offsets = struct.unpack_from(f">{count}I", blob, offsets_begin) if count else ()
    sizes = struct.unpack_from(f">{count}I", blob, sizes_begin) if count else ()
    children: list[FhmChild] = []
    for index, (offset, declared_size) in enumerate(zip(offsets, sizes)):
        notes: list[str] = []
        if offset > len(blob):
            data = b""
            notes.append("offset past end of container")
        else:
            available = len(blob) - offset
            actual_size = min(declared_size, available)
            data = blob[offset : offset + actual_size]
            if declared_size > available:
                notes.append(
                    f"declared size 0x{declared_size:x} truncated to 0x{actual_size:x}"
                )
        if index + 1 < count and offsets[index + 1] < offset:
            notes.append("next child offset is not monotonic")
        children.append(FhmChild(index, offset, declared_size, len(data), _magic(data), data, notes))
    return children


_EXTENSION_BY_MAGIC = {
    "FHM ": ".fhm",
    "MDLP": ".mdlp",
    "PLAD": ".plad",
    "NTXR": ".ntxr",
    "NDXR": ".ndxr",
    "NSXR": ".nsxr",
    "NFIC": ".nfic",
    "Scen": ".scen",
    "CAPT": ".capt",
    "SWG ": ".swg",
    "ACE6": ".ace6",
}


def ext_for_magic(magic: str) -> str:
    return _EXTENSION_BY_MAGIC.get(magic, ".bin")


def safe_tag(value: str) -> str:
    text = "".join(ch if 32 <= ord(ch) < 127 else f"_{ord(ch):02x}" for ch in value)
    text = re.sub(r"[^A-Za-z0-9._-]+", "_", text).strip("._-")
    return text or "unknown"
