import importlib.util
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "audit_ac6_global_ladder.py"
SPEC = importlib.util.spec_from_file_location("audit_ac6_global_ladder", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def test_global_ladder_contract() -> None:
    MODULE.audit()
