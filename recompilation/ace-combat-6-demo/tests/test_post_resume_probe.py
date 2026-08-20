"""Focused source/mapper checks for the bounded post-resume probe."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
TRACE = PROJECT / "src/guest_bridge/event_post_set_trace.hpp"
BRIDGE = PROJECT / "src/guest_bridge.cpp"
ATOMIC = PROJECT / "src/guest_bridge/graphics_mmio_cpu.hpp"
ADAPTER = PROJECT / "tools/ppc_context_adapter.h"
VECTOR = PROJECT / "src/guest_bridge/vector_read_trace.cpp"
DISPATCH = PROJECT / "src/guest_bridge/kernel_objects_dispatch.hpp"
DISPATCH_ORIGINAL = PROJECT / "src/guest_bridge/kernel_objects_dispatch_original.hpp"
MAPPER_PATH = PROJECT / "tools/map_generated_guest_load_sites.py"


def load_mapper():
    spec = importlib.util.spec_from_file_location("ac6_post_resume_mapper", MAPPER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot import post-resume mapper")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_mapper_fixture(mapper, root: Path, function: str, address: int,
                         instruction_bytes: bytes):
    generated = root / "generated"
    generated.mkdir()
    alias = function.removeprefix("__imp__")
    (generated / "ppc_recomp.0.cpp").write_text(
        f"PPC_FUNC_IMPL({function}) {{\n"
        "  // lwz r3,0(r1)\n"
        "  PPC_LOAD_U32(0);\n"
        "}\n",
        encoding="utf-8",
    )
    (generated / "ppc_func_mapping.cpp").write_text(
        "PPCFuncMapping PPCFuncMappings[] = {\n"
        f"  {{ 0x{address:08X}, {alias} }},\n}};\n",
        encoding="utf-8",
    )
    base = bytearray((index & 0xFF for index in range(address - 0x82000000 + 4)))
    offset = address - 0x82000000
    base[offset:offset + 4] = instruction_bytes
    basefile = root / "base.bin"
    basefile.write_bytes(base)
    mapper.EXPECTED_BASEFILE_SHA256 = hashlib.sha256(base).hexdigest()
    xex = root / "default.xex"
    xex.write_bytes(b"controlled fixture XEX")
    mapper.EXPECTED_XEX_SHA256 = hashlib.sha256(xex.read_bytes()).hexdigest()
    (root / "manifest.json").write_text(json.dumps({
        "schema": "ac6-demo-codegen-manifest/v2",
        "target_id": "ac6-demo-xbox360-pal",
        "xex_sha256": mapper.EXPECTED_XEX_SHA256,
        "xenonrecomp_commit": mapper.EXPECTED_XENONRECOMP_COMMIT,
    }), encoding="utf-8")
    log = root / "trace.log"
    log.write_text(
        "AC6_POST_RESUME_INSTRUCTION_HANDOFF resume_pc=0x821A69CC "
        "callsite=0x821A69C8 tick=1 thread=1 "
        "signal_handle=0xE0000048 wait_handle=0xE000004C\n"
        f"AC6_POST_RESUME_ACCESS kind=load32 address=0x00000010 size=4 "
        f"value=0x01020304 tick=2 thread=1 lr=0xDEADBEEF "
        f"function={function} generated_line=3\n",
        encoding="utf-8",
    )
    return type("Args", (), {"basefile": basefile, "xex": xex,
                              "generated_dir": generated, "log": log})


class PostResumeProbeTests(unittest.TestCase):
    def test_disabled_and_exact_filter_are_source_guarded(self):
        trace = TRACE.read_text(encoding="utf-8")
        self.assertIn('AC6_DEMO_WATCH_POST_RESUME_ACCESS', trace)
        self.assertNotIn('AC6_DEMO_WATCH_EVENT_POST_SET', trace)
        self.assertIn('std::string_view{value} == "1"', trace)
        self.assertIn('wait_handle != 0xE000004CU', trace)
        self.assertIn('signal_handle != 0xE0000048U', trace)
        self.assertIn('resume_pc != 0x821A69CCU', trace)
        self.assertIn('thread != 1U', trace)
        self.assertIn("capture_attempts", trace)
        self.assertIn("post_resume_watch_enabled_fast", trace)
        self.assertEqual(trace.count('std::getenv("AC6_DEMO_WATCH_POST_RESUME_ACCESS")'), 1)

    def test_one_shot_handoff_and_post_access_contract(self):
        trace = TRACE.read_text(encoding="utf-8")
        dispatch = DISPATCH_ORIGINAL.read_text(encoding="utf-8")
        self.assertEqual(trace.count("AC6_POST_RESUME_INSTRUCTION_HANDOFF"), 1)
        self.assertEqual(trace.count("AC6_POST_RESUME_ACCESS kind="), 2)
        self.assertIn("compare_exchange_strong", trace)
        self.assertIn("resume_pc - 4U", trace)
        self.assertEqual(dispatch.count("arm_post_resume_access("), 2)
        self.assertNotIn("arm_event_post_set", dispatch)
        self.assertIn('#include "kernel_objects_dispatch_original.hpp"',
                      DISPATCH.read_text(encoding="utf-8"))

    def test_fixed_width_scalar_vector_and_atomic_routes(self):
        trace = TRACE.read_text(encoding="utf-8")
        bridge = BRIDGE.read_text(encoding="utf-8")
        vector = VECTOR.read_text(encoding="utf-8")
        atomic = ATOMIC.read_text(encoding="utf-8")
        adapter = ADAPTER.read_text(encoding="utf-8")
        self.assertIn('"value=0x%0*llX', trace)
        self.assertIn('std::array<char, 33U>', trace)
        self.assertIn('"store128"', bridge)
        self.assertIn('"load128"', vector)
        for name in ("lwarx", "stwcx", "ldarx", "stdcx"):
            self.assertIn(f'"{name}"', atomic)
        self.assertIn('"atomic_failed"', atomic)
        self.assertIn('"atomic_success"', atomic)
        self.assertIn("AC6_PPC_RECORD_POST_RESUME_VECTOR_READ", adapter)
        self.assertIn("AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED", adapter)
        self.assertIn("std::memory_order_relaxed", adapter)

    def test_codegen_copy_and_ctest_registration_are_explicit(self):
        build_demo = (PROJECT / "tools/build_demo.py").read_text(encoding="utf-8")
        cmake = (PROJECT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('shutil.copy2(context, adapter)', build_demo)
        self.assertIn('adapter.read_bytes() != context.read_bytes()', build_demo)
        self.assertIn('tools/ppc_context_adapter.h', cmake)
        self.assertIn('ac6-demo-post-resume-header-tests', cmake)
        self.assertIn('-p test_post_resume_probe.py', cmake)

    def test_mapper_requires_one_handoff_and_one_access(self):
        mapper = load_mapper()
        with tempfile.TemporaryDirectory(dir="/fastdata/lavaulta/tmp") as directory:
            root = Path(directory)
            generated = root / "generated"
            generated.mkdir()
            source = generated / "ppc_recomp.0.cpp"
            source.write_text(
                "PPC_FUNC_IMPL(__imp__sub_82000000) {\n"
                "  // lwz r3,0(r1)\n"
                "  PPC_LOAD_U32(0);\n"
                "}\n",
                encoding="utf-8",
            )
            base = bytes(range(256)) * 128
            basefile = root / "base.bin"
            basefile.write_bytes(base)
            mapper.EXPECTED_BASEFILE_SHA256 = hashlib.sha256(base).hexdigest()
            xex = root / "default.xex"
            xex.write_bytes(b"controlled fixture XEX")
            mapper.EXPECTED_XEX_SHA256 = hashlib.sha256(xex.read_bytes()).hexdigest()
            (generated / "ppc_func_mapping.cpp").write_text(
                "PPCFuncMapping PPCFuncMappings[] = {\n"
                "  { 0x82000000, sub_82000000 },\n};\n",
                encoding="utf-8",
            )
            (root / "manifest.json").write_text(json.dumps({
                "schema": "ac6-demo-codegen-manifest/v2",
                "target_id": "ac6-demo-xbox360-pal",
                "xex_sha256": mapper.EXPECTED_XEX_SHA256,
                "xenonrecomp_commit": mapper.EXPECTED_XENONRECOMP_COMMIT,
            }), encoding="utf-8")
            log = root / "trace.log"
            log.write_text(
                "AC6_POST_RESUME_INSTRUCTION_HANDOFF resume_pc=0x821A69CC "
                "callsite=0x821A69C8 tick=1 thread=1 "
                "signal_handle=0xE0000048 wait_handle=0xE000004C\n"
                "AC6_POST_RESUME_ACCESS kind=load32 address=0x00000010 "
                "size=4 value=0x01020304 tick=2 thread=1 lr=0x821A69CC "
                "function=__imp__sub_82000000 generated_line=3\n",
                encoding="utf-8",
            )
            args = type("Args", (), {"basefile": basefile, "xex": xex,
                                      "generated_dir": generated,
                                      "log": log})
            result = mapper.build(args)
            self.assertEqual(result["mapped_rows"], 1)
            self.assertEqual(result["access"]["instruction_bytes"], "00 01 02 03")
            self.assertEqual(result["target"]["xex_sha256"],
                             hashlib.sha256(b"controlled fixture XEX").hexdigest())

    def test_mapper_rejects_ambiguous_function_and_wrong_bytes(self):
        mapper = load_mapper()
        with tempfile.TemporaryDirectory(dir="/fastdata/lavaulta/tmp") as directory:
            root = Path(directory)
            generated = root / "generated"
            generated.mkdir()
            body = (
                "PPC_FUNC_IMPL(__imp__sub_82000000) {\n"
                "  // lwz r3,0(r1)\n"
                "  PPC_LOAD_U32(0);\n"
                "}\n"
            )
            (generated / "ppc_recomp.0.cpp").write_text(body, encoding="utf-8")
            (generated / "ppc_recomp.1.cpp").write_text(body, encoding="utf-8")
            base = bytes(range(256)) * 128
            basefile = root / "base.bin"
            basefile.write_bytes(base)
            mapper.EXPECTED_BASEFILE_SHA256 = hashlib.sha256(base).hexdigest()
            xex = root / "default.xex"
            xex.write_bytes(b"controlled fixture XEX")
            mapper.EXPECTED_XEX_SHA256 = hashlib.sha256(xex.read_bytes()).hexdigest()
            with self.assertRaises(mapper.MappingError):
                mapper.load_functions(generated)
            basefile.write_bytes(base[:-1] + bytes([base[-1] ^ 1]))
            log = root / "trace.log"
            log.write_text("AC6_POST_RESUME_INSTRUCTION_HANDOFF broken\n",
                           encoding="utf-8")
            # A changed PAL file is rejected before any source join is trusted.
            with self.assertRaises(mapper.MappingError):
                mapper.build(type("Args", (), {"basefile": basefile,
                                                 "xex": xex,
                                                 "generated_dir": generated,
                                                 "log": log}))

    def test_mapper_rejects_missing_wrong_and_truncated_xex(self):
        mapper = load_mapper()
        with tempfile.TemporaryDirectory(dir="/fastdata/lavaulta/tmp") as directory:
            root = Path(directory)
            basefile = root / "base.bin"
            base = bytes(range(256)) * 128
            basefile.write_bytes(base)
            mapper.EXPECTED_BASEFILE_SHA256 = hashlib.sha256(base).hexdigest()
            missing = root / "missing.xex"
            args = type("Args", (), {"basefile": basefile, "xex": missing,
                                      "generated_dir": root / "generated",
                                      "log": root / "trace.log"})
            with self.assertRaises(mapper.MappingError):
                mapper.build(args)
            xex = root / "default.xex"
            xex.write_bytes(b"wrong")
            with self.assertRaises(mapper.MappingError):
                mapper.build(type("Args", (), {"basefile": basefile, "xex": xex,
                                                 "generated_dir": root / "generated",
                                                 "log": root / "trace.log"}))
            xex.write_bytes(b"wrong-truncated")
            with self.assertRaises(mapper.MappingError):
                mapper.build(type("Args", (), {"basefile": basefile, "xex": xex,
                                                 "generated_dir": root / "generated",
                                                 "log": root / "trace.log"}))

    def test_mapper_resolves_generic_gpr_fpr_vmx_helpers_from_mapping(self):
        mapper = load_mapper()
        fixtures = (
            ("__imp____restgprlr_27", 0x82327154, bytes.fromhex("eb61ffd0")),
            ("__imp____savefpr_14", 0x82000010, bytes.fromhex("11223344")),
            ("__imp____savevmx_65", 0x82000020, bytes.fromhex("aabbccdd")),
        )
        with tempfile.TemporaryDirectory(dir="/fastdata/lavaulta/tmp") as directory:
            for index, (function, address, expected) in enumerate(fixtures):
                with self.subTest(function=function):
                    root = Path(directory) / str(index)
                    root.mkdir()
                    args = write_mapper_fixture(mapper, root, function, address, expected)
                    result = mapper.build(args)
                    self.assertEqual(result["access"]["guest_pc"], f"0x{address:08X}")
                    self.assertEqual(result["access"]["instruction_bytes"],
                                     " ".join(f"{byte:02x}" for byte in expected))

    def test_mapper_rejects_stale_manifest_duplicate_and_missing_alias(self):
        mapper = load_mapper()
        with tempfile.TemporaryDirectory(dir="/fastdata/lavaulta/tmp") as directory:
            root = Path(directory)
            args = write_mapper_fixture(mapper, root, "__imp____restgprlr_27",
                                         0x82327154, bytes.fromhex("eb61ffd0"))
            mapping = args.generated_dir / "ppc_func_mapping.cpp"
            mapping.write_text(
                "PPCFuncMapping PPCFuncMappings[] = {\n"
                "  { 0x82327154, __restgprlr_27 },\n"
                "  { 0x82327158, __restgprlr_27 },\n};\n",
                encoding="utf-8",
            )
            with self.assertRaises(mapper.MappingError):
                mapper.build(args)
            mapping.write_text(
                "PPCFuncMapping PPCFuncMappings[] = {\n"
                "  { 0x82327154, __different_helper },\n};\n",
                encoding="utf-8",
            )
            with self.assertRaises(mapper.MappingError):
                mapper.build(args)
            (root / "manifest.json").write_text(json.dumps({
                "schema": "ac6-demo-codegen-manifest/v2",
                "target_id": "ac6-demo-xbox360-pal",
                "xex_sha256": mapper.EXPECTED_XEX_SHA256,
                "xenonrecomp_commit": "stale",
            }), encoding="utf-8")
            with self.assertRaises(mapper.MappingError):
                mapper.build(args)
if __name__ == "__main__":
    unittest.main()
