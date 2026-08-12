from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from audit_ac6_global_checkpoint2 import (
    CONTRACT,
    Checkpoint2Error,
    audit_document,
)


class GlobalCheckpoint2Tests(unittest.TestCase):
    def document(self) -> dict:
        return json.loads(CONTRACT.read_text(encoding="utf-8"))

    def test_live_contract_is_fail_closed(self) -> None:
        audit_document(self.document())

    def test_checkpoint_cannot_pass_with_open_lane(self) -> None:
        document = copy.deepcopy(self.document())
        document["state"] = "passed"

        with self.assertRaisesRegex(
            Checkpoint2Error, "checkpoint state outruns lanes"
        ):
            audit_document(document)

    def test_lane_cannot_pass_with_residual_blockers(self) -> None:
        document = copy.deepcopy(self.document())
        document["lanes"][0]["state"] = "passed"

        with self.assertRaisesRegex(
            Checkpoint2Error, "vmx-vmx128 passed blockers"
        ):
            audit_document(document)

    def test_static_first_policy_is_required(self) -> None:
        document = copy.deepcopy(self.document())
        del document["policy"]["microexecution_is_ambiguity_escalation"]

        with self.assertRaisesRegex(Checkpoint2Error, "checkpoint policy"):
            audit_document(document)

    def test_evidence_strategy_must_remain_passed(self) -> None:
        document = copy.deepcopy(self.document())
        document["evidence_strategy"]["state"] = "open"

        with self.assertRaisesRegex(
            Checkpoint2Error, "checkpoint evidence strategy state"
        ):
            audit_document(document)

    def test_semantic_scope_is_mission01(self) -> None:
        document = copy.deepcopy(self.document())
        document["semantic_scope"] = "all-missions"

        with self.assertRaisesRegex(Checkpoint2Error, "semantic scope"):
            audit_document(document)

    def test_shared_reader_regression_scope_remains_global(self) -> None:
        document = copy.deepcopy(self.document())
        document["shared_reader_regression_scope"] = ["M01"]

        with self.assertRaisesRegex(Checkpoint2Error, "shared reader"):
            audit_document(document)


if __name__ == "__main__":
    unittest.main()
