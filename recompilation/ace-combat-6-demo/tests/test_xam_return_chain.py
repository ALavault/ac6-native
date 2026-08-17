from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
CODEGEN = PROJECT / "build-codegen-on" / "codegen"
XEX = WORKSPACE / "demo-game-file" / "extracted" / "stfs-root" / "Default.xex"
BASEFILE = CODEGEN / "xex-basefile.bin"
GENERATED = CODEGEN / "generated"
MAPPER = PROJECT / "tools" / "map_xam_return_chain.py"
RUNNER = PROJECT / "tools" / "run_xam_return_chain_ab.sh"
sys.path.insert(0, str(PROJECT / "tools"))
from map_generated_guest_load_sites import MappingError, _site_kind  # noqa: E402


def valid_trace(*, lr: str = "0x00000000", exclusive_address: str = "0x829D15BC") -> str:
    return "\n".join([
        "AC6_XAM_RETURN_CHAIN_ARM caller_lr=0x822F616C tick=252 thread=1 user=0 flags=0x00000000 output=0x829D153C result=0x00000000 state16=000102030405060708090A0B0C0D0E0F",
        f"AC6_XAM_RETURN_CHAIN_ACCESS kind=load16 address=0x829D1584 size=2 value_be=0x1234 tick=252 thread=1 lr={lr} function=__imp__sub_822F6008 generated_line=4187",
        f"AC6_XAM_RETURN_CHAIN_ACCESS kind=store32 address=0x829D1558 size=4 value_be=0x12345678 tick=252 thread=1 lr={lr} function=__imp__sub_822F6008 generated_line=4189",
        f"AC6_XAM_RETURN_CHAIN_ACCESS kind=store32 address={exclusive_address} size=4 value_be=0x12345678 tick=252 thread=1 lr={lr} function=__imp__sub_822F5E58 generated_line=3948",
        "AC6_XAM_RETURN_CHAIN_STOP reason=qualified_store_exclusive accesses=3",
        "",
    ])


class XamReturnChainMapperTests(unittest.TestCase):
    def setUp(self) -> None:
        os.environ.setdefault("TMPDIR", "/fastdata/lavaulta/tmp")
        for path in (XEX, BASEFILE, GENERATED / "ppc_func_mapping.cpp",
                     GENERATED / "ppc_recomp.39.cpp", CODEGEN / "manifest.json",
                     RUNNER):
            if not path.is_file() or path.is_symlink():
                self.skipTest(f"qualified codegen fixture unavailable: {path}")

    def run_mapper(self, trace: str, *, manifest: Path | None = None,
                   expect: str = "qualified_store_exclusive") -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory(prefix="xam-map-", dir=os.environ["TMPDIR"]) as directory:
            root = Path(directory)
            trace_path = root / "trace.log"
            output_path = root / "map.json"
            trace_path.write_text(trace, encoding="utf-8")
            command = [sys.executable, str(MAPPER), "--trace", str(trace_path),
                       "--basefile", str(BASEFILE), "--xex", str(XEX),
                       "--generated-dir", str(GENERATED), "--expect", expect,
                       "--json", str(output_path)]
            if manifest is not None:
                command.extend(["--manifest", str(manifest)])
            completed = subprocess.run(command, text=True, capture_output=True)
            if completed.returncode == 0:
                self.result = json.loads(output_path.read_text(encoding="utf-8"))
            return completed

    def test_bounded_no_exclusive_route_is_explicit(self) -> None:
        trace = valid_trace().replace(
            "kind=store32 address=0x829D15BC size=4 value_be=0x12345678 "
            "tick=252 thread=1 lr=0x00000000 function=__imp__sub_822F5E58 generated_line=3948",
            "kind=load32 address=0x829D15BC size=4 value_be=0x00000000 "
            "tick=252 thread=1 lr=0x00000000 function=__imp__sub_822F5E58 generated_line=3953",
        ).replace("reason=qualified_store_exclusive", "reason=bound")
        completed = self.run_mapper(trace, expect="no_exclusive")
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.result["expectation"], "no_exclusive")
        self.assertEqual(self.result["stop"]["reason"], "bound")

    def test_three_allowlisted_sites_bytes_and_hashes(self) -> None:
        completed = self.run_mapper(valid_trace(lr="0xDEADBEEF"))
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.result["rows"], 3)
        self.assertEqual(self.result["target"]["xex_sha256"],
                         "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8")
        self.assertEqual(self.result["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual([row["guest_pc"] for row in self.result["sites"]],
                         ["0x822F601C", "0x822F6020", "0x822F5EA0"])
        self.assertEqual([row["instruction_bytes"] for row in self.result["sites"]],
                         ["a1 7f 00 48", "91 7f 00 1c", "90 7f 00 80"])
        self.assertTrue(self.result["policy"]["lr_is_not_pc"])

    def test_non_allowlisted_line_and_exclusive_address_refuse(self) -> None:
        bad_line = valid_trace().replace("generated_line=4189", "generated_line=4190")
        completed = self.run_mapper(bad_line)
        self.assertNotEqual(completed.returncode, 0)

    def test_final_qualified_store_contract_is_strict(self) -> None:
        inverses = (
            valid_trace().replace("kind=store32 address=0x829D15BC size=4 value_be=0x12345678",
                                 "kind=store32 address=0x829D15BC size=1 value_be=0x12"),
            valid_trace().replace("kind=store32 address=0x829D15BC",
                                 "kind=load32 address=0x829D15BC"),
            valid_trace().replace("generated_line=3948", "generated_line=3949"),
            valid_trace().replace("reason=qualified_store_exclusive",
                                 "reason=bound"),
            valid_trace().replace(
                "AC6_XAM_RETURN_CHAIN_STOP reason=qualified_store_exclusive",
                "AC6_XAM_RETURN_CHAIN_ACCESS kind=load8 address=0x829D1500 "
                "size=1 value_be=0x00 tick=252 thread=1 lr=0x0 "
                "function=__imp__sub_822F6008 generated_line=4187\n"
                "AC6_XAM_RETURN_CHAIN_STOP reason=qualified_store_exclusive"),
        )
        for trace in inverses:
            self.assertNotEqual(self.run_mapper(trace).returncode, 0)

    def test_demo_identity_and_atomic_refusal_contract(self) -> None:
        source = (PROJECT / "src/guest_bridge/xam_return_chain_trace.hpp").read_text()
        self.assertIn('"site_not_allowlisted"', source)
        completed = self.run_mapper(valid_trace())
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.result["ghidra_owner"]["project"], "ace-combat-6-demo")

    def test_fresh_process_xam_only_and_off_fast_path(self) -> None:
        executable = PROJECT / "build" / "ac6-demo-xam-return-chain-header-tests"
        if not executable.is_file():
            self.skipTest(f"header probe unavailable: {executable}")
        base = {key: value for key, value in os.environ.items()
                if not key.startswith("AC6_DEMO_WATCH_")}
        enabled = dict(base, AC6_XAM_PROBE_ONLY="1",
                       AC6_DEMO_WATCH_XAM_RETURN_CHAIN="1")
        result = subprocess.run([str(executable)], env=enabled, text=True,
                                capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("function=__imp__sub_822F5E58 generated_line=3948", result.stderr)
        self.assertIn("reason=qualified_store_exclusive", result.stderr)
        disabled = dict(base, AC6_XAM_PROBE_ONLY="1")
        result = subprocess.run([str(executable)], env=disabled, text=True,
                                capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("AC6_XAM_RETURN_CHAIN_ARM", result.stderr)
        bad_address = valid_trace(exclusive_address="0x829D15BD")
        completed = self.run_mapper(bad_address)
        self.assertNotEqual(completed.returncode, 0)

    def test_manifest_must_be_adjacent(self) -> None:
        completed = self.run_mapper(valid_trace(), manifest=PROJECT / "README.md")
        self.assertNotEqual(completed.returncode, 0)

    def test_generated_site_ambiguity_is_rejected(self) -> None:
        with self.assertRaises(MappingError):
            _site_kind("PPC_LOAD_U32(a); PPC_STORE_U32(b);", "load32")

    def test_explicit_refusal_is_fail_closed(self) -> None:
        trace = valid_trace().replace(
            "AC6_XAM_RETURN_CHAIN_STOP",
            "AC6_XAM_RETURN_CHAIN_ACCESS_REFUSED kind=stwcx address=0x829D15BC size=4 value_be=0x00000000 success=0 tick=252 thread=1 lr=0x00000000 function=f generated_line=1 site_pc=0x00000000 reason=site_not_allowlisted\nAC6_XAM_RETURN_CHAIN_STOP",
        )
        completed = self.run_mapper(trace)
        self.assertNotEqual(completed.returncode, 0)

    def test_runner_fixed_parser_and_reused_lifecycle_contract(self) -> None:
        source = RUNNER.read_text(encoding="utf-8")
        self.assertIn("if (( $# != 0 )); then", source)
        self.assertNotIn("AC6_DEMO_LIFECYCLE", source)
        self.assertIn("single owned supervisor reused by both fresh probes", source)
        self.assertIn('"backend": "vulkan"', source)
        self.assertIn('"audio_driver": "dummy"', source)
        self.assertIn('"xma": "qualified runtime flags"', source)
        self.assertIn('--expect "$expectation"', source)
        self.assertIn('"route_expectations"', source)
        completed = subprocess.run([str(RUNNER), "unexpected"],
                                   text=True, capture_output=True)
        self.assertEqual(completed.returncode, 2)


if __name__ == "__main__":
    unittest.main()
