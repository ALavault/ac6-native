from __future__ import annotations

import importlib.util
import copy
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/prepare.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_prepare", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class PrepareTests(unittest.TestCase):
    def test_profiles_are_explicit_and_native_is_not_oracle(self) -> None:
        self.assertEqual(MODULE.PROFILES, ("rexglue-oracle", "native"))

    def test_qualify_file_accepts_exact_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "input.bin"
            path.write_bytes(b"qualified")
            expected = {"size": 9, "sha256": MODULE.file_sha256(path)}
            result = MODULE.qualify_file(path, expected, "fixture")
            self.assertEqual(result["size"], 9)
            self.assertEqual(result["sha256"], expected["sha256"])

    def test_qualify_file_rejects_wrong_size_before_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "input.bin"
            path.write_bytes(b"x")
            with self.assertRaisesRegex(MODULE.PreparationError, "size mismatch"):
                MODULE.qualify_file(path, {"size": 2, "sha256": "0" * 64}, "fixture")

    def test_target_definitions_keep_pal_blocked_behind_us(self) -> None:
        product = SCRIPT.parents[1]
        us = json.loads((product / "targets/ntsc-uj.json").read_text())
        pal = json.loads((product / "targets/pal.json").read_text())
        self.assertEqual(us["qualification_state"], "ready")
        self.assertEqual(pal["qualification_state"], "blocked-until-ntsc-uj-gameplay-pass")
        self.assertEqual(pal["ghidra"]["project"], "ghidra-projects/ace-combat-6")
        self.assertEqual(us["ghidra"]["project"], "ghidra-projects/ac6-us")
        self.assertEqual(us["ghidra"]["language"], "PowerPC:BE:64:Xenon")

    def test_ntsc_hook_map_is_exact_and_read_only(self) -> None:
        product = SCRIPT.parents[1]
        definition = json.loads((product / "targets/ntsc-uj.json").read_text())
        hook_map = json.loads((product / definition["hook_map"]["path"]).read_text())
        self.assertEqual(MODULE.file_sha256(product / definition["hook_map"]["path"]),
                         definition["hook_map"]["sha256"])
        self.assertEqual(
            [hook["address"] for hook in hook_map["hooks"]],
            [
                "0x821C3800", "0x821C5268", "0x821C5708", "0x8218F4F0",
                "0x820943B0", "0x82158D90", "0x82196590", "0x821A6400",
                "0x82267160", "0x82267258", "0x822ED310", "0x82256490",
                "0x8226C068",
            ],
        )
        self.assertTrue(all(hook["decision"] in {
            "include-read-only-observable",
            "include-bounded-call-presence-observable",
        } for hook in hook_map["hooks"]))
        overlay = (product / "overlay/ac6_route_sync_ntsc_uj.cpp").read_text()
        self.assertNotIn("PPC_STORE", overlay)
        self.assertEqual(overlay.count("__imp__rex_sub_"), 26)

    def test_hook_map_from_another_target_is_rejected(self) -> None:
        product = SCRIPT.parents[1]
        definition = json.loads((product / "targets/ntsc-uj.json").read_text())
        hook_map = json.loads((product / definition["hook_map"]["path"]).read_text())
        wrong = copy.deepcopy(hook_map)
        wrong["target"] = "pal"
        with self.assertRaisesRegex(MODULE.PreparationError, "identity mismatch"):
            MODULE.validate_hook_map_identity(
                wrong, definition, {"sha256": definition["xex"]["sha256"]}, "ntsc-uj"
            )


if __name__ == "__main__":
    unittest.main()
