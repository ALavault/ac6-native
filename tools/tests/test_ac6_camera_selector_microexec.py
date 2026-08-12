from __future__ import annotations

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


class CameraSelectorMicroexecTests(unittest.TestCase):
    def test_repository_evidence_passes(self) -> None:
        audit()

    def test_substituted_semantics_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "analysis/microexec/camera"
            target.mkdir(parents=True)
            for name in (
                "mode2-target-selector-direct.ppc.json",
                "mode3-gain-curve.ppc.json",
            ):
                document = json.loads(
                    (ROOT / "analysis/microexec/camera" / name).read_text(
                        encoding="utf-8"
                    )
                )
                if name.startswith("mode2"):
                    document["provenance"]["asserted_semantics_enabled"] = True
                (target / name).write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaises(CameraSelectorEvidenceError):
                audit(root)


if __name__ == "__main__":
    unittest.main()
