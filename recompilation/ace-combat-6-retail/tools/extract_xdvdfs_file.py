#!/usr/bin/env python3
"""Extract one bounded file from an identity-qualified Xbox 360 XDVDFS ISO."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Any


PRODUCT = Path(__file__).resolve().parents[1]
WORKSPACE = PRODUCT.parents[1]
MAGIC = b"MICROSOFT*XBOX*MEDIA"
SECTOR_SIZE = 2048
ENTRY_HEADER_SIZE = 14
DIRECTORY_ATTRIBUTE = 0x10
ORDINAL_TERMINATOR = 0xFFFF
MAX_DIRECTORY_SIZE = 32 * 1024 * 1024
MAX_DIRECTORY_DEPTH = 64
DEFAULT_MAX_FILE_SIZE = 1024 * 1024
LIKELY_GAME_OFFSETS = (0, 0xFB20, 0x20600, 0x02080000, 0x0FD90000, 0x18300000)


class ExtractionError(RuntimeError):
    pass


@dataclass(frozen=True)
class Volume:
    game_offset: int
    root_sector: int
    root_size: int


@dataclass(frozen=True)
class Entry:
    name: str
    sector: int
    length: int
    attributes: int


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ExtractionError(f"JSON object required: {path}")
    return value


def read_exact(stream: BinaryIO, offset: int, length: int, image_size: int) -> bytes:
    if offset < 0 or length < 0 or offset > image_size or length > image_size - offset:
        raise ExtractionError("XDVDFS range extends beyond the ISO")
    stream.seek(offset)
    data = stream.read(length)
    if len(data) != length:
        raise ExtractionError("short read from ISO")
    return data


def read_volume(stream: BinaryIO, image_size: int) -> Volume:
    for game_offset in LIKELY_GAME_OFFSETS:
        descriptor_offset = game_offset + 32 * SECTOR_SIZE
        if descriptor_offset + SECTOR_SIZE > image_size:
            continue
        descriptor = read_exact(stream, descriptor_offset, SECTOR_SIZE, image_size)
        if descriptor[:len(MAGIC)] != MAGIC:
            continue
        if descriptor[0x7EC:0x7EC + len(MAGIC)] != MAGIC:
            raise ExtractionError("XDVDFS volume descriptor has no trailing magic")
        root_sector, root_size = struct.unpack_from("<II", descriptor, 20)
        if root_size < ENTRY_HEADER_SIZE - 1 or root_size > MAX_DIRECTORY_SIZE:
            raise ExtractionError("invalid XDVDFS root directory size")
        root_offset = game_offset + root_sector * SECTOR_SIZE
        read_exact(stream, root_offset, root_size, image_size)
        return Volume(game_offset, root_sector, root_size)
    raise ExtractionError("XDVDFS volume descriptor not found")


def read_directory(
    stream: BinaryIO,
    image_size: int,
    volume: Volume,
    sector: int,
    length: int,
    visited_tables: set[int],
) -> list[Entry]:
    if length < ENTRY_HEADER_SIZE - 1 or length > MAX_DIRECTORY_SIZE:
        raise ExtractionError("invalid XDVDFS directory size")
    table_offset = volume.game_offset + sector * SECTOR_SIZE
    if table_offset in visited_tables:
        raise ExtractionError("XDVDFS directory cycle detected")
    visited_tables.add(table_offset)
    table = read_exact(stream, table_offset, length, image_size)
    entries: list[Entry] = []
    pending = [0]
    seen: set[int] = set()
    while pending:
        ordinal = pending.pop()
        if ordinal in seen:
            continue
        seen.add(ordinal)
        offset = ordinal * 4
        if offset + ENTRY_HEADER_SIZE > len(table):
            continue
        node_l, node_r, entry_sector, entry_length, attributes, name_length = (
            struct.unpack_from("<HHIIBB", table, offset)
        )
        for node in (node_l, node_r):
            if node not in (0, ORDINAL_TERMINATOR):
                pending.append(node)
        name_end = offset + ENTRY_HEADER_SIZE + name_length
        if not name_length or name_end > len(table):
            continue
        try:
            name = table[offset + ENTRY_HEADER_SIZE:name_end].decode("ascii")
        except UnicodeDecodeError as error:
            raise ExtractionError("non-ASCII XDVDFS file name") from error
        data_offset = volume.game_offset + entry_sector * SECTOR_SIZE
        if data_offset > image_size or entry_length > image_size - data_offset:
            raise ExtractionError(f"XDVDFS entry extends beyond the ISO: {name}")
        entries.append(Entry(name, entry_sector, entry_length, attributes))
    return entries


def resolve_entry(stream: BinaryIO, image_size: int, internal_path: str) -> tuple[Volume, Entry]:
    parts = [part for part in internal_path.replace("\\", "/").split("/") if part]
    if not parts or any(part in {".", ".."} for part in parts):
        raise ExtractionError("internal path must name one file inside the ISO")
    volume = read_volume(stream, image_size)
    sector = volume.root_sector
    length = volume.root_size
    visited_tables: set[int] = set()
    for depth, part in enumerate(parts):
        if depth > MAX_DIRECTORY_DEPTH:
            raise ExtractionError("XDVDFS directory depth exceeded")
        entries = read_directory(
            stream, image_size, volume, sector, length, visited_tables
        )
        entry = next((item for item in entries if item.name.casefold() == part.casefold()), None)
        if entry is None:
            raise ExtractionError(f"file not found in XDVDFS image: {internal_path}")
        is_directory = bool(entry.attributes & DIRECTORY_ATTRIBUTE)
        if depth + 1 == len(parts):
            if is_directory:
                raise ExtractionError(f"internal path names a directory: {internal_path}")
            return volume, entry
        if not is_directory:
            raise ExtractionError(f"internal path crosses a file: {entry.name}")
        sector, length = entry.sector, entry.length
    raise AssertionError("non-empty path did not resolve")


def extract_file(iso: Path, internal_path: str, maximum_size: int) -> tuple[Volume, Entry, bytes]:
    image_size = iso.stat().st_size
    with iso.open("rb") as stream:
        volume, entry = resolve_entry(stream, image_size, internal_path)
        if entry.length > maximum_size:
            raise ExtractionError(
                f"bounded extraction rejected {entry.length} bytes (maximum {maximum_size})"
            )
        offset = volume.game_offset + entry.sector * SECTOR_SIZE
        return volume, entry, read_exact(stream, offset, entry.length, image_size)


def workspace_relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(WORKSPACE))
    except ValueError:
        return str(path.resolve())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj",))
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument("--path", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--max-bytes", type=int, default=DEFAULT_MAX_FILE_SIZE)
    arguments = parser.parse_args()

    definition = load_json(PRODUCT / "targets" / f"{arguments.target}.json")
    expected = definition.get("iso", {})
    iso = arguments.iso.resolve()
    if not iso.is_file():
        raise ExtractionError(f"ISO is not a file: {iso}")
    if arguments.max_bytes < 1 or arguments.max_bytes > 16 * 1024 * 1024:
        raise ExtractionError("--max-bytes must be in 1..16777216")
    image_size = iso.stat().st_size
    if image_size != expected.get("size"):
        raise ExtractionError(f"ISO size mismatch: {image_size} != {expected.get('size')}")
    iso_digest = file_sha256(iso)
    if iso_digest != expected.get("sha256"):
        raise ExtractionError(f"ISO SHA-256 mismatch: {iso_digest}")
    for path in (arguments.output, arguments.receipt):
        if path.exists():
            raise ExtractionError(f"refusing to overwrite: {path}")
        path.parent.mkdir(parents=True, exist_ok=True)

    volume, entry, payload = extract_file(iso, arguments.path, arguments.max_bytes)
    payload_digest = hashlib.sha256(payload).hexdigest()
    arguments.output.write_bytes(payload)
    receipt = {
        "schema": "ac6.xdvdfs-bounded-extraction.v1",
        "status": "qualified",
        "target": arguments.target,
        "source_iso": {
            "path": str(iso),
            "size": image_size,
            "sha256": iso_digest,
        },
        "xdvdfs": {
            "game_offset": volume.game_offset,
            "root_sector": volume.root_sector,
            "root_size": volume.root_size,
        },
        "file": {
            "internal_path": arguments.path.replace("\\", "/"),
            "sector": entry.sector,
            "iso_offset": volume.game_offset + entry.sector * SECTOR_SIZE,
            "size": entry.length,
            "sha256": payload_digest,
            "output": workspace_relative(arguments.output),
        },
    }
    arguments.receipt.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(
        f"xdvdfs_extract=pass target={arguments.target} path={arguments.path} "
        f"size={entry.length} sha256={payload_digest}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ExtractionError, OSError, json.JSONDecodeError) as error:
        print(f"xdvdfs_extract=fail error={error}")
        raise SystemExit(1)
