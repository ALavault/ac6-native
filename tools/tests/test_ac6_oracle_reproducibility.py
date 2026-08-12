from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from audit_ac6_oracle_reproducibility import (
    CONTRACT,
    ReproducibilityError,
    validate_document,
)


class OracleReproducibilityTests(unittest.TestCase):
    def document(self) -> dict:
        return json.loads((ROOT / CONTRACT).read_text(encoding="utf-8"))

    def test_tracked_reproducibility_contract(self) -> None:
        validate_document(self.document(), ROOT)

    def test_timing_cannot_claim_sixty_fps_oracle(self) -> None:
        document = copy.deepcopy(self.document())
        document["timing"]["oracle_presentation_fps"] = 60

        with self.assertRaisesRegex(
            ReproducibilityError, "oracle timing contract"
        ):
            validate_document(document, ROOT)

    def test_capture_diagnostics_cannot_close_gate(self) -> None:
        document = copy.deepcopy(self.document())
        document["capture_build"]["codegen_diagnostics"]["gate_evidence"] = True

        with self.assertRaisesRegex(
            ReproducibilityError, "codegen diagnostic boundary"
        ):
            validate_document(document, ROOT)


if __name__ == "__main__":
    unittest.main()
