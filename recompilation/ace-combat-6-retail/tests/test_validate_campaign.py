from __future__ import annotations

import copy
import importlib.util
import json
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/validate_campaign.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_validate_campaign", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)
PRODUCT = SCRIPT.parents[1]


def documents() -> tuple[dict[str, object], dict[str, object]]:
    campaign = json.loads((PRODUCT / "targets/ntsc-uj-campaign.json").read_text())
    target = json.loads((PRODUCT / "targets/ntsc-uj.json").read_text())
    return campaign, target


class ValidateCampaignTests(unittest.TestCase):
    def test_current_manifest_is_valid_but_not_release_ready(self) -> None:
        campaign, target = documents()
        summary = MODULE.validate_campaign(campaign, target)
        self.assertEqual(summary["missions"], 15)
        self.assertEqual(summary["selector_qualified"], 15)
        self.assertEqual(summary["static_qualified"], 15)
        self.assertEqual(summary["gameplay_pass"], 0)
        self.assertEqual(summary["debrief_pass"], 0)
        self.assertEqual(summary["visual_pass"], 0)
        self.assertEqual(summary["controls"], 0)
        self.assertFalse(summary["release_ready"])

    def test_wrong_xex_identity_is_rejected(self) -> None:
        campaign, target = documents()
        campaign["identity"]["xex_sha256"] = "0" * 64
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "XEX identity"):
            MODULE.validate_campaign(campaign, target)

    def test_all_fifteen_missions_are_required(self) -> None:
        campaign, target = documents()
        campaign["missions"].pop()
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "missions 1..15"):
            MODULE.validate_campaign(campaign, target)

    def test_unqualified_data_table_index_is_rejected(self) -> None:
        campaign, target = documents()
        campaign["missions"][1]["static_route_status"] = "selector-qualified"
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "unqualified DATA.TBL"):
            MODULE.validate_campaign(campaign, target)

    def test_wrong_qualified_data_table_identity_is_rejected(self) -> None:
        campaign, target = documents()
        campaign["static_evidence"]["dpl_to_data_table"]["data_table"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "DATA.TBL contract"):
            MODULE.validate_campaign(campaign, target)

    def test_scene_tcam_static_totals_are_fail_closed(self) -> None:
        campaign, target = documents()
        campaign["static_evidence"]["scene_tcam"]["totals"]["tcam_resources"] = 87
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "Scene/TCAM"):
            MODULE.validate_campaign(campaign, target)

    def test_scenario_static_totals_are_fail_closed(self) -> None:
        campaign, target = documents()
        campaign["static_evidence"]["scenario_payloads"]["totals"]["roundtrip_pass"] = 14
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "scenario static"):
            MODULE.validate_campaign(campaign, target)

    def test_gameplay_pass_requires_receipts(self) -> None:
        campaign, target = documents()
        runtime = campaign["missions"][1]["runtime"]
        runtime["gameplay_status"] = "pass"
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "route path is missing"):
            MODULE.validate_campaign(campaign, target)

    def test_legacy_mission01_receipts_are_not_promoted(self) -> None:
        campaign, target = documents()
        runtime = campaign["missions"][0]["runtime"]
        runtime.update({
            "gameplay_status": "pass",
            "control_observed": True,
            "route": "recompilation/ace-combat-6-retail/routes/mission01-qualified-96.steps",
            "gate_receipt": "artifacts/retail-us-gameplay-final-compose-20260827/RESULT.json",
            "audit_receipt": "recompilation/ace-combat-6-retail/build/ntsc-uj/gameplay-validation.json",
        })
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "gate is not capture-ready"):
            MODULE.validate_campaign(campaign, target)

    def test_release_gate_rejects_current_partial_manifest(self) -> None:
        campaign, target = documents()
        with self.assertRaisesRegex(MODULE.CampaignValidationError, "release contract"):
            MODULE.validate_campaign(campaign, target, require_release=True)


if __name__ == "__main__":
    unittest.main()
