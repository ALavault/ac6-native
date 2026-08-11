import importlib.util
import struct
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "catalog_ac6_data_table.py"
SPEC = importlib.util.spec_from_file_location("catalog_ac6_data_table", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def qualified_table() -> bytearray:
    table = bytearray(MODULE.EXPECTED_SIZE)
    struct.pack_into(">II", table, 0, MODULE.EXPECTED_ENTRIES, MODULE.EXPECTED_PACKS)
    for index in range(MODULE.EXPECTED_ENTRIES):
        struct.pack_into(">IIII", table, 8 + index * 16, 0, index + 1, 1, 1)
    return table


def test_rejects_wrong_identity() -> None:
    table = qualified_table()
    try:
        MODULE.catalogue_bytes(table)
    except ValueError as error:
        assert str(error) == "unqualified DATA.TBL identity"
    else:
        raise AssertionError("unqualified table accepted")


def test_qualified_repository_catalogue_is_exact() -> None:
    root = SCRIPT.parents[1]
    table = root / "game-files/DATA.TBL"
    if not table.is_file():
        return
    expected = MODULE.catalogue_bytes(table.read_bytes())
    assert (root / "analysis/data-table-catalogue.tsv").read_bytes() == expected
