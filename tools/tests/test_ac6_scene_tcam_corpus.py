from __future__ import annotations

import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from audit_ac6_retail_content_cache import CURRENT, IDENTITY, RECORD, V2_HEADER
from audit_ac6_scene_tcam_corpus import (
    EXPECTED,
    CorpusError,
    MissionScan,
    _read_v2_index,
    audit,
    parse_scene_paths,
    parse_strict_fhm,
    scan_mission_payload,
    validate_expected,
    validate_matrix_document,
    validate_tcam_mop,
)


RETAIL_SENTINEL = b"RETAIL_SECRET_BYTES_MUST_NOT_REACH_JSON"


def make_fhm(children: list[bytes]) -> bytes:
    count = len(children)
    table_end = 0x14 + count * 16
    offsets: list[int] = []
    cursor = table_end
    for child in children:
        if child:
            offsets.append(cursor)
            cursor += len(child)
        else:
            offsets.append(0)
    data = bytearray(cursor)
    data[:4] = b"FHM "
    data[4] = 1
    data[5] = 1
    struct.pack_into(">H", data, 6, 0x10)
    struct.pack_into(">I", data, 0x10, count)
    for index, (offset, child) in enumerate(zip(offsets, children, strict=True)):
        struct.pack_into(">I", data, 0x14 + index * 4, offset)
        struct.pack_into(">I", data, 0x14 + count * 4 + index * 4, len(child))
        if child:
            data[offset : offset + len(child)] = child
    return bytes(data)


def make_sparse_fhm() -> bytes:
    data = bytearray(make_fhm([b"first", b"", b"third"]))
    struct.pack_into(">I", data, 0x18, 0xDEADBEEF)
    return bytes(data)


def make_scene_table(paths: list[str]) -> bytes:
    records = []
    for path in paths:
        encoded = path.encode("ascii") + b"\0"
        if len(encoded) > 0x80:
            raise AssertionError("synthetic Scene path is too long")
        records.append(encoded.ljust(0x80, b"\0"))
    return b"".join(records)


def make_tcam_mop(marker: int = 0) -> bytes:
    gyz_offset = 0x40
    gyz_size = 0x100
    record_table = 0x50
    data = bytearray(gyz_offset + gyz_size)
    struct.pack_into(">I", data, 0x20, gyz_size)
    struct.pack_into(">I", data, 0x30, gyz_offset)
    data[gyz_offset : gyz_offset + 4] = b"GYZ\0"
    struct.pack_into(">I", data, gyz_offset + 8, gyz_size)
    struct.pack_into(">I", data, gyz_offset + 0x0C, 0x20)
    struct.pack_into(">I", data, gyz_offset + 0x20, 0x00011E00)
    struct.pack_into(">I", data, gyz_offset + 0x2C, 3)
    struct.pack_into(">I", data, gyz_offset + 0x30, record_table)
    for index in range(3):
        record = gyz_offset + record_table + index * 0x30
        struct.pack_into(">I", data, record + 0x10, index + 1)
        struct.pack_into(">I", data, record + 0x14, 0xE0 + index * 2)
        struct.pack_into(">I", data, record + 0x18, 0xE1 + index * 2)
    data[-8:] = RETAIL_SENTINEL[:7] + bytes([marker & 0xFF])
    return bytes(data)


def make_scene_group(
    mission_id: int,
    table_index: int,
    first_path_index: int,
    path_count: int,
    has_tcam: bool,
) -> bytes:
    paths: list[str] = []
    resources: list[bytes] = []
    for local_index in range(path_count):
        path_index = first_path_index + local_index
        if has_tcam and local_index == 0:
            name = f"Tcam__m{mission_id:02d}_{table_index:03d}.mop"
            resource = make_tcam_mop(table_index)
        else:
            name = f"Track__m{mission_id:02d}_{path_index:04d}.mop"
            resource = RETAIL_SENTINEL + struct.pack(">II", table_index, path_index)
        paths.append(f"Scene/m{mission_id:02d}/g{table_index:03d}/{name}")
        resources.append(resource)
    nfic = b"NFICCUT\0" + b"\0" * 8
    return make_fhm([nfic, make_fhm(resources), make_scene_table(paths)])


def make_mission_payload(mission_id: int) -> bytes:
    table_count, path_count, tcam_count, branches = EXPECTED[mission_id]
    if table_count == 0:
        return make_fhm([b"opaque"])
    per_table = [path_count // table_count] * table_count
    for index in range(path_count % table_count):
        per_table[index] += 1
    grouped: dict[int, list[bytes]] = {branch: [] for branch in branches}
    first_path = 0
    for table_index, count in enumerate(per_table):
        branch = branches[table_index % len(branches)]
        grouped[branch].append(
            make_scene_group(
                mission_id,
                table_index,
                first_path,
                count,
                table_index < tcam_count,
            )
        )
        first_path += count
    root_children = [b"opaque"] * (max(branches) + 1)
    for branch, groups in grouped.items():
        root_children[branch] = make_fhm(groups)
    return make_fhm(root_children)


def write_v2_cache(cache: Path) -> dict[int, bytes]:
    payloads = {mission_id + 8: make_mission_payload(mission_id) for mission_id in EXPECTED}
    labels = ("xex", "data_tbl", "data00", "data01")
    digests = [bytes.fromhex(IDENTITY[label][1]) for label in labels]
    sizes = [IDENTITY[label][0] for label in labels]
    header = V2_HEADER.pack(
        b"AC6RIDX\0",
        2,
        V2_HEADER.size,
        RECORD.size,
        926,
        926,
        2,
        *digests,
        *sizes,
        bytes(32),
    )
    rows = []
    for index in range(926):
        payload = payloads.get(index, b"x")
        digest = hashlib.sha256(payload).digest()
        rows.append(
            RECORD.pack(
                index,
                0x00020000,
                0,
                2,
                0,
                index * 2_000_000,
                len(payload),
                len(payload),
                len(payload),
                digest,
                digest,
            )
        )
    raw_index = header + b"".join(rows)
    index_digest = hashlib.sha256(raw_index).digest()
    index_dir = cache / "indices"
    index_dir.mkdir(parents=True)
    (index_dir / f"{index_digest.hex()}.ac6idx").write_bytes(raw_index)
    (cache / "current").write_bytes(CURRENT.pack(b"AC6RCUR\0", 2, CURRENT.size, index_digest))
    for payload in payloads.values():
        digest = hashlib.sha256(payload).hexdigest()
        blob = cache / "blobs" / "sha256" / digest[:2] / digest
        blob.parent.mkdir(parents=True, exist_ok=True)
        blob.write_bytes(payload)
    return payloads


class SceneTcamParserTests(unittest.TestCase):
    def test_strict_fhm_and_scene_table_positive(self) -> None:
        scene = make_scene_table(["Scene/a/Tcam__a.mop"])
        parsed = parse_strict_fhm(make_fhm([b"opaque", scene]))
        self.assertEqual(2, parsed.declared_count)
        self.assertEqual(("Scene/a/Tcam__a.mop",), parse_scene_paths(scene))

    def test_strict_fhm_rejects_header_and_overlapping_children(self) -> None:
        invalid = bytearray(make_fhm([b"first", b"second"]))
        invalid[5] = 0
        with self.assertRaises(CorpusError):
            parse_strict_fhm(bytes(invalid))

    def test_strict_fhm_accepts_sparse_zero_size_slot(self) -> None:
        parsed = parse_strict_fhm(make_sparse_fhm())
        self.assertEqual((0, 2), tuple(child.index for child in parsed.live_children))

        invalid = bytearray(make_fhm([b"first", b"second"]))
        first_offset = struct.unpack_from(">I", invalid, 0x14)[0]
        struct.pack_into(">I", invalid, 0x18, first_offset + 1)
        with self.assertRaises(CorpusError):
            parse_strict_fhm(bytes(invalid))

    def test_scene_records_reject_bad_prefix_terminator_and_padding(self) -> None:
        cases = [
            b"Other/a.mop\0".ljust(0x80, b"\0"),
            b"Scene/" + b"a" * (0x80 - len(b"Scene/")),
            b"Scene/a.mop\0X".ljust(0x80, b"\0"),
            b"Scene/../a.mop\0".ljust(0x80, b"\0"),
        ]
        for payload in cases:
            with self.subTest(payload=payload[:16]):
                with self.assertRaises(CorpusError):
                    parse_scene_paths(payload)

    def test_tcam_mop_validates_every_bounded_field(self) -> None:
        valid = make_tcam_mop()
        validate_tcam_mop(valid, "test")
        gyz = 0x40
        mutations = {
            "outer_size": (0x20, 0),
            "inner_size": (gyz + 8, 0),
            "header_size": (gyz + 0x0C, 0x10),
            "tag": (gyz + 0x20, 0),
            "count": (gyz + 0x2C, 2),
            "table": (gyz + 0x30, 0xF0),
            "data_offset": (gyz + 0x50 + 0x14, 0x20),
        }
        for name, (offset, value) in mutations.items():
            invalid = bytearray(valid)
            struct.pack_into(">I", invalid, offset, value)
            with self.subTest(name=name):
                with self.assertRaises(CorpusError):
                    validate_tcam_mop(bytes(invalid), "test")
        invalid = bytearray(valid)
        invalid[gyz] = ord("X")
        with self.assertRaises(CorpusError):
            validate_tcam_mop(bytes(invalid), "test")

    def test_triplet_and_cardinality_fail_closed(self) -> None:
        paths = make_scene_table(["Scene/a/Track.mop", "Scene/a/Track2.mop"])
        bad_cardinality = make_fhm([make_fhm([b"NFICCUT\0" + b"\0" * 8, make_fhm([b"one"]), paths])])
        with self.assertRaises(CorpusError):
            scan_mission_payload(bad_cardinality, 1)

        bad_triplet = make_fhm([make_fhm([b"wrong-state", make_fhm([b"one", b"two"]), paths])])
        with self.assertRaises(CorpusError):
            scan_mission_payload(bad_triplet, 1)

    def test_exact_mission_census_is_enforced(self) -> None:
        empty = MissionScan(0, 0, (), ())
        validate_expected(2, empty)
        with self.assertRaises(CorpusError):
            validate_expected(1, empty)
        with self.assertRaises(CorpusError):
            validate_expected(16, empty)


class SceneTcamCorpusTests(unittest.TestCase):
    def test_complete_v2_corpus_cli_is_deterministic_and_metadata_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "private-retail-cache"
            write_v2_cache(cache)

            matrix = audit(cache)
            self.assertEqual(
                {
                    "mission_payloads": 15,
                    "scene_paths": 2950,
                    "scene_tables": 176,
                    "tcam_resources": 88,
                },
                matrix["totals"],
            )
            self.assertEqual(list(range(1, 16)), [m["mission_id"] for m in matrix["missions"]])
            self.assertEqual(list(range(9, 24)), [m["payload_entry"] for m in matrix["missions"]])

            output = root / "matrix" / "scene-tcam.json"
            command = [
                sys.executable,
                str(TOOLS / "audit_ac6_scene_tcam_corpus.py"),
                "--cache",
                str(cache),
                "--matrix-out",
                str(output),
            ]
            first = subprocess.run(command, check=False, capture_output=True, text=True)
            self.assertEqual(0, first.returncode, first.stdout + first.stderr)
            first_bytes = output.read_bytes()
            second = subprocess.run(command, check=False, capture_output=True, text=True)
            self.assertEqual(0, second.returncode, second.stdout + second.stderr)
            self.assertEqual(first_bytes, output.read_bytes())

            checked = subprocess.run(
                [
                    sys.executable,
                    str(TOOLS / "audit_ac6_scene_tcam_corpus.py"),
                    "--check-matrix",
                    str(output),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, checked.returncode, checked.stdout + checked.stderr)
            self.assertIn("metadata_only=1", checked.stdout)

            serialized = first_bytes.decode("utf-8")
            self.assertNotIn(RETAIL_SENTINEL.decode("ascii"), serialized)
            self.assertNotIn(str(cache), serialized)
            document = json.loads(serialized)
            self.assertFalse(document["policy"]["retail_bytes_embedded"])
            self.assertFalse(document["policy"]["cache_paths_embedded"])
            self.assertEqual(88, len(document["tcam_resources"]))
            self.assertTrue(all(not value.startswith("/") for value in _all_strings(document)))

            document["retail_payload"] = RETAIL_SENTINEL.decode("ascii")
            with self.assertRaises(CorpusError):
                validate_matrix_document(document)

    def test_non_v2_current_pointer_fails_before_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            cache.joinpath("current").write_bytes(CURRENT.pack(b"AC6RCUR\0", 1, CURRENT.size, bytes(32)))
            with self.assertRaises(CorpusError):
                _read_v2_index(cache)


def _all_strings(value: object) -> list[str]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, dict):
        strings: list[str] = []
        for key, child in value.items():
            strings.extend(_all_strings(key))
            strings.extend(_all_strings(child))
        return strings
    if isinstance(value, list):
        strings = []
        for child in value:
            strings.extend(_all_strings(child))
        return strings
    return []


if __name__ == "__main__":
    unittest.main()
