from __future__ import annotations

from collections.abc import Callable
import json
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from audit_ac6_camera_selector_microexec import (  # noqa: E402
    CameraSelectorEvidenceError,
    ROOT,
    audit,
)


EVIDENCE_NAMES = (
    "mode2-target-selector-direct.ppc.json",
    "mode2-indirect-scalar-tail.ppc.json",
    "mode2-indirect-small-direction.ppc.json",
    "mode3-gain-curve.ppc.json",
    "mode3-axis-normalizer.ppc.json",
    "mode2-tunnel-query-hit.ppc.json",
    "mode2-tunnel-query-miss.ppc.json",
    "mode2-tunnel-query-inactive.ppc.json",
)


def copy_evidence(root: Path, changed_name: str, mutate: Callable[[dict], None]) -> None:
    target = root / "analysis/microexec/camera"
    target.mkdir(parents=True)
    for name in EVIDENCE_NAMES:
        document = json.loads((ROOT / "analysis/microexec/camera" / name).read_text(encoding="utf-8"))
        if name == changed_name:
            mutate(document)
        (target / name).write_text(json.dumps(document), encoding="utf-8")


class CameraSelectorMicroexecTests(unittest.TestCase):
    def test_repository_evidence_passes(self) -> None:
        audit()

    def test_substituted_semantics_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode3-axis-normalizer.ppc.json",
                lambda document: document["provenance"].__setitem__("asserted_semantics_enabled", True),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_indirect_scalar_result_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-indirect-scalar-tail.ppc.json",
                lambda document: document["memory_writes"][0].__setitem__("after_hex", "00000000"),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_indirect_small_direction_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-indirect-small-direction.ppc.json",
                lambda document: document["registers"].__setitem__("f31", "0x0000000000000000"),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_indirect_small_direction_return_exit_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-indirect-small-direction.ppc.json",
                lambda document: document.__setitem__("exit", {"kind": "return"}),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_mode3_axis_result_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode3-axis-normalizer.ppc.json",
                lambda document: document["region_dumps"][0].__setitem__("after_hex", "0000000000000000"),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_mode3_axis_execution_census_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode3-axis-normalizer.ppc.json",
                lambda document: document["provenance"].__setitem__("callee_entries", 7),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_mode3_axis_register_bridge_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode3-axis-normalizer.ppc.json",
                lambda document: document["provenance"].__setitem__("register_file_bridge", True),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_tunnel_hit_result_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-tunnel-query-hit.ppc.json",
                lambda document: document["registers"].__setitem__("r3", "0x00000000"),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_tunnel_miss_callback_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-tunnel-query-miss.ppc.json",
                lambda document: next(
                    region for region in document["provenance"]["regions"] if region["name"] == "vtable_query"
                ).__setitem__("sha256", "0" * 64),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)

    def test_tunnel_inactive_bit_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_evidence(
                root,
                "mode2-tunnel-query-inactive.ppc.json",
                lambda document: next(
                    region for region in document["provenance"]["regions"] if region["name"] == "tunnel_flags"
                ).__setitem__("sha256", "0" * 64),
            )
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)


if __name__ == "__main__":
    unittest.main()
