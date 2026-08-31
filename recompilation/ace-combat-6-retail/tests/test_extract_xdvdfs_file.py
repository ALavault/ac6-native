from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/extract_xdvdfs_file.py"
SPEC = importlib.util.spec_from_file_location("ac6_extract_xdvdfs_file", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def make_image(path: Path, payload: bytes, *, trailing_magic: bool = True) -> None:
    root_sector = 40
    file_sector = 41
    root_size = MODULE.ENTRY_HEADER_SIZE + len("DATA.TBL")
    image = bytearray((file_sector + 1) * MODULE.SECTOR_SIZE)
    descriptor = 32 * MODULE.SECTOR_SIZE
    image[descriptor:descriptor + len(MODULE.MAGIC)] = MODULE.MAGIC
    if trailing_magic:
        image[descriptor + 0x7EC:descriptor + 0x7EC + len(MODULE.MAGIC)] = MODULE.MAGIC
    struct.pack_into("<II", image, descriptor + 20, root_sector, root_size)
    root = root_sector * MODULE.SECTOR_SIZE
    struct.pack_into(
        "<HHIIBB", image, root, 0, 0, file_sector, len(payload), 0, len("DATA.TBL")
    )
    image[root + MODULE.ENTRY_HEADER_SIZE:root + root_size] = b"DATA.TBL"
    start = file_sector * MODULE.SECTOR_SIZE
    image[start:start + len(payload)] = payload
    path.write_bytes(image)


class ExtractXdvdfsFileTests(unittest.TestCase):
    def test_extracts_one_root_file_case_insensitively(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            iso = Path(directory) / "disc.iso"
            payload = b"qualified-table"
            make_image(iso, payload)
            volume, entry, extracted = MODULE.extract_file(iso, "data.tbl", 1024)
        self.assertEqual(volume.game_offset, 0)
        self.assertEqual(entry.sector, 41)
        self.assertEqual(extracted, payload)

    def test_missing_trailing_magic_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            iso = Path(directory) / "disc.iso"
            make_image(iso, b"table", trailing_magic=False)
            with self.assertRaisesRegex(MODULE.ExtractionError, "trailing magic"):
                MODULE.extract_file(iso, "DATA.TBL", 1024)

    def test_file_size_bound_is_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            iso = Path(directory) / "disc.iso"
            make_image(iso, b"0123456789")
            with self.assertRaisesRegex(MODULE.ExtractionError, "bounded extraction rejected"):
                MODULE.extract_file(iso, "DATA.TBL", 9)

    def test_parent_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            iso = Path(directory) / "disc.iso"
            make_image(iso, b"table")
            with self.assertRaisesRegex(MODULE.ExtractionError, "internal path"):
                MODULE.extract_file(iso, "../DATA.TBL", 1024)


if __name__ == "__main__":
    unittest.main()
