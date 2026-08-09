from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from audit_ac6_retail_content_cache import (
    AuditError,
    CURRENT,
    HEADER,
    IDENTITY,
    MISSION01_REQUIRED_ENTRIES,
    RECORD,
    cross_check_mission01_resources,
    parse_index,
    read_current,
)


def synthetic_index() -> bytes:
    labels = ("xex", "data_tbl", "data00", "data01")
    digests = [bytes.fromhex(IDENTITY[label][1]) for label in labels]
    sizes = [IDENTITY[label][0] for label in labels]
    header = HEADER.pack(
        b"AC6RIDX\0",
        1,
        HEADER.size,
        RECORD.size,
        17,
        926,
        2,
        *digests,
        *sizes,
    )
    records = []
    index = 1
    digest = bytes([index]) * 32
    records.append(
        RECORD.pack(index, 0, 0, 1, 0, index * 4096, 100, 200, 200, digest, digest)
    )
    for index in range(9, 24):
        digest = bytes([index]) * 32
        records.append(
            RECORD.pack(index, 0, 0, 1, 0, index * 4096, 100, 200, 200, digest, digest)
        )
    index = 119
    digest = bytes([index]) * 32
    records.append(
        RECORD.pack(index, 0, 0, 1, 0, index * 4096, 100, 200, 200, digest, digest)
    )
    return header + b"".join(records)


class RetailCacheAuditTests(unittest.TestCase):
    def test_binary_index_shape_is_independently_readable(self) -> None:
        identity, records = parse_index(synthetic_index())
        self.assertEqual(17, len(records))
        self.assertEqual(
            [1, *range(9, 24), 119],
            [row["data_table_entry_index"] for row in records],
        )
        self.assertEqual(IDENTITY["data00"][1], identity["data00"]["sha256"])

    def test_duplicate_or_truncated_index_fails(self) -> None:
        raw = bytearray(synthetic_index())
        second_record = HEADER.size + RECORD.size
        raw[second_record : second_record + 4] = (1).to_bytes(4, "big")
        with self.assertRaises(AuditError):
            parse_index(bytes(raw))
        with self.assertRaises(AuditError):
            parse_index(synthetic_index()[:-1])

    def test_inconsistent_record_metadata_fails(self) -> None:
        raw = bytearray(synthetic_index())
        first_record = HEADER.size
        raw[first_record + 9] = 2  # group says deflate, codec says raw
        with self.assertRaises(AuditError):
            parse_index(bytes(raw))

        raw = bytearray(synthetic_index())
        raw[first_record + 28 : first_record + 36] = (201).to_bytes(8, "big")
        with self.assertRaises(AuditError):
            parse_index(bytes(raw))

    def test_required_common_and_world_resources_are_exact(self) -> None:
        records = [
            {field: value for field, value in requirement.items() if field != "role"}
            for requirement in MISSION01_REQUIRED_ENTRIES
        ]
        checked = cross_check_mission01_resources(records)
        self.assertEqual(
            ["common_camera_tables", "mission01_world_mapset"],
            [record["role"] for record in checked],
        )
        records[0]["payload_size"] += 1
        with self.assertRaises(AuditError):
            cross_check_mission01_resources(records)

    def test_current_pointer_version_is_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            digest = bytes.fromhex("12" * 32)
            (cache / "current").write_bytes(CURRENT.pack(b"AC6RCUR\0", 1, CURRENT.size, digest))
            self.assertEqual(digest.hex(), read_current(cache))
            (cache / "current").write_bytes(CURRENT.pack(b"AC6RCUR\0", 2, CURRENT.size, digest))
            with self.assertRaises(AuditError):
                read_current(cache)


if __name__ == "__main__":
    unittest.main()
