from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from audit_ac6_contract_artifacts import cited_paths
from contract_audit_scope import ContractScopeError, current_contracts


class ContractAuditScopeTests(unittest.TestCase):
    def test_superseded_evidence_is_not_in_current_scope(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            replacement = root / "gate-v2.json"
            replacement.write_text('{"schema":"gate.v2"}', encoding="utf-8")
            historical = root / "gate-v1.json"
            historical.write_text(
                json.dumps({
                    "schema": "gate.v1",
                    "superseded_by": str(replacement),
                    "evidence": [{"path": "gone.bin", "sha256": "0" * 64}],
                }),
                encoding="utf-8",
            )

            active, superseded = current_contracts([historical, replacement])

            self.assertEqual([replacement], [record.path for record in active])
            self.assertEqual([historical], [record.path for record in superseded])
            self.assertEqual(set(), cited_paths(active[0].document))

    def test_missing_replacement_cannot_hide_a_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            historical = Path(temporary) / "gate-v1.json"
            historical.write_text(
                '{"superseded_by":"missing-v2.json"}', encoding="utf-8"
            )
            with self.assertRaises(ContractScopeError):
                current_contracts([historical])

    def test_self_supersession_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            historical = Path(temporary) / "gate.json"
            historical.write_text(
                json.dumps({"superseded_by": str(historical)}), encoding="utf-8"
            )
            with self.assertRaises(ContractScopeError):
                current_contracts([historical])


if __name__ == "__main__":
    unittest.main()
