import hashlib
import importlib.util
import json
import tempfile
import tomllib
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
SPEC = importlib.util.spec_from_file_location("ac6_demo_build", PROJECT / "tools/build_demo.py")
assert SPEC is not None and SPEC.loader is not None
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class BuildPolicyTests(unittest.TestCase):
    def _write_manifest(self, **overrides):
        document = {
            "schema": BUILD.GHIDRA_SCHEMA,
            "target_id": BUILD.GHIDRA_TARGET_ID,
            "project_path": BUILD.GHIDRA_PROJECT_PATH,
            "project": "ace-combat-6-demo",
            "program": "Default.xex",
            "module": "Default.xex",
            "language": BUILD.GHIDRA_LANGUAGE,
            "xex_sha256": BUILD.EXPECTED_XEX,
            "image_base": "0x82000000",
            "entry_point": "0x821A7160",
            "text": {"address": "0x82090000", "size": "0x002E67C4",
                     "byte_sha256": "1" * 64},
            "pdata": {"address": "0x82077200", "size": "0x00010438",
                      "byte_sha256": "2" * 64},
            "ghidra": {
                "version": BUILD.GHIDRA_VERSION,
                "loader": "XEX Loader by Warranty Voider",
                "language": BUILD.GHIDRA_LANGUAGE,
                "compiler_spec": "default",
            },
            "import_journal": {
                "schema": "ac6-demo-ghidra-import-journal/v1",
                "sha256": "3" * 64,
            },
            "script_sha256": {
                "tools/import_ghidra_demo.py": "4" * 64,
                "tools/ghidra/ExportQualifiedDemoChunks.java": "5" * 64,
                "tools/ghidra/ValidateDemoBoundarySet.java": "7" * 64,
                "config/confirmed-chunks.toml": "6" * 64,
            },
            "chunks": [{"address": "0x82100000", "size": "0x20",
                        "byte_sha256": "0" * 64}],
        }
        document.update(overrides)
        temporary = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
        json.dump(document, temporary)
        temporary.close()
        self.addCleanup(lambda: Path(temporary.name).unlink(missing_ok=True))
        return Path(temporary.name)

    def test_only_target_qualified_json_chunks_are_accepted(self):
        chunks = BUILD.confirmed_functions(self._write_manifest())
        self.assertIn((0x82100000, 0x20), chunks)
        self.assertIn((0x822D8CF8, 0x138), chunks)

    def test_wrong_xex_is_rejected(self):
        with self.assertRaises(BUILD.BuildError):
            BUILD.confirmed_functions(self._write_manifest(xex_sha256="wrong"))

    def test_wrong_language_is_rejected(self):
        with self.assertRaises(BUILD.BuildError):
            BUILD.confirmed_functions(self._write_manifest(language="PowerPC:BE:64:A2ALT-32addr"))

    def test_historical_ghidra_version_is_rejected(self):
        metadata = {
            "version": "10.4",
            "loader": "XEX Loader by Warranty Voider",
            "language": BUILD.GHIDRA_LANGUAGE,
            "compiler_spec": "default",
        }
        with self.assertRaises(BUILD.BuildError):
            BUILD.confirmed_functions(self._write_manifest(ghidra=metadata))

    def test_wrong_project_path_is_rejected(self):
        with self.assertRaises(BUILD.BuildError):
            BUILD.confirmed_functions(self._write_manifest(project_path="historical-project"))

    def test_playable_gate_stays_false_until_all_six_lanes_close(self):
        gate = json.loads((PROJECT / "config/demo-playable-gate-v1.json").read_text())
        self.assertEqual(gate["schema"], "demo-playable-gate-v1")
        self.assertFalse(gate["supported"])
        self.assertEqual(len(gate["required_lanes"]), 6)
        self.assertTrue(all(not lane["closed"]
                            for lane in gate["required_lanes"].values()))

    def test_dynamic_evidence_schemas_are_distinct_and_demo_qualified(self):
        schema_root = PROJECT / "config/schemas"
        expected = {
            "ac6-demo-static-decomp-atlas-v1.schema.json":
                "ac6-demo-static-decomp-atlas/v1",
            "ac6-demo-milestone-digest-v1.schema.json":
                "ac6-demo-milestone-digest/v1",
            "ac6-demo-reachability-atlas-v1.schema.json":
                "ac6-demo-reachability-atlas/v1",
            "ac6-demo-corridor-capsule-v1.schema.json":
                "ac6-demo-corridor-capsule/v1",
        }
        documents = {
            name: json.loads((schema_root / name).read_text())
            for name in expected
        }
        self.assertEqual(
            {document["$id"] for document in documents.values()},
            set(expected.values()),
        )
        for name, schema_id in expected.items():
            document = documents[name]
            self.assertEqual(document["$id"], schema_id)
            self.assertFalse(document["additionalProperties"])
            serialized = json.dumps(document, sort_keys=True)
            self.assertIn(BUILD.EXPECTED_XEX, serialized)
            self.assertNotIn("AC6RTPLY", serialized)
        milestone = documents[
            "ac6-demo-milestone-digest-v1.schema.json"]
        self.assertIn("guest_transition", milestone["required"])
        self.assertIn("xenos_readback", milestone["required"])
        static_atlas = documents[
            "ac6-demo-static-decomp-atlas-v1.schema.json"]
        self.assertEqual(
            static_atlas["properties"]["coverage"]["properties"]
                        ["function_count"]["const"],
            12876,
        )
        roles = static_atlas["$defs"]["function"]["properties"]["role"]["enum"]
        self.assertEqual(roles, [
            "bootstrap", "kernel/scheduler", "XAM/input/content", "VFS",
            "Xenos", "XMA/audio", "frontend", "mission/scenario",
            "flight/combat/objectives", "unknown",
        ])

    def test_legacy_tsv_is_rejected(self):
        temporary = tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False)
        temporary.write("82090000\t17\tFunction_82090000\n")
        temporary.close()
        path = Path(temporary.name)
        self.addCleanup(lambda: path.unlink(missing_ok=True))
        with self.assertRaises(BUILD.BuildError):
            BUILD.confirmed_functions(path)

    def test_import_stubs_preserve_identity_and_trap(self):
        with tempfile.TemporaryDirectory() as directory:
            source = BUILD.write_import_stubs(Path(directory), [
                {"module": "xam.xex", "ordinal": 490, "name": "XamAlloc"},
                {"module": "xboxkrnl.exe", "ordinal": 327, "name": "RtlUnwind"},
            ])
            text = source.read_text()
            self.assertIn("__imp__XamAlloc", text)
            self.assertNotIn('extern "C" void __imp__XamAlloc', text)
            self.assertIn('"xam.xex"', text)
            self.assertIn("static_cast<std::uint16_t>(490)", text)
            self.assertIn("AC6_PPC_IMPORT_DISPATCH", text)
            self.assertIn("AC6_PPC_IMPORT_TRAP", text)
            self.assertIn("ac6demo::RuntimeTrap", text)

    def test_import_stub_symbol_collision_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(BUILD.BuildError):
                BUILD.write_import_stubs(Path(directory), [
                    {"module": "one", "ordinal": 1, "name": "Same"},
                    {"module": "two", "ordinal": 2, "name": "Same"},
                ])

    def test_import_stub_uses_xenonrecomp_symbol_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            source = BUILD.write_import_stubs(
                Path(directory),
                [{"module": "(min", "ordinal": 479,
                  "name": "KiApcNormalRoutineNop_0"}],
                {"__imp__KiApcNormalRoutineNop"})
            text = source.read_text()
            self.assertIn("void __imp__KiApcNormalRoutineNop(", text)
            self.assertNotIn("void __imp__KiApcNormalRoutineNop_0(", text)

    def test_variable_import_is_recorded_and_not_called(self):
        with tempfile.TemporaryDirectory() as directory:
            source = BUILD.write_import_stubs(
                Path(directory),
                [{"module": "(min", "ordinal": 27,
                  "name": "ExThreadObjectType"}],
                set(),
                {"ExThreadObjectType"})
            text = source.read_text()
            self.assertIn("AC6_PPC_DATA_IMPORT_TRAP", text)
            self.assertIn('"ExThreadObjectType"', text)
            self.assertIn("true},", text)
            self.assertNotIn("void __imp__ExThreadObjectType(", text)

    def test_unclassified_import_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(BUILD.BuildError):
                BUILD.write_import_stubs(
                    Path(directory),
                    [{"module": "xboxkrnl.exe", "ordinal": 1,
                      "name": "Missing"}],
                    {"__imp__Different"},
                    {"KnownData"})

    def test_variable_exports_are_read_from_pinned_table(self):
        with tempfile.TemporaryDirectory() as directory:
            table = Path(directory) / "XenonUtils/xbox/xboxkrnl_table.inc"
            table.parent.mkdir(parents=True)
            table.write_text(
                "XE_EXPORT(xboxkrnl, 0x0000001B, ExThreadObjectType, kVariable),\n"
                "XE_EXPORT(xboxkrnl, 0x0000001C, ExTimerObjectType, kFunction),\n"
            )
            names = BUILD.variable_import_names(Path(directory))
            self.assertEqual(names, {"ExThreadObjectType"})

    def test_import_listing_keeps_module_across_version_line(self):
        records = BUILD.parse_import_listing(
            "# xam.xex v2.0.5632.0\n"
            "# (min v2.0.1861.0, 1 imports)\n"
            "    1) XamAlloc\n"
            "# xboxkrnl.exe v2.0.5632.0\n"
            "# (min v2.0.1861.0, 1 imports)\n"
            "    2) ExAllocatePool\n"
        )
        self.assertEqual(
            [(record["module"], record["name"]) for record in records],
            [("xam.xex", "XamAlloc"), ("xboxkrnl.exe", "ExAllocatePool")],
        )


class BoundaryEvidenceTests(unittest.TestCase):
    def test_boundary_evidence_matches_durable_inputs(self):
        evidence_paths = sorted((WORKSPACE / "analysis/demo").glob(
            "ac6-demo-????????-boundary-evidence.json"
        ))
        config_path = PROJECT / "config/confirmed-chunks.toml"
        records = tomllib.loads(config_path.read_text())["function"]
        records_by_address = {item["address"]: item for item in records}
        dispatches = set()

        self.assertGreaterEqual(len(evidence_paths), 2)
        for path in evidence_paths:
            with self.subTest(path=path.name):
                evidence = json.loads(path.read_text())
                entry = int(evidence["static_chain"]["entry"], 0)
                record = records_by_address[entry]
                artifacts = evidence["artifacts"]
                self.assertEqual(evidence["schema"],
                                 "ac6-demo-boundary-closure-evidence/v1")
                self.assertEqual(evidence["lane"], "bridge")
                self.assertEqual(evidence["target_identity"]["xex_sha256"],
                                 BUILD.EXPECTED_XEX)
                self.assertEqual(record["byte_sha256"],
                                 evidence["static_chain"]["entry_byte_sha256"])
                self.assertEqual(
                    hashlib.sha256(bytes.fromhex(
                        evidence["static_chain"]["entry_bytes"]
                    )).hexdigest(),
                    evidence["static_chain"]["entry_byte_sha256"],
                )
                self.assertTrue(all(
                    isinstance(value, str) and len(value) == 64
                    for value in artifacts.values()
                ))
                for event in evidence["dynamic_events"]:
                    if event["event"] == "virtual_dispatch":
                        dispatches.add((event["lr"], event["vtable"],
                                        event["slot"], event["target"]))
        self.assertTrue(
            {
                ("0x82323F08", "0x8200654C", 11, "0x820D0FF8"),
                ("0x820DF8E4", "0x82006D8C", 6, "0x8222F2D0"),
                ("0x821042B0", "0x820092BC", 10, "0x821044D8"),
                ("0x82259D58", "0x8201130C", 10, "0x8216C940"),
            }.issubset(dispatches)
        )

    def test_xam_signin_capsule_matches_portable_contract(self):
        capsule = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-xam-signin-state-capsule.json").read_text())
        self.assertEqual(capsule["schema"],
                         "ac6-demo-behavioral-capsule/v1")
        self.assertEqual(capsule["lane"], "bridge")
        self.assertEqual(capsule["target_identity"]["xex_sha256"],
                         BUILD.EXPECTED_XEX)
        self.assertEqual(capsule["native_comparator"]["expected"],
                         [[0, 1], [1, 0], [2, 0], [3, 0]])
        self.assertEqual(capsule["native_comparator"]["rejected"], [4])
        self.assertEqual(capsule["confidence"]["stock_parity"], "unknown")

    def test_offline_recvfrom_capsule_stays_host_disconnected(self):
        capsule = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-offline-recvfrom-capsule.json").read_text())
        self.assertEqual(capsule["schema"],
                         "ac6-demo-behavioral-capsule/v1")
        self.assertEqual(capsule["lane"], "bridge")
        self.assertEqual(capsule["target_identity"]["xex_sha256"],
                         BUILD.EXPECTED_XEX)
        intervention = capsule["interventions"][0]
        self.assertFalse(intervention["host_network_access"])
        self.assertEqual(capsule["output_schema"]["return"], "signed -1")
        self.assertEqual(capsule["output_schema"]["last_error"],
                         "10038 WSAENOTSOCK")
        self.assertEqual(capsule["memory_effects"]["writes"], [])
        self.assertFalse(capsule["branch_coverage"]["host_network_path"])

    def test_controller_state_capsule_keeps_visual_gate_open(self):
        capsule = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-controller-state-capsule.json").read_text())
        self.assertEqual(capsule["schema"],
                         "ac6-demo-behavioral-capsule/v1")
        self.assertEqual(capsule["lane"], "bridge")
        self.assertEqual(capsule["target_identity"]["xex_sha256"],
                         BUILD.EXPECTED_XEX)
        comparator = capsule["native_comparator"]
        self.assertEqual(comparator["field_order"],
                         ["current", "pressed", "released", "previous"])
        self.assertEqual(comparator["neutral"], [0, 0, 0, 0])
        self.assertEqual(comparator["start"], [16, 16, 0, 16])
        self.assertEqual(comparator["normalized_neutral"], 0)
        self.assertEqual(comparator["normalized_start"], 0x400)
        self.assertEqual(comparator["logical_current_neutral"], 0)
        self.assertEqual(comparator["logical_pressed_neutral"], 0)
        self.assertEqual(comparator["logical_current_start"], 0x10)
        self.assertEqual(comparator["logical_pressed_start"], 0x10)
        self.assertTrue(capsule["branch_coverage"]["title_normalizer"])
        self.assertFalse(capsule["branch_coverage"]["menu_state_consumer"])
        sdk = capsule["sdk_cross_match"]
        self.assertEqual(sdk["state_size"], 16)
        self.assertEqual(sdk["buttons_offset"], 4)
        self.assertEqual(sdk["start_mask"], 0x10)
        self.assertTrue(capsule["validation"]["deterministic_replay"])
        self.assertFalse(capsule["validation"]["renderer_validation"])
        self.assertFalse(capsule["validation"]["menu_transition_validation"])
        evidence = json.loads((WORKSPACE / "analysis/demo" /
                               "ac6-demo-input-boundary-evidence.json").read_text())
        xrefs = evidence["static_input_cone"]["normalized_state_xrefs"]
        self.assertEqual(xrefs["classification"], "static_exact")
        self.assertEqual(xrefs["count"], 4)
        self.assertEqual(
            [item["site"] for item in xrefs["entries"]
             if item["access"] == "read"],
            ["0x821995E8"],
        )
        self.assertIn("not evidence", xrefs["menu_conclusion"])
        handoff = evidence["static_input_cone"]["logical_bitset_handoff"]
        self.assertEqual(handoff["start_binding"]["logical_mask"],
                         "0x00000010")
        self.assertEqual(handoff["addresses"]["pressed"], "0x82798488")
        consumers = evidence["static_input_cone"]["first_static_consumers"]
        self.assertEqual(
            [item["start_test"] for item in consumers],
            ["0x82170FCC", "0x82185210"],
        )
        self.assertTrue(all(item["runtime_reach"] == "unknown"
                            for item in consumers))

    def test_renderer_frontier_keeps_pixel_gate_open(self):
        evidence = json.loads((WORKSPACE / "analysis/demo" /
                               "ac6-demo-renderer-frontier-evidence.json").read_text())
        self.assertEqual(evidence["schema"],
                         "ac6-demo-renderer-frontier-evidence/v1")
        self.assertEqual(evidence["target"]["xex_sha256"],
                         BUILD.EXPECTED_XEX)
        claims = {claim["id"]: claim for claim in evidence["claims"]}
        self.assertEqual(claims["vd_swap_reached"]["classification"],
                         "bridge_observed")
        self.assertEqual(claims["visible_frontend"]["classification"],
                         "unknown")
        self.assertEqual(evidence["wire_packet"]["dword_7"], "0xC0036400")
        self.assertEqual(evidence["wire_packet"]["dword_8"], "0x53574150")
        self.assertEqual(evidence["vd_swap"]["width"], 1280)
        self.assertEqual(evidence["vd_swap"]["height"], 720)
        self.assertTrue(evidence["frontier"]["structural_pm4_decode"])
        self.assertTrue(
            evidence["frontier"]["renderer_relevant_packet_execution"])
        self.assertTrue(evidence["frontier"]["synchronous_effect_execution"])
        self.assertFalse(
            evidence["frontier"]["full_semantic_packet_execution"])
        self.assertEqual(
            evidence["frontier"]["draw_packets_observed_in_final_census"], 2)
        self.assertEqual(
            evidence["frontier"]["typed_draws_accumulated_across_ring_configurations"],
            26,
        )
        self.assertEqual(evidence["frontier"]["pm4_xe_swap_observed"], 1)
        self.assertFalse(evidence["frontier"]["pixel_output"])
        self.assertFalse(evidence["frontier"]["qualified_frontend"])
        census = evidence["ring"]["packet_census"]
        self.assertEqual(census["packet_count"], 877)
        self.assertEqual(census["decoded_dword_count"], 3065)
        self.assertEqual(census["type_counts"], [340, 0, 252, 285])
        self.assertEqual(census["type3_opcode_counts"]["DRAW_INDX_2_0x36"], 2)
        self.assertEqual(census["type3_opcode_counts"]["XE_SWAP_0x64"], 1)
        self.assertTrue(census["reached_corpus_qualified"])
        semantic = evidence["reached_semantic_inputs"]
        self.assertEqual(
            [shader["dword_count"] for shader in semantic["shader_loads"]],
            [24, 27, 9, 15],
        )
        self.assertEqual(
            [shader["guest_big_endian_byte_sha256"]
             for shader in semantic["shader_loads"]],
            [
                "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
                "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
                "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
                "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
            ],
        )
        self.assertEqual(
            [draw["primitive"] for draw in semantic["draws"]],
            ["rectangle_list", "rectangle_list"],
        )
        self.assertTrue(all(draw["index_count"] == 3
                            for draw in semantic["draws"]))
        self.assertEqual(
            [draw["register_state_sha256"] for draw in semantic["draws"]],
            [
                "14536e8c1d5df66b6ea034496d7a6c25f0ec62df0122188282fcb5e3627095d8",
                "003a1ada74eb303b64b9896e33e87027885e40ff9e7d0ca693bfc106bdf13168",
            ],
        )
        self.assertTrue(semantic["swap"]["tiled"])
        typed = evidence["typed_command_output"]
        self.assertTrue(typed["transactional"])
        self.assertEqual(typed["shader_load_count"], 5)

        self.assertEqual(typed["draw_count"], 26)
        self.assertEqual(typed["bootstrap_point_draw_count"], 24)
        self.assertEqual(typed["final_rectangle_draw_count"], 2)
        self.assertEqual(typed["present_count"], 1)
        self.assertEqual(typed["present_resource_id"],
                         "5a192a0ad264cdedf1c271c3fc72944aa209c810d99495fe4acba00b862a93e0")
        self.assertFalse(typed["guest_addresses_in_public_commands"])
        intervention_ids = {
            item["id"] for item in evidence["interventions"]
            if item["behavioral"]
        }
        self.assertEqual(intervention_ids, {
            "hle_vd_swap_packet",
            "bounded_cp_semantics",
            "headless_present_counter",
        })

    def test_neutral_first_pm4_inventory_stops_before_visual_claim(self):
        inventory = json.loads((WORKSPACE / "analysis/demo" /
                                "ac6-demo-neutral-first-pm4-inventory-v1.json").read_text())
        self.assertEqual(inventory["schema"], "ac6-demo-pm4-inventory/v1")
        self.assertEqual(inventory["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(inventory["semantic_decode"],
                         "blocked_at_first_unknown")
        self.assertEqual(inventory["first_unknown"], {
            "ib_address": "0x1685A000", "index": "0x0A02",
            "kind": "register", "name": "UNKNOWN_0A02",
            "offset_dword": 2,
        })
        self.assertEqual(
            [(item["address"], item["dword_count"], item["packet_count"])
             for item in inventory["buffers"]],
            [("0x1686A040", 11, 3), ("0x1685A000", 64, 23),
             ("0x16ADF000", 74, 16), ("0x16ADFD40", 13, 3),
             ("0x16AE0980", 48, 24)],
        )
        opcodes = {name for item in inventory["buffers"]
                   for name in item["type3_opcode_counts"]}
        self.assertNotIn("PM4_XE_SWAP", opcodes)

    def test_demo_nsxr_inventory_is_redacted_and_matches_reached_pixel_shader(self):
        path = WORKSPACE / "analysis/demo/ac6-demo-nsxr-shader-inventory-v1.json"
        encoded = path.read_bytes()
        inventory = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "0d087b68072845390eb4d0a8415fec91c4fb4c67ceb2aa33f799150d96995e68")
        self.assertEqual(inventory["schema"], "ac6-demo-nsxr-shader-inventory/v1")
        self.assertEqual(inventory["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertFalse(inventory["proprietary_bytes_published"])
        self.assertEqual((inventory["wrapper_count"], inventory["container_count"]),
                         (7, 14))
        reached = "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25"
        matches = [(wrapper["address"], container["wrapper_offset"])
                   for wrapper in inventory["wrappers"]
                   for container in wrapper["containers"]
                   if container["microcode_sha256"] == reached]
        self.assertEqual(matches, [
            ("0x8264B390", "0x00000280"),
            ("0x8264B790", "0x00000280"),
        ])

    def test_neutral_main_ib_is_structurally_closed_and_has_exact_resolve_fields(self):
        inventory = json.loads((WORKSPACE / "analysis/demo" /
                                "ac6-demo-neutral-after-230b-pm4-inventory-v1.json").read_text())
        self.assertIsNone(inventory["first_unknown"])
        self.assertEqual(inventory["semantic_decode"], "structurally_known")
        main = next(item for item in inventory["buffers"]
                    if item["address"] == "0x1274A000")
        self.assertEqual((main["dword_count"], main["packet_count"]), (3029, 871))
        packets = {item["offset_dword"]: item for item in main["packets"]}
        self.assertEqual(packets[326]["base_register"], "0x2318")
        self.assertEqual(packets[326]["payload"]["preview_dwords"], [
            "0x00100000", "0x1374A000", "0x02D00500", "0x01000300",
        ])
        self.assertEqual(packets[387]["opcode"], "0x36")
        self.assertEqual(packets[415]["opcode"], "0x64")

    def test_fresh_pm4_opaque_register_ab_is_closed_without_pixel_claim(self):
        capsule_path = WORKSPACE / "analysis/demo" / "ac6-demo-pm4-opaque-registers-v2.json"
        capsule_bytes = capsule_path.read_bytes()
        capsule = json.loads(capsule_bytes)
        self.assertEqual(hashlib.sha256(capsule_bytes).hexdigest(),
                         "839c03f74f0e4a0205a10c3edfb5029f0f779c0b615cece9efaa6353678124d8")
        self.assertEqual(capsule["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertTrue(capsule["fresh_ab"]["comparison"]["neutral_before_start"])
        self.assertTrue(capsule["fresh_ab"]["comparison"]["graphics_equal"])
        self.assertTrue(capsule["fresh_ab"]["comparison"]["effects_equal"])
        self.assertFalse(capsule["fresh_ab"]["comparison"]["trace_equal"])
        self.assertIsNone(capsule["ring_and_ib"]["main_ib"]["first_unknown"])
        self.assertEqual(capsule["opaque_register_packet"]["writes"], [
            {"index": "0x0A02", "value": "0xC0100000",
             "status": "opaque-storage-qualified"},
            {"index": "0x0A03", "value": "0x07F00000",
             "status": "opaque-storage-qualified"},
            {"index": "0x0A04", "value": "0xC0000000",
             "status": "opaque-storage-qualified"},
            {"index": "0x0A05", "value": "0x00100000",
             "status": "opaque-storage-qualified"},
        ])
        self.assertTrue(capsule["policy"]["fail_closed_on_unseen_register_value"])
        self.assertFalse(capsule["policy"]["guest_owned_pixels_qualified"])
        for name, expected_hash in (
                ("ac6-demo-neutral-pm4-inventory-v2-opaque-registers.json",
                 "cd3a37a299fe48805fd80cc1804db99820d85bcd60265668b5ab8e13d2d5e868"),
                ("ac6-demo-start-pm4-inventory-v2-opaque-registers.json",
                 "018d61e85e3f87d518c2289c65496b9de917dbd9abbe5890a1985f21926e21e0")):
            inventory_bytes = (WORKSPACE / "analysis/demo" / name).read_bytes()
            inventory = json.loads(inventory_bytes)
            self.assertEqual(hashlib.sha256(inventory_bytes).hexdigest(), expected_hash)
            self.assertEqual(inventory["semantic_decode"], "structurally_known")
            self.assertIsNone(inventory["first_unknown"])
            main = next(item for item in inventory["buffers"]
                        if item["address"] == "0x1274A000")
            self.assertEqual((main["dword_count"], main["packet_count"],
                              main["byte_sha256"]),
                             (3029, 871,
                              "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6"))

    def test_reached_pixel_spirv_has_fail_closed_validation_receipt(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-pixel-spirv-validation-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["input"]["microcode_sha256"],
                         "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25")
        self.assertEqual(receipt["tools"]["spirv_tools"]["commit"],
                         "e39e5c5838bc4b4162c349f2a2e5f163efe5432f")
        self.assertEqual(receipt["outputs"]["spirv_val_exit_code"], 0)
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertFalse(receipt["policy"]["hlsl_or_spirv_tracked"])

    def test_reached_vertex_spirv_and_tiling_receipts_are_fail_closed(self):
        shader_receipt = json.loads((WORKSPACE / "analysis/demo" /
                                     "ac6-demo-rexglue-reached-vertex-spirv-v1.json").read_text())
        self.assertEqual(shader_receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(len(shader_receipt["shaders"]), 3)
        self.assertTrue(all(shader["spirv_val_exit_code"] == 0
                            for shader in shader_receipt["shaders"]))
        self.assertEqual(shader_receipt["xenosrecomp"]["status"],
                         "not-run-fail-closed")
        self.assertFalse(shader_receipt["policy"]["microcode_disassembly_or_spirv_tracked"])

        tiling = json.loads((WORKSPACE / "analysis/demo" /
                             "ac6-demo-reached-resolve-tiling-v1.json").read_text())
        self.assertEqual(tiling["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(tiling["destination"]["tiled_extent_hex"], "0x00398000")
        self.assertEqual(tiling["destination"]["end_exclusive"], "0x13AE2000")
        self.assertEqual(tiling["coherency_observation"]["coher_size_host"],
                         "0x00385000")
        self.assertTrue(tiling["policy"]["fail_closed"])
        self.assertFalse(tiling["policy"]["readback_or_screencap_claimed"])

    def test_neutral_vulkan_resolve_receipt_is_test_only_and_reproducible(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-neutral-vulkan-resolve-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["reached_contract"]
                         ["push_constant_dwords_absolute_guest_base"], [
                             "0x00000010", "0x00091400", "0x01000300",
                             "0x00005C28", "0x1374A000",
                         ])
        self.assertEqual(receipt["validation"]["fresh_processes"], 2)
        self.assertTrue(receipt["validation"]["fresh_outputs_identical"])
        self.assertTrue(receipt["validation"]["full_tiled_buffer_matches_cpu_oracle"])
        self.assertEqual(receipt["validation"]["neutral_linear_rgba8_sha256"],
                         "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f")
        self.assertTrue(receipt["policy"]["test_only"])
        self.assertFalse(receipt["policy"]["product_runtime_readback_claimed"])
        self.assertFalse(receipt["policy"]["screencap_claimed"])

    def test_renderer_mailbox_receipt_keeps_shader_words_ephemeral(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-renderer-command-mailbox-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["mailbox"]["limit_commands"], 4096)
        shader = receipt["mailbox"]["shader_payload"]
        self.assertTrue(shader["runtime_only"])
        self.assertFalse(shader["reported_or_traced"])
        self.assertFalse(shader["installed_as_asset"])
        self.assertFalse(receipt["policy"]["shader_or_microcode_serialized"])
        self.assertFalse(receipt["policy"]["proprietary_asset_tracked"])
        self.assertTrue(receipt["policy"]["fail_closed"])

        knownness = json.loads((WORKSPACE / "analysis/demo" /
                                "ac6-demo-neutral-edram-knownness-v1.json").read_text())
        self.assertEqual(knownness["bootstrap"]["draw_count"], 24)
        self.assertEqual(knownness["bootstrap"]["effect"],
                         "no-color-depth-or-stencil-write")
        self.assertEqual(knownness["knownness_after_resolve"]["known_pixels"],
                         1280 * 720)
        self.assertEqual(knownness["knownness_after_resolve"]["unknown_pixels"],
                         0)
        self.assertEqual(knownness["knownness_after_resolve"]
                         ["semantic_frame_qualified"], "uniform-black")
        self.assertFalse(knownness["knownness_after_resolve"]
                         ["runtime_readback_produced"])

    def test_point_draw_trace_is_fresh_pal_ab_and_fail_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-point-draw-trace-v1.json"
        encoded = path.read_bytes()
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "f4341a16de1f165f32d880d7f004b01c70ea6a3ea5817f8ad7f31b045eac6c04")
        receipt = json.loads(encoded)
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertTrue(receipt["scope"]["fresh_store_per_route"])
        self.assertTrue(receipt["scope"]["read_only"])
        self.assertFalse(receipt["scope"]["xenia_executed"])
        self.assertFalse(receipt["scope"]["ptrace_used"])
        self.assertEqual(receipt["point_draws"]["rows_per_route"], 24)
        self.assertEqual(receipt["point_draws"]["thread_set"], [1])
        self.assertTrue(receipt["point_draws"]["all_rows_same_raw_state"])
        self.assertEqual(receipt["point_draws"]["raw_registers"]["0x2104"],
                         "0x00000000")
        self.assertEqual(receipt["point_draws"]["raw_registers"]["fetch_dwords"], 0)
        self.assertTrue(receipt["runs"]["comparison"]["stderr_byte_identical"])
        self.assertTrue(receipt["runs"]["comparison"]["graphics_subtree_byte_identical"])
        self.assertFalse(receipt["renderer_observation"]["readback_produced"])
        self.assertFalse(receipt["renderer_observation"]["screencap_produced"])
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertFalse(receipt["policy"]["renderer_fallback"])

    def test_main_ib_rr_provenance_is_demo_scoped_and_exact(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-main-ib-rr-provenance-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["captured_main_ib"], {
            "address": "0x1274A000",
            "end_exclusive": "0x1274CF54",
            "dword_count": 3029,
            "byte_sha256": "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6",
            "first_dword": "0x00000D02",
            "last_dword": "0x00000005",
        })
        self.assertEqual(receipt["ring_publication"]["guest_dwords"], [
            "0xC0013F00", "0x1274A000", "0x00000BD5",
        ])
        self.assertEqual(receipt["ring_publication"]["store_guest_pcs"], [
            "0x821B9D24", "0x821B9D3C", "0x821B9D44",
        ])
        self.assertEqual(receipt["writers"]["first_zone_initial_write"]
                         ["guest_pc"], "0x821BAE5C")
        self.assertEqual(receipt["writers"]["first_dword_final_write"]
                         ["guest_pc"], "0x821B0D70")
        self.assertEqual(receipt["writers"]["first_dword_final_write"]
                         ["instruction_bytes"], "95 4B 00 04")
        self.assertEqual(receipt["writers"]["first_dword_final_write"]
                         ["guest_lr"], None)
        self.assertEqual(receipt["writers"]["first_dword_final_write"]
                         ["guest_thread"], None)
        self.assertEqual(receipt["writers"]["first_dword_final_write"]
                         ["tick"], None)
        self.assertEqual(receipt["writers"]["last_zone_final_write"]
                         ["guest_pc"], "0x821BA01C")
        self.assertFalse(receipt["limits"]["final_first_dword_lr_thread_tick_complete"])
        self.assertFalse(receipt["limits"]["whole_ib_single_writer_claimed"])
        self.assertFalse(receipt["limits"]["start_or_vulkan_route_qualified"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["system_rr_used"])
        self.assertFalse(receipt["policy"]["proprietary_asset_tracked"])

    def test_vertex_shader_rr_provenance_joins_exact_image_and_pm4_ranges(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-vertex-shader-rr-provenance-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["rr_source_commit"],
                         "7352eb807ed75e3b51be85fa6a27f121235dbfb0")
        self.assertEqual(receipt["scope"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["scope"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        shaders = receipt["vertex_shaders"]
        self.assertEqual([(shader["sha256"], shader["byte_count"],
                           shader["image_source"]["start"],
                           shader["pm4_destination"]["start"],
                           shader["copy"]["guest_pc"],
                           shader["copy"]["guest_lr"])
                          for shader in shaders], [
            ("099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
             96, "0x82013E20", "0x16ADF014", "0x82327DEC", "0x821B1DB8"),
            ("93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
             108, "0x820140A0", "0x1274A254", "0x82327DEC", "0x821B63FC"),
            ("586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
             60, "0x82014140", "0x1274A540", "0x82327E38", "0x821B7830"),
        ])
        self.assertTrue(all(shader["copy"]["guest_thread"] == 1 and
                            shader["copy"]["tick"] == 0 for shader in shaders))
        self.assertEqual(receipt["image_initialization"]["host_writer"],
                         "ac6demo::GuestMemory::map_bytes")
        self.assertIsNone(receipt["image_initialization"]["guest_pc"])
        self.assertFalse(receipt["limits"]["container_match_claimed"])
        self.assertFalse(receipt["limits"]["retail_cross_match_used"])
        self.assertFalse(receipt["policy"]["microcode_or_shader_tracked"])
        self.assertFalse(receipt["policy"]["proprietary_asset_tracked"])

        validator = (PROJECT / "tools/validate_qualified_vertex_sources.py").read_text()
        self.assertIn("--scalar-block-layout", validator)
        self.assertIn("TemporaryDirectory", validator)
        self.assertIn(receipt["scope"]["pal_basefile_sha256"], validator)
        for shader in shaders:
            self.assertIn(shader["sha256"], validator)
        self.assertIn(
            "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
            validator,
        )
        self.assertIn('(\"491360\", \"pixel\", 0x13E80, 36, 1,', validator)
        self.assertIn("qualified_shader_sources=4/4", validator)

    def test_map_shader_offline_gate_is_demo_scoped_and_fail_closed(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-map-shader-offline-gate-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["result"]["qualified_unique"], 72)
        self.assertEqual(receipt["result"]["blocked_unique"], 6)
        self.assertEqual(receipt["result"]["qualified_occurrences"], 102)
        self.assertEqual(receipt["result"]["blocked_occurrences"], 6)
        self.assertEqual(receipt["result"]["pixel_qualified"], 28)
        self.assertEqual(receipt["result"]["vertex_qualified"], 44)
        self.assertTrue(receipt["result"]["fresh_gate_receipts_byte_identical"])
        self.assertEqual(len(receipt["blocked"]), 6)
        self.assertEqual(
            {item["blocker"] for item in receipt["blocked"]},
            {"xenosrecomp_unsupported_point_size_export"},
        )
        self.assertFalse(receipt["policy"]["proprietary_bytes_tracked"])
        self.assertFalse(receipt["policy"]["generated_hlsl_or_spirv_tracked"])
        self.assertTrue(receipt["policy"]["fail_closed"])
        gate = (WORKSPACE / "tools/qualify_ac6_map_shaders.py").read_text()
        self.assertIn("TMPDIR must be /fastdata/lavaulta/tmp", gate)
        self.assertIn("output collision", gate)
        self.assertIn("--register-count", gate)
        self.assertIn("parse_vertex_elements", gate)
        self.assertIn("normalize_semantic_alias_parameters", gate)
        self.assertIn("point_size_edge_flag_kill_vertex_mask", gate)
        cli = (PROJECT / "tools/rexglue_shader_cli.cpp").read_text()
        self.assertIn("dynamic_register_addressing=true translation=blocked", cli)
        self.assertIn("shader SHA-256 identity mismatch", cli)
        self.assertIn("--analysis-only", cli)

    def test_start_newpress_rr_provenance_preserves_ab_and_unknown_boundary(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-start-newpress-rr-provenance-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        scope = receipt["scope"]
        self.assertEqual(scope["rr_source_commit"],
                         "7352eb807ed75e3b51be85fa6a27f121235dbfb0")
        self.assertEqual(scope["direct_report_sha256"], scope["rr_report_sha256"])
        self.assertEqual(scope["direct_rtply_sha256"], scope["rr_rtply_sha256"])
        self.assertTrue(scope["fresh_store_per_run"])
        writer = receipt["new_press"]["writer"]
        self.assertEqual((writer["guest_pc"], writer["instruction_bytes"],
                          writer["guest_lr"], writer["guest_thread"], writer["tick"]),
                         ("0x822F6054", "91 7F 00 14", "0x822F617C", 1, 252))
        copy = receipt["first_copy"]
        self.assertEqual((copy["guest_pc"], copy["source_new_press_address"],
                          copy["destination_new_press_address"]),
                         ("0x82327E34", "0x829D1550", "0x7F0409F0"))
        access = receipt["first_post_copy_access"]
        self.assertEqual((access["guest_pc"], access["instruction_bytes"],
                          access["effective_aligned_address"]),
                         ("0x821A4E2C", "11 A0 F8 C3", "0x7F0409F0"))
        self.assertTrue(receipt["limits"]["first_post_copy_guest_pc_qualified"])
        semantic = receipt["semantic_consumer"]
        self.assertEqual((semantic["input_read"]["guest_pc"],
                          semantic["start_extract"]["guest_pc"],
                          semantic["output_write"]["guest_pc"],
                          semantic["output_write"]["guest_address"]),
                         ("0x82198D0C", "0x82198D68", "0x82198DD0",
                          "0x827B37E0"))
        self.assertEqual(receipt["persistence_check"]["logical_value_at_next_tick"],
                         "0x00000000")
        self.assertFalse(receipt["persistence_check"]["two_tick_persistence"])
        logical = receipt["logical_bitset_handoff"]["writer"]
        self.assertEqual((logical["guest_pc"], logical["instruction_bytes"],
                          logical["guest_address"],
                          logical["logical_value_at_tick_252"],
                          logical["logical_value_at_tick_253"]),
                         ("0x821DE6F8", "91 03 0E 4C", "0x82798488",
                          "0x00000010", "0x00000000"))
        self.assertFalse(receipt["logical_bitset_handoff"]["menu_consumers"]
                         ["runtime_reached"])
        probe = receipt["menu_boundary_probe"]
        self.assertEqual(probe["hits"], {"0x82170F58": 0, "0x82185198": 0})
        self.assertEqual(probe["max_tick"], 300)
        self.assertEqual(probe["classification"],
                         "demo-observed-no-hit-through-tick-300")
        self.assertEqual(len(probe["gdb_script_sha256"]), 64)
        extended = receipt["extended_start_ab"]
        self.assertEqual(extended["direct_rtply_sha256"],
                         extended["rr_rtply_sha256"])
        self.assertEqual(extended["direct_report_sha256"],
                         extended["rr_report_sha256"])
        self.assertTrue(extended["fresh_store_per_run"])
        self.assertFalse(extended["frontend_milestone"])
        self.assertEqual(extended["presents"], 163)
        self.assertFalse(receipt["limits"]["two_tick_persistence_proven"])
        self.assertFalse(receipt["limits"]["visual_transition_proven"])
        self.assertFalse(receipt["limits"]["start_promoted"])
        self.assertFalse(receipt["policy"]["system_rr_used"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["proprietary_asset_tracked"])
        dispatch = receipt["task_dispatch_probe"]
        self.assertEqual((dispatch["breakpoint_guest_pc"],
                          dispatch["dispatch_callsite_guest_pc"],
                          dispatch["dispatch_slot"],
                          dispatch["observed_ticks"]),
                         ("0x82259D10", "0x82259D74", 4,
                          {"first": 252, "last": 299, "count": 48}))
        self.assertEqual(dispatch["manager"], "0x18970400")
        self.assertEqual([item["offset"] for item in dispatch["lists"]],
                         [564, 580, 596])
        self.assertEqual(dispatch["lists"][0]["nodes"], [])
        self.assertEqual([(node["task"], node["vtable"], node["slot4"])
                          for node in dispatch["lists"][1]["nodes"]],
                         [("0x2E7F0080", "0x8201130C", "0x8218A4A0")])
        self.assertEqual([(node["task"], node["vtable"], node["slot4"])
                          for node in dispatch["lists"][2]["nodes"]],
                         [("0x18BA2BF4", "0x8200F388", "0x8218CE20"),
                          ("0x18980000", "0x82011694", "0x821929A8")])
        self.assertFalse(dispatch["menu_candidates"]["observed_in_lists"])
        self.assertFalse(dispatch["menu_candidates"]["observed_update_hits"])

    def test_start_queue_rr_provenance_keeps_instruction_join_fail_closed(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-start-queue-rr-provenance-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["rr_source_commit"],
                         "7352eb807ed75e3b51be85fa6a27f121235dbfb0")
        self.assertTrue(receipt["scope"]["fresh_store_per_run"])
        queue = receipt["queue"]
        self.assertEqual((queue["base"], queue["producer_address"],
                          queue["consumer_address"]),
                         ("0x82386CC0", "0x8238CD90", "0x8238CD94"))
        boundary = receipt["boundary_probe"]
        self.assertEqual((boundary["observations"], boundary["worker"]["tick"],
                          boundary["primary"]["first_tick"],
                          boundary["primary"]["last_tick"]), (49, 221, 252, 299))
        self.assertEqual(boundary["primary"]["count"], 48)
        self.assertEqual((boundary["worker"]["producer"],
                          boundary["worker"]["consumer"],
                          boundary["primary"]["producer"],
                          boundary["primary"]["consumer"]), (0, 0, 0, 0))
        producer = receipt["watchpoints"]["producer"]
        consumer = receipt["watchpoints"]["consumer"]
        self.assertEqual((producer["tick"], producer["guest_thread"],
                          producer["transition"]), (299, 25, {"guest_old": 1, "guest_new": 0}))
        self.assertEqual((consumer["tick"], consumer["guest_thread"],
                          consumer["transition"]), (298, 25, {"guest_old": 1, "guest_new": 0}))
        self.assertEqual((producer["dynamic_function"],
                          consumer["dynamic_function"]),
                         ("sub_820FFCA0", "sub_820FFCA0"))
        self.assertEqual((producer["static_store_candidate"]["guest_pc"],
                          consumer["static_store_candidate"]["guest_pc"]),
                         ("0x820FFD94", "0x820FFD98"))
        self.assertEqual((producer["static_store_candidate"]["instruction_bytes"],
                          consumer["static_store_candidate"]["instruction_bytes"]),
                         ("93 7F 60 D0", "93 7F 60 D4"))
        self.assertEqual((producer["dynamic_callsite_join"]["host_return_address"],
                          producer["dynamic_callsite_join"]["host_callsite_address"],
                          consumer["dynamic_callsite_join"]["host_return_address"],
                          consumer["dynamic_callsite_join"]["host_callsite_address"]),
                         ("0x5f8d5322e219", "0x5f8d5322e214",
                          "0x5f8d5322e259", "0x5f8d5322e254"))
        self.assertEqual(consumer["normal_path_static_candidate"]["guest_pc"],
                         "0x820FFD78")
        self.assertTrue(receipt["limits"]["host_callsite_and_guest_static_map_joined"])
        self.assertEqual(receipt["watchpoints"]["producer_helper"]
                         ["static_store_candidate"]["guest_pc"], "0x820FF75C")
        self.assertEqual(receipt["watchpoints"]["producer_helper"]["transition"],
                         {"guest_old": 0, "guest_new": 1})
        slot_probe = receipt["native_slot_probe"]
        self.assertEqual((slot_probe["record_count"],
                          slot_probe["valid_slot_snapshot_count"],
                          slot_probe["slot_snapshot_bytes"]), (290, 96, 96))
        self.assertTrue(slot_probe["enabled_only_by_env"] ==
                        "AC6_DEMO_WATCH_RENDER_QUEUE_WRITERS")
        self.assertTrue(slot_probe["observations"][0]["slot_all_zero"])
        self.assertTrue(slot_probe["observations"][1]["slot_all_zero"])
        self.assertEqual((slot_probe["observations"][0]["guest_pc_static_map"],
                          slot_probe["observations"][1]["guest_pc_static_map"],
                          slot_probe["observations"][2]["guest_pc_static_map"],
                          slot_probe["observations"][3]["guest_pc_static_map"]),
                         ("0x820FF75C", "0x820FFD78", "0x820FFD94",
                          "0x820FFD98"))
        self.assertFalse(slot_probe["observations"][0]["context_lr_is_guest_lr"])
        self.assertTrue(receipt["limits"]["native_hook_store_lines_observed"])
        self.assertTrue(receipt["limits"]["native_hook_slot_payload_observed_all_zero"])
        self.assertIn("reverse", receipt["reverse_watchpoint_method"]
                      ["direction_normalization"])
        self.assertFalse(receipt["limits"]["direct_producer_store_pc_qualified"])
        self.assertFalse(receipt["limits"]["direct_consumer_store_pc_qualified"])
        self.assertFalse(receipt["limits"]["guest_lr_qualified"])
        self.assertFalse(receipt["limits"]["queue_command_payload_semantics_qualified"])
        self.assertFalse(receipt["limits"]["visual_transition_proven"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["system_rr_used"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_neutral_start_vulkan_ab_keeps_black_readbacks_and_no_promotion(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-neutral-start-vulkan-ab-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["architecture"],
                         "Xenon big-endian / Xenos")
        self.assertTrue(receipt["scope"]["fresh_process_per_run"])
        self.assertEqual(receipt["scope"]["max_ticks"], 253)
        self.assertEqual(receipt["runs"]["neutral"]["return_code"], 4)
        self.assertEqual(receipt["runs"]["start"]["return_code"], 4)
        self.assertNotEqual(receipt["runs"]["neutral"]["rtply_sha256"],
                             receipt["runs"]["start"]["rtply_sha256"])
        self.assertNotEqual(receipt["runs"]["neutral"]["report_sha256"],
                             receipt["runs"]["start"]["report_sha256"])
        self.assertEqual(receipt["runs"]["neutral"]["stderr_sha256"],
                         "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
        self.assertEqual(receipt["runs"]["start"]["stderr_sha256"],
                         receipt["runs"]["neutral"]["stderr_sha256"])
        renderer = receipt["renderer_stdout_fields"]
        self.assertEqual((renderer["shader_loads"], renderer["draws"],
                          renderer["presents"], renderer["validated_modules"],
                          renderer["vulkan_modules"], renderer["graphics_pipelines"]),
                         (5, 26, 1, 4, 4, 2))
        self.assertEqual(renderer["normal_readback_sha256"],
                         "0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366")
        self.assertEqual(renderer["neutral_resolve_sha256"],
                         "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f")
        comparison = receipt["comparison"]
        self.assertEqual(comparison["report_subtrees_byte_equal"],
                         ["outcome", "milestones", "graphics", "scheduler"])
        self.assertEqual((comparison["completed_ticks"],
                          comparison["presentation_notifications"]), (253, 116))
        self.assertFalse(comparison["frontend_milestone"])
        self.assertFalse(comparison["mission_milestone"])
        self.assertFalse(comparison["terminal_milestone"])
        self.assertEqual(comparison["vd_swap"], {
            "calls": 116,
            "tick": 252,
            "frontbuffer_address": "0x1374A000",
            "format": 6,
            "tiled": True,
            "width": 1280,
            "height": 720,
        })
        self.assertFalse(receipt["policy"]["start_promoted"])
        self.assertFalse(receipt["policy"]["play_fallback_enabled"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["proprietary_asset_tracked"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_current_pm4_vulkan_guest_readback_is_reproducible_but_black(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-vulkan-guest-readback-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "f3e376f3778937c89f33574f7c7ba28972d3d309aeeda6b24ebd37e3e7d2350a")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertTrue(receipt["fresh_ab"]["comparison"]["renderer_stdout_equal"])
        self.assertTrue(receipt["fresh_ab"]["comparison"]["report_graphics_equal"])
        self.assertTrue(receipt["fresh_ab"]["comparison"]["guest_writeback_both_routes"])
        self.assertFalse(receipt["fresh_ab"]["comparison"]["trace_equal"])
        stats = receipt["renderer_stats"]
        self.assertEqual((stats["shader_loads"], stats["draws"], stats["presents"],
                          stats["normal_draws"], stats["neutral_resolves"]),
                         (5, 26, 1, 1, 1))
        self.assertTrue(stats["guest_writeback"])
        self.assertEqual(stats["guest_linear_sha256"],
                         "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f")
        self.assertEqual(receipt["guest_present_join"], {
            "destination_address": "0x1374A000", "format": 6, "tiled": True,
            "width": 1280, "height": 720, "tiled_extent_bytes": "0x398000",
            "linear_extent_bytes": "0x384000",
            "readback_verification": "guest reread untiled and SHA matched resolved linear pixels",
            "padding_policy": "guest padding preserved",
        })
        self.assertFalse(receipt["policy"]["screencap_produced"])
        self.assertFalse(receipt["policy"]["frontend_promoted"])
        self.assertTrue(receipt["policy"]["guest_readback_fail_closed"])

    def test_event_handoff_probe_keeps_xenia_and_pal_handles_separate(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-handoff-probe-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["architecture"],
                         "Xenon big-endian / Xenos")
        scope = receipt["scope"]
        self.assertTrue(scope["instrumentation_default"] is False)
        self.assertEqual((scope["record_limit_per_run"], scope["records_per_run"],
                          scope["record_tick_range"]), (4096, 4096, [0, 285]))
        self.assertEqual(scope["neutral_stderr_sha256"],
                         scope["start_stderr_sha256"])
        primary = receipt["demo_observed"]["bridge_handles"]["primary"]
        self.assertEqual((primary["signal"], primary["wait"],
                          primary["signal_wait_enter"],
                          primary["signal_wait_block"],
                          primary["signal_wait_resume"]),
                         ("0xE0000048", "0xE000004C", 71, 36, 35))
        generic = receipt["xenia_generic"]["signal_and_wait"]
        self.assertEqual((generic["signal_handle"], generic["wait_handle"]),
                         ("0xF8000088", "0xF800008C"))
        self.assertEqual(receipt["demo_qualified"]["completed_ticks"], 600)
        self.assertFalse(receipt["demo_qualified"]["frontend"])
        self.assertFalse(receipt["demo_qualified"]["mission"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["runtime_behavior_changed"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_focused_event_handoff_covers_full_ab_window(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-handoff-focused-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        scope = receipt["scope"]
        self.assertEqual((scope["record_tick_range"], scope["records_per_run"]),
                         ([0, 599], 13785))
        self.assertTrue(scope["stderr_byte_equal"])
        self.assertEqual(receipt["operations"]["set_enter"], 3210)
        self.assertEqual(receipt["operations"]["clear"], 55)
        self.assertEqual(receipt["operations"]["pulse_enter"], 2)
        self.assertEqual(receipt["operations"]["resume_thread"], 11)
        self.assertEqual(receipt["demo_observed"]["primary_pair"]["signal_wait_records"],
                         1402)
        self.assertEqual(receipt["demo_observed"]["secondary_pair"]["signal_wait_records"],
                         1415)
        self.assertEqual(receipt["demo_qualified"]["presents"], 463)
        self.assertFalse(receipt["demo_qualified"]["frontend"])
        self.assertFalse(receipt["policy"]["runtime_behavior_changed"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_writer_static_join_is_pal_only_and_exact(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-writer-static-join-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        dynamic = receipt["dynamic"]
        self.assertEqual(dynamic["neutral_stderr_sha256"],
                         dynamic["start_stderr_sha256"])
        self.assertEqual(dynamic["pulse"], {
            "handles": ["0xE000002C"], "ticks": [112, 264],
            "thread": 9, "caller_lr": "0x821A688C"})
        self.assertEqual(dynamic["resume"]["records"], 11)
        self.assertEqual(dynamic["resume"]["caller_lr"], "0x821A6B0C")
        callers = {item["import"]: item for item in receipt["static_join"]["callers"]}
        self.assertEqual((callers["NtPulseEvent"]["callsite"],
                          callers["NtPulseEvent"]["owner_entry"]),
                         ("0x821A6888", "0x821A6878"))
        self.assertEqual((callers["NtResumeThread"]["callsite"],
                          callers["NtResumeThread"]["owner_entry"]),
                         ("0x821A6B08", "0x821A6AF8"))
        self.assertEqual(callers["NtWaitForMultipleObjectsEx"]["ordinal"], 254)
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_handle_writer_probe_joins_exact_pal_store_words(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-handle-writer-probe-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertTrue(receipt["instrumentation"]["read_only"])
        self.assertEqual(receipt["runs"]["neutral"]["writer_rows"], 163)
        self.assertEqual(receipt["runs"]["neutral"]["writer_rows_sha256"],
                         receipt["runs"]["start"]["writer_rows_sha256"])
        self.assertEqual(receipt["static_join"]["site_count"], 25)
        sites = {(item["owner"], item["writer_pc"]): item
                 for item in receipt["static_join"]["sites"]}
        self.assertEqual(sites[("0x821A1E38", "0x821A1EAC")]["writer_bytes"],
                         "0x939EFFF0")
        self.assertEqual(sites[("0x822EED70", "0x822EEDB4")]["writer_bytes"],
                         "0x907F0004")
        self.assertEqual(sites[("0x8219A060", "0x8219A4F4")]["writer_bytes"],
                         "0x913F595C")
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["screencap_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_branch_delay_thunk_frontier_is_exact_and_fail_closed(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-frontier-branch-delay-thunk-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        rr_pm4 = receipt["rr_pm4_provenance_reused"]
        self.assertEqual(rr_pm4["source_commit"],
                         "7352eb807ed75e3b51be85fa6a27f121235dbfb0")
        self.assertEqual(rr_pm4["binary_sha256"],
                         "33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6")
        self.assertEqual(rr_pm4["main_ib_sha256"],
                         "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6")
        self.assertEqual(rr_pm4["writers"]["first_dword_final"], {
            "guest_pc": "0x821B0D70",
            "guest_lr": None,
            "guest_thread": None,
            "tick": None,
            "status": "demo-qualified-pc-only; lr-thread-tick-unknown",
        })
        self.assertEqual(rr_pm4["writers"]["last_dword_final"]["guest_pc"],
                         "0x821BA01C")
        self.assertEqual(rr_pm4["writers"]["ring_publication"]["packet"],
                         ["0xC0013F00", "0x1274A000", "0x00000BD5"])
        self.assertTrue(rr_pm4["policy"]["same_xex_only"])
        self.assertTrue(rr_pm4["policy"]["no_resynchronization"])
        frontier = receipt["pre_resolution_frontier"]
        self.assertEqual((frontier["tick"], frontier["thread"],
                          frontier["return_lr"], frontier["target"]),
                         (385, 1, "0x820DC4FC", "0x820D3310"))
        thunk = receipt["static_join"]["thunk"]
        self.assertEqual(thunk["words"],
                         ["0x81830000", "0x816C0064", "0x7D6903A6", "0x4E800420"])
        self.assertEqual(thunk["bytes_sha256"],
                         "8018a855aaf23aa448ea5acac2fbdc00107cdd658bbc497035361fce6f49556c")
        vtable = receipt["static_join"]["vtable"]
        self.assertEqual((vtable["address"], vtable["slot_address"],
                          vtable["slot_target"]),
                         ("0x82006A9C", "0x82006B00", "0x8219DC18"))
        self.assertEqual(receipt["static_join"]["target_function"]["byte_sha256"],
                         "eb701dd327924814da2db7e2c0ee348bbdf5ab32c4f0ba2fcde3b777a9a612b5")
        dynamic = receipt["dynamic_observation"]
        self.assertTrue(dynamic["all_addresses_mapped"])
        self.assertTrue(dynamic["nested_edge_qualified"])
        self.assertEqual(receipt["runs"]["neutral"]["return_code"], 4)
        self.assertEqual(receipt["runs"]["start"]["return_code"], 4)
        self.assertNotEqual(receipt["runs"]["neutral"]["rtply_sha256"],
                             receipt["runs"]["start"]["rtply_sha256"])
        comparison = receipt["comparison"]
        self.assertEqual(comparison["report_subtrees_byte_equal"],
                         ["outcome", "milestones", "graphics", "scheduler"])
        self.assertEqual((comparison["completed_ticks"],
                          comparison["presentation_notifications"]), (600, 463))
        self.assertEqual((comparison["render_queue_producer_changes"],
                          comparison["render_queue_consumer_changes"]), (695, 0))
        self.assertFalse(comparison["frontend_milestone"])
        self.assertFalse(comparison["mission_milestone"])
        self.assertFalse(comparison["terminal_milestone"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertFalse(receipt["policy"]["generated_cpp_modified"])
        self.assertFalse(receipt["policy"]["fallback_or_synthetic_progression"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_render_queue_exact_writers_correct_sampled_consumer_counter(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-render-queue-write-provenance-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["scope"]["stderr_write_record_count"], 2090)
        self.assertEqual(receipt["scope"]["neutral_stderr_sha256"],
                         receipt["scope"]["start_stderr_sha256"])
        summary = receipt["queue"]["exact_store_summary"]
        self.assertEqual((summary["producer_nonzero"]["address"],
                          summary["producer_nonzero"]["count"],
                          summary["producer_nonzero"]["thread"],
                          summary["producer_nonzero"]["lr"]),
                         ("0x8238CD90", 348, 1, "0x820FF734"))
        self.assertEqual((summary["consumer_nonzero"]["address"],
                          summary["consumer_nonzero"]["count"],
                          summary["consumer_nonzero"]["thread"],
                          summary["consumer_nonzero"]["lr"],
                          summary["consumer_nonzero"]["function"]),
                         ("0x8238CD94", 348, 25, "0x820FFCE4", "0x820FFCA0"))
        self.assertEqual(summary["consumer_reset"]["count"], 348)
        self.assertTrue(receipt["queue"]["slot_samples"]["all_zero"])
        self.assertEqual(receipt["queue"]["slot_samples"]["sha256"],
                         "2ea9ab9198d1638007400cd2c3bef1cc745b864b76011a0e1bc52180ac6452d4")
        self.assertTrue(receipt["ab_comparison"]["neutral_start_queue_write_summary_equal"])
        self.assertFalse(receipt["ab_comparison"]["frontend"])
        self.assertFalse(receipt["ab_comparison"]["mission"])
        self.assertFalse(receipt["policy"]["runtime_behavior_changed"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_render_queue_slot_probe_excludes_metadata_and_finds_no_payload(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-render-queue-slot-write-probe-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["runtime_binary_sha256"],
                         "6fc214d3ead4d07105de98b39d0abd573cb6834098cc9824746b46a4c09f5cd7")
        self.assertEqual(receipt["scope"]["neutral_stderr_sha256"],
                         receipt["scope"]["start_stderr_sha256"])
        probe = receipt["queue"]["slot_payload_probe"]
        self.assertEqual(probe["producer_slot_direct_store_count_per_run"], 348)
        self.assertEqual(probe["producer_slot_direct_nonzero_count_per_run"], 0)
        self.assertEqual(probe["consumer_slot_direct_store_count_per_run"], 0)
        self.assertTrue(probe["consumer_slot_snapshot_all_zero"])
        self.assertEqual(probe["nonzero_queue_metadata_addresses"],
                         ["0x8238CD90", "0x8238CD94", "0x8238CD9C"])
        self.assertFalse(receipt["ab_comparison"]["nonzero_slot_payload"])
        self.assertFalse(receipt["ab_comparison"]["readback_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_runtime_rexglue_neutral_receipt_is_ephemeral_and_deterministic(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-runtime-rexglue-neutral-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(len(receipt["modules"]), 4)
        self.assertEqual({module["microcode_sha256"] for module in
                          receipt["modules"]}, {
            "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
            "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
            "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
            "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
        })
        vertex_sources = {
            module["microcode_sha256"]: module["qualified_image_source"]
            for module in receipt["modules"] if module["stage"] == "vertex"
        }
        self.assertEqual(vertex_sources, {
            "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3":
                ["0x82013E20", "0x82013E80"],
            "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b":
                ["0x820140A0", "0x8201410C"],
            "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0":
                ["0x82014140", "0x8201417C"],
        })
        source_probe = receipt["external_validation"]["qualified_vertex_source_probe"]
        self.assertEqual((source_probe["basefile_ranges_passed"],
                          source_probe["external_spirv_val_passed"]), (3, 3))
        self.assertTrue(source_probe["temporary_only"])
        neutral = receipt["neutral_runtime"]
        self.assertEqual((neutral["shader_loads"], neutral["draws"],
                          neutral["presents"], neutral["validated_modules"]),
                         (5, 26, 1, 4))
        self.assertTrue(neutral["runs_byte_identical"])
        self.assertTrue(neutral["rr_gate_rtply_unchanged"])
        self.assertTrue(neutral["rr_gate_report_unchanged"])
        self.assertEqual(receipt["external_validation"]["modules_passed"], 4)
        vulkan = receipt["neutral_vulkan_runtime"]
        self.assertEqual(vulkan["fresh_store_runs"], 2)
        self.assertEqual(vulkan["created_vk_shader_modules"], 4)
        self.assertEqual(vulkan["created_descriptor_set_layouts"], 2)
        self.assertEqual(vulkan["created_pipeline_layouts"], 1)
        self.assertEqual(vulkan["created_graphics_pipelines"], 2)
        self.assertEqual(vulkan["populated_shared_memory_descriptors"], 4)
        self.assertEqual(vulkan["populated_constant_buffer_descriptors"], 10)
        self.assertTrue(vulkan["runs_byte_identical"])
        self.assertTrue(vulkan["graphics_pipeline_created"])
        self.assertFalse(vulkan["draw_executed"])
        self.assertFalse(vulkan["readback_produced"])
        shared = receipt["reached_shared_memory"]
        self.assertEqual(shared["classification"], "demo-qualified")
        self.assertEqual(shared["guest_range"], ["0x127CA03C", "0x127CA0A8"])
        self.assertEqual(shared["guest_bytes"], 108)
        self.assertEqual(shared["populated_segment"], 2)
        self.assertTrue(shared["zero_initialized_before_commit"])
        self.assertTrue(shared["constant_descriptors_populated"])
        constants = receipt["reached_constant_payloads"]
        self.assertEqual(constants["descriptor_sets"], 2)
        self.assertEqual(constants["descriptors_per_set"], 5)
        self.assertEqual(constants["sizes"]["system"], 504)
        self.assertEqual(constants["spirv_static_system_members"]["vertex"],
                         [0, 1, 2, 3, 4, 8, 9, 10, 12])
        self.assertEqual(constants["normal_system_flags"], "0x00074B00")
        self.assertEqual(constants["copy_system_flags"], "0x00070B00")
        self.assertFalse(constants["draw_executed"])
        layout = receipt["reached_vulkan_layout_abi"]
        self.assertTrue(layout["scalar_block_layout_required_and_enabled"])
        self.assertEqual(layout["sets"][0]["descriptor_count"], 4)
        self.assertEqual(layout["sets"][1]["bindings"], [0, 1, 2, 3, 4])
        self.assertEqual(layout["push_constant_bytes"], 0)
        self.assertEqual(layout["texture_sets_reached"], 0)
        profiles = receipt["reached_graphics_pipeline_profiles"]
        self.assertEqual([profile["samples"] for profile in profiles], [4, 1])
        self.assertEqual(profiles[0]["rb_surface_info"], "0x0A020280")
        self.assertEqual(profiles[1]["rb_surface_info"], "0x14000500")
        self.assertFalse(receipt["transaction"]["serialization"])
        self.assertFalse(receipt["policy"]
                         ["shader_microcode_or_spirv_tracked"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_reached_vertex_shader_exports_observed_zero_color(self):
        path = WORKSPACE / "analysis/demo/ac6-demo-reached-vertex-shader-analysis-v1.json"
        encoded = path.read_bytes()
        analysis = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "8638214bdc031d89ed50e6316de67f31efad80f561b2a3ff40cc544f56c1c5bc")
        self.assertEqual(analysis["microcode_sha256"],
                         "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b")
        self.assertEqual(analysis["demo_qualified"]["color_export"],
                         "interpolator0=max(r0,r0)")
        self.assertEqual(analysis["demo_qualified"]["pixel_input_color0"],
                         [0.0, 0.0, 0.0, 0.0])
        self.assertEqual(analysis["unknown"], [
            "EDRAM content outside the reached rectangle",
            "effects of earlier bootstrap point draws",
        ])

    def test_event_handle_consumer_probe_is_ab_identical_and_fail_closed(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-handle-consumer-probe-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["instrumentation"]["record_limit"], 8192)
        self.assertTrue(receipt["instrumentation"]["default_enabled"] is False)
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["completed_ticks"], start["completed_ticks"]), (300, 300))
        self.assertEqual((neutral["consumer_rows"], start["consumer_rows"]), (3927, 3927))
        self.assertEqual(neutral["consumer_rows_sha256"], start["consumer_rows_sha256"])
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        self.assertEqual(neutral["consumer_groups"], start["consumer_groups"])
        self.assertEqual((neutral["presents"], start["presents"]), (163, 163))
        self.assertFalse(receipt["ab_comparison"]["start_transition_promoted"])
        self.assertFalse(receipt["ab_comparison"]["frontend"])
        self.assertFalse(receipt["ab_comparison"]["mission"])
        self.assertEqual(receipt["consumer_groups_observed"][4], {
            "address": "0x82934748", "value": "0xE0000048",
            "context_lr": "0x822EEE38", "count": 351,
            "ticks": [1, 299], "threads": [1, 12]})
        self.assertEqual(receipt["join_to_writer_probe"]["writer_sites"], 25)
        self.assertEqual(receipt["join_to_writer_probe"]["writer_rows_per_run"], 163)
        self.assertTrue(receipt["policy"]["default_disabled"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_handle_consumer_pc_probe_maps_every_row_to_pal_bytes(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-event-handle-consumer-pc-probe-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["pal_basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["mapper"]["output_schema"],
                         "ac6-demo-generated-guest-load-map/v1")
        self.assertTrue(receipt["mapper"]["basefile_bytes_checked"])
        self.assertTrue(receipt["mapper"]["fail_closed_on_missing_or_ambiguous_site"])
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["consumer_rows"], neutral["mapped_rows"]), (3927, 3927))
        self.assertEqual((start["consumer_rows"], start["mapped_rows"]), (3927, 3927))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        self.assertEqual(neutral["unique_guest_load_sites"], 64)
        self.assertEqual(start["unique_guest_load_sites"], 64)
        sites = {site["guest_pc"]: site for site in receipt["exact_guest_load_sites"]}
        self.assertEqual(sites["0x822EEE38"]["instruction_bytes"], "80 7F 00 00")
        self.assertEqual(sites["0x822EEE44"]["instruction_bytes"], "80 7F 00 04")
        self.assertEqual(sites["0x822E40B8"]["instruction_bytes"], "80 7F 00 00")
        self.assertEqual(sites["0x822E40C8"]["instruction_bytes"], "80 9F 00 04")
        self.assertEqual(receipt["joins"]["writer_sites"], 25)
        self.assertFalse(receipt["joins"]["context_lr_is_exact_guest_pc"])
        self.assertTrue(receipt["joins"]["generated_source_map_is_exact_guest_pc"])
        self.assertFalse(receipt["ab_comparison"]["start_transition_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_handle_payload_probe_is_bounded_and_ab_identical(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-handle-payload-probe-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "7371e170f3a913c8a1b93242e283105dc7a989e1bde6b22445b3652156df6e1b")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["instrumentation"]["snapshot_words"], 8)
        self.assertEqual(receipt["instrumentation"]["snapshot_alignment_bytes"], 32)
        self.assertTrue(receipt["instrumentation"]["overflow_guard_fail_closed"])
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["payload_rows"], start["payload_rows"]), (3927, 3927))
        self.assertEqual((neutral["payload_groups"], start["payload_groups"]), (1057, 1057))
        self.assertEqual((neutral["unique_payload_snapshots"], start["unique_payload_snapshots"]), (530, 530))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        sites = {site["guest_pc"]: site for site in receipt["exact_payload_sites"]}
        self.assertEqual(sites["0x822EEE38"]["snapshot_count"], 223)
        self.assertEqual(sites["0x822E40B8"]["nonbaseline_ticks"],
                         {"0x000000DC": 222, "0x0000001D": 252})
        self.assertFalse(receipt["ab_comparison"]["start_transition_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_handle_payload_writer_joins_post_read_without_mismatch(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-handle-payload-writer-probe-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "48e9133065efe0bfbb4a90301d4aa09eded09a4480944cfd6fb0e6e769405918")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertTrue(receipt["instrumentation"]["overflow_safe_intersection"])
        self.assertFalse(receipt["instrumentation"]["default_enabled"])
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["payload_writer_rows"], start["payload_writer_rows"]), (374, 374))
        self.assertEqual((neutral["mapped_writer_rows"], start["mapped_writer_rows"]), (374, 374))
        self.assertEqual((neutral["unique_writer_sites"], start["unique_writer_sites"]), (19, 19))
        self.assertEqual((neutral["same_tick_thread_writer_to_post_read_pairs"],
                          start["same_tick_thread_writer_to_post_read_pairs"]), (351, 351))
        self.assertEqual((neutral["value_to_post_read_word7_mismatches"],
                          start["value_to_post_read_word7_mismatches"]), (0, 0))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        sequence = receipt["qualified_causal_sequence"]["steps"]
        self.assertEqual([step["guest_pc"] for step in sequence],
                         ["0x822EEE30", "0x822EEE38", "0x822EEE3C", "0x822EEE44"])
        self.assertEqual(sequence[2]["instruction_bytes"], "FB DF 00 10")
        self.assertTrue(receipt["qualified_causal_sequence"]["post_read_word7_matches_writer_low32"])
        self.assertFalse(receipt["ab_comparison"]["start_transition_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_handle_setevent_join_is_ordered_and_ab_identical(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-handle-setevent-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "dbd85daf6507d778da3c63af55e36c0f6a2e1ae725500665b88208265a687d4b")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["runs"]["neutral"]["handoff_rows"], 4485)
        self.assertEqual(receipt["runs"]["start"]["handoff_rows"], 4485)
        self.assertEqual(receipt["runs"]["neutral"]["set_enter_e000004c"], 351)
        self.assertEqual(receipt["runs"]["start"]["set_enter_e000004c"], 351)
        self.assertTrue(receipt["qualified_chain"]["post_read_to_set_enter_ordered"])
        self.assertEqual(receipt["qualified_chain"]["nt_set_event_caller_lr"], "0x821A6AC4")
        self.assertEqual(receipt["qualified_chain"]["sample_tick_252"]["post_read_word7"], "0x0000001D")
        self.assertTrue(receipt["ab_comparison"]["stderr_sha256_equal"])
        self.assertFalse(receipt["ab_comparison"]["start_transition_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_post_set_scheduler_join_is_tick_exact_and_ab_identical(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-post-set-scheduler-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "7b419adf84f7d7ec83afac7ade49c018a574bfc7bf654022539508c955ffa78d")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["set_enter_e000004c"], start["set_enter_e000004c"]),
                         (351, 351))
        self.assertEqual((neutral["post_set_schedule_rows"], start["post_set_schedule_rows"]),
                         (351, 351))
        self.assertEqual((neutral["schedule_tick_delta_zero"], start["schedule_tick_delta_zero"]),
                         (351, 351))
        self.assertTrue(neutral["event_wake_to_schedule_equal"])
        self.assertTrue(start["event_wake_to_schedule_equal"])
        self.assertEqual(neutral["entry_counts"], start["entry_counts"])
        self.assertEqual(neutral["entry_counts"], {
            "0x821A7160": 53,
            "0x821C4970": 149,
            "0x82320560": 97,
            "0x822EE158": 39,
            "0x821A1D10": 13,
        })
        self.assertEqual(receipt["inputs"]["neutral_stderr_sha256"],
                         receipt["inputs"]["start_stderr_sha256"])
        self.assertEqual(receipt["inputs"]["post_set_schedule_rows_sha256"],
                         "ebf80c87c8f5524ea25d364bab109585594ebbcdaba7dd934ac6ae0a2792537a")
        self.assertFalse(receipt["policy"]["readback_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_event_post_set_memory_join_is_ab_identical_and_tick_exact(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-post-set-memory-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "8516533d5dc08cb778100ae0cef74a52493c42564f9a003f794a31ad86649b4f")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        neutral = receipt["runs"]["neutral"]
        start = receipt["runs"]["start"]
        self.assertEqual((neutral["memory_rows"], start["memory_rows"]), (351, 351))
        self.assertEqual((neutral["memory_tick_delta_zero"],
                          start["memory_tick_delta_zero"]), (351, 351))
        self.assertEqual(neutral["operation_counts"], start["operation_counts"])
        self.assertEqual(neutral["entry_counts"], start["entry_counts"])
        self.assertEqual(neutral["unique_guest_addresses"], 56)
        self.assertEqual(receipt["inputs"]["memory_rows_sha256"],
                         "1fb44bae233f8499cbd9e284a1e58248d799ac68b41b48befb327fca76ca7c61")
        self.assertTrue(receipt["ab_comparison"]["memory_rows_equal"])
        self.assertTrue(receipt["ab_comparison"]["event_wake_to_memory_same_tick"])
        self.assertFalse(receipt["policy"]["readback_promoted"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_ib_publish_writer_join_is_ab_identical_and_ordered(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-ib-publish-writer-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "9fb98c47bc69e174b3e80c5b9e342092f13ebedb7e29421d6079e8f5ac267c9f")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["ib"]["main"]["dword_count"], 3029)
        self.assertEqual(receipt["ib"]["main"]["byte_sha256"],
                         "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6")
        self.assertEqual(receipt["ab"]["neutral"]["writer_rows"], 4958)
        self.assertEqual(receipt["ab"]["start"]["writer_rows"], 4958)
        self.assertTrue(receipt["ab"]["byte_identical"])
        self.assertEqual(receipt["ordering"]["first_main_ib_log_index"], 5)
        self.assertEqual(receipt["ordering"]["first_ring_publication_log_index"], 5786)
        self.assertEqual(receipt["ordering"]["first_event_post_set_schedule_index"], 5790)
        self.assertEqual(receipt["coverage"]["missing_contiguous_range"],
                         {"start": "0x1274A660", "end_exclusive": "0x1274A760", "bytes": 256})
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_xenia_runtime_final_archive_is_oracle_only_and_hash_bound(self):
        path = WORKSPACE / "analysis/demo" / "ac6-xenia-runtime-final-archive-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "bfb414641bdb808689f9dc50b9d6fc0b133db7fa7b634388bb7bb21f6755439a")
        self.assertEqual(receipt["archive"]["sha256"],
                         "0196ab3630a937118abea3d41e6d3dc663fcfdb04a3d7d2a843d572361578768")
        self.assertIsNone(receipt["oracle_context"]["pal_demo_xex_sha256"])
        self.assertEqual(receipt["successful_run"]["runtime"]["event_counts"],
                         {"thread_create": 23, "frame_swap": 12496, "audio_submit": 59969})
        self.assertEqual(receipt["successful_run"]["semantic"]["watchdog_snapshots_with_waits"], 0)
        self.assertEqual(receipt["classification"]["demo-qualified"], [])
        self.assertTrue(receipt["policy"]["screenshots_promoted_to_native_proof"] is False)
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_ib_reader_pm4_join_is_ab_identical_and_guest_pc_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-ib-reader-pm4-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "7b99e0b7be96e4652ac8d90adccd908cd0298a4dd64ae0d7465cc854535065f1")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["ab"]["neutral"]["reader_rows"], 9)
        self.assertEqual(receipt["ab"]["start"]["reader_rows"], 9)
        self.assertTrue(receipt["ab"]["reader_stream_byte_identical"])
        self.assertEqual(receipt["pm4_join"]["main_ib_sha256"],
                         "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6")
        self.assertEqual(receipt["consumer"]["guest_reader_pc"], None)
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_fetch_constant_join_is_exact_and_fail_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-fetch-constant-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "5b7b7959ef24f45abffe2019d8ef2f268afebd25718e72b0e3a63fde4ff7497f")
        self.assertEqual(receipt["source"]["main_ib_sha256"],
                         "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6")
        self.assertEqual(receipt["source"]["packet_offset_dword"], 408)
        self.assertEqual(receipt["decoded_fields"]["base_address"], "0x1374A000")
        self.assertEqual(receipt["decoded_fields"]["pitch_pixels_calculated"], 1280)
        self.assertEqual(receipt["decoded_fields"]["width"], 1280)
        self.assertEqual(receipt["decoded_fields"]["height"], 720)
        self.assertTrue(receipt["cross_checks"]["fetch_words_byte_identical_to_vd_swap"])
        self.assertTrue(receipt["implementation"]["validation"]["changed_fetch_traps"])
        self.assertTrue(receipt["policy"]["runtime_guard_fail_closed"])

    def test_copy_resolve_profile_is_exact_but_pixels_remain_unknown(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-copy-resolve-profile-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                     "56eb728585ed820bec4fa334f7dbf245357adb26fb9cf17ae20c0ef64fb155d8")
        self.assertEqual(receipt["source"]["main_ib_sha256"],
                         "d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6")
        copy = receipt["copy_state"]
        self.assertEqual((copy["copy_src_select"], copy["copy_sample_select"],
                          copy["copy_command_generic"]), (0, 0, "convert"))
        self.assertEqual((copy["copy_dest_pitch_pixels"], copy["copy_dest_height"],
                          copy["copy_dest_format_raw"]), (1280, 720, 6))
        self.assertEqual((copy["copy_dest_endian_raw"], copy["copy_dest_swap"]),
                         (0, 1))
        self.assertEqual(receipt["source_render_target"]["source_select_generic"],
                         "color RT0")
        self.assertIn("contents of EDRAM color RT0 before the copy",
                      receipt["classification"]["unknown"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_xma_late_aperture_probe_is_ab_identical_and_fail_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-xma-late-aperture-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "89095136919676de3391e05f3dbfe46323ce1657aa876c6a904d6505bf48ad1c")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertTrue(receipt["scope"]["fresh_store_per_route"])
        self.assertFalse(receipt["scope"]["rr_reexecuted"])
        self.assertEqual(receipt["scope"]["binary_sha256"],
                         "d80c12c25729d4f008d7471ea73c7ca07850f652a79560d5c5b28bfbb25c9edb")
        self.assertEqual([r["base"] for r in receipt["watched_ranges"]],
                         ["0x7FEA1A40", "0x7FEA1940", "0x7FEA1804", "0x7FEA1818"])
        neutral = receipt["routes"]["neutral"]
        start = receipt["routes"]["start"]
        self.assertEqual((neutral["return_code"], start["return_code"],
                          neutral["completed_ticks"], start["completed_ticks"],
                          neutral["presents"], start["presents"]), (3, 3, 1048, 1048, 911, 911))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        self.assertEqual(neutral["late_accesses"], start["late_accesses"])
        self.assertEqual(neutral["late_accesses"][0], {
            "op": "store32", "address": "0x7FEA1A80", "size": 4,
            "value_wire": "0x01000000", "pc": "0x82357240",
            "lr": "0x823572AC", "thread": 21, "tick": 1048,
            "function": "__imp__sub_82357240", "line": 14268,
        })
        self.assertEqual((neutral["fixed_1804_hits"], neutral["fixed_1818_hits"],
                          start["fixed_1804_hits"], start["fixed_1818_hits"]),
                         (0, 0, 0, 0))
        self.assertTrue(receipt["comparison"]["frontier_equal"])
        self.assertTrue(receipt["comparison"]["late_access_stream_equal"])
        self.assertFalse(receipt["comparison"]["effect_observed"])
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertFalse(receipt["policy"]["mmio_mapping_added"])

    def test_static_voicepacks_are_bounded_dual_oracle_only(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-xma-static-voicepacks-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "5793bcfe7127af10f83c1d6f216f74c09a7939fcf5efa9182a9766268d34fed7")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["packs"]["eng"]["riff_count"], 738)
        self.assertEqual(receipt["packs"]["jpn"]["riff_count"], 738)
        for language in ("eng", "jpn"):
            pack = receipt["packs"][language]
            self.assertEqual((pack["vgmstream_opened"], pack["ffprobe_opened"]),
                             (738, 738))
            self.assertEqual((pack["codec"], pack["format_tag"],
                              pack["sample_rate_hz"], pack["channels"]),
                             ("xma1", "0x0165", 48000, 1))
            self.assertEqual(len(pack["sampled_segments"]), 3)
        self.assertTrue(receipt["validation"]["all_riff_records_bounded"])
        self.assertTrue(receipt["validation"]["all_segments_contiguous"])
        self.assertTrue(receipt["validation"]["vgmstream_metadata_only"])
        self.assertFalse(receipt["validation"]["pcm_or_wav_written"])
        self.assertFalse(receipt["validation"]["runtime_consumer_joined"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_xma_kick_optin_is_exact_and_default_guarded(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-xma-kick-optin-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "c8d5cd4136393c0f0afee0aadfe2d100123e1204e4072133634ca45b8ada4a71")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        experiment = receipt["experiment"]
        self.assertEqual((experiment["address"], experiment["length"],
                          experiment["wire_value"], experiment["logical_value"],
                          experiment["pc"], experiment["lr"],
                          experiment["thread"], experiment["tick"]),
                         ("0x7FEA1A80", 4, "0x01000000", "0x00000001",
                          "0x82357240", "0x823572AC", 21, 1048))
        self.assertTrue(experiment["accepted_before_effect"])
        self.assertEqual(experiment["next_frontier"]["ordinal"], 548)
        self.assertTrue(receipt["scope"]["fresh_store_per_route"])
        self.assertTrue(receipt["scope"]["production_route_unchanged"])
        neutral = receipt["routes"]["neutral"]
        start = receipt["routes"]["start"]
        self.assertEqual((neutral["return_code"], start["return_code"],
                          neutral["completed_ticks"], start["completed_ticks"],
                          neutral["presents"], start["presents"]),
                         (3, 3, 1048, 1048, 911, 911))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        guard = receipt["routes"]["default_guard"]
        self.assertEqual((guard["return_code"], guard["completed_ticks"],
                          guard["presents"], guard["frontier"]),
                         (3, 1048, 911, "xboxkrnl.exe ordinal 548"))
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertTrue(receipt["policy"]["opt_in_only"])
        self.assertTrue(receipt["policy"]["no_retail"])

    def test_xma_three_slot_optin_stays_bounded_and_fail_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-xma-three-slot-optin-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "06ea6535dd199da1cd780538cd1a4a20b40ff27c2dc78ded5969939e8d52f552")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["experiment"]["acceptance"], {
            "address": "0x7FEA1A80", "length": 4,
            "order": [1, 2, 4], "default_route_unchanged": True,
            "production_enabled": False,
        })
        self.assertEqual([
            (slot["slot"], slot["context"], slot["wire"], slot["logical"])
            for slot in receipt["experiment"]["slots"]
        ], [
            ("0x17360050", "0x2E800000", "0x01000000", 1),
            ("0x173600B0", "0x2E800040", "0x02000000", 2),
            ("0x17360110", "0x2E800080", "0x04000000", 4),
        ])
        neutral = receipt["routes"]["neutral"]
        start = receipt["routes"]["start"]
        self.assertEqual((neutral["return_code"], start["return_code"],
                          neutral["completed_ticks"], start["completed_ticks"],
                          neutral["presents"], start["presents"]),
                         (4, 4, 1100, 1100, 1, 1))
        self.assertEqual(neutral["stderr_sha256"], start["stderr_sha256"])
        self.assertTrue(receipt["graphics"]["normalized_equal_neutral_start"])
        self.assertFalse(receipt["graphics"]["pixels_qualified"])
        self.assertEqual(receipt["frontier"]["scheduler"],
                         {"threads": 23, "runnable": 0,
                          "blocked": 23, "finished": 0})
        self.assertEqual(receipt["frontier"]["indirect_target"], "0x822F8848")
        self.assertEqual(receipt["frontier"]["wait_key"], "0xE000004C")
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertTrue(receipt["policy"]["opt_in_only"])
        self.assertTrue(receipt["policy"]["no_retail"])

    def test_event_handoff_frontier_is_observed_without_xenia_or_ptrace(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-event-handoff-xma-frontier-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "c8160645cd23e936d5ac924aaf93e88dcc8fbb0a8ba9bca54b26bb86f877abf9")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertFalse(receipt["scope"]["xenia_patch_applied"])
        self.assertFalse(receipt["scope"]["xenia_executed"])
        self.assertFalse(receipt["scope"]["ptrace_used"])
        self.assertTrue(receipt["scope"]["production_route_changed"] is False)
        window = receipt["window"]
        self.assertEqual((window["first_tick"], window["last_tick"],
                          window["rows_per_route"], window["handles"]),
                         (1040, 1099, 600,
                          {"signal": "0xE0000048", "wait": "0xE000004C"}))
        self.assertEqual(window["operations"], {
            "set_enter": 120, "event_wake": 120, "set_exit": 120,
            "signal_wait_enter": 120, "signal_wait_resume": 60,
            "signal_wait_block": 60,
        })
        self.assertTrue(window["neutral_start_rows_identical"])
        self.assertFalse(window["lost_wakeup_observed"])
        self.assertEqual(receipt["frontier"]["scheduler"],
                         {"threads": 23, "runnable": 0,
                          "blocked": 23, "finished": 0})
        self.assertTrue(receipt["policy"]["read_only"])
        self.assertTrue(receipt["policy"]["no_xenia_patch_applied"])
        self.assertTrue(receipt["policy"]["no_ptrace"])

    def test_dynamic_object_vtable_join_is_stable_and_read_only(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-dynamic-object-vtable-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "85302fbd9eab150d72e2261ed920bfd9db27293d0950595b20ab679972b50390")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["binary_sha256"],
                         "43677602c888cf481e0d0498a19a99cdda201526d306a8e76c860a6962e164d0")
        self.assertTrue(receipt["scope"]["read_only"])
        self.assertFalse(receipt["scope"]["xenia_executed"])
        self.assertFalse(receipt["scope"]["xenia_patch_applied"])
        self.assertFalse(receipt["scope"]["ptrace_used"])
        self.assertFalse(receipt["scope"]["production_route_changed"])
        for route in ("neutral", "start"):
            observed = receipt["routes"][route]
            self.assertEqual((observed["outcome"], observed["completed_ticks"],
                              observed["indirect_observations"],
                              observed["distinct_observation_ticks"]),
                             ("max_ticks", 1100, 853, 851))
            self.assertEqual(observed["observation_tick_range"], [0, 1099])
        self.assertTrue(receipt["routes"]["neutral_start_stderr_byte_identical"])
        join = receipt["dynamic_join"]
        self.assertEqual((join["callsite_lr"], join["indirect_target"],
                          join["object"], join["vtable"], join["slot"],
                          join["slot_target"]),
                         ("0x822E559C", "0x822F8848", "0x82934280",
                          "0x8202A488", 4, "0x822F8848"))
        self.assertTrue(join["observed_on_both_routes"])
        self.assertTrue(join["value_stable_across_all_observations"])
        self.assertEqual(receipt["classification"]["xenia-generic"], [])
        self.assertIn("semantic role and ABI contract of 0x822F8848",
                      receipt["classification"]["unknown"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_thread_affinity_body_state_keeps_renderer_and_xenia_unknown(self):
        path = WORKSPACE / "analysis/demo" / \
            "ac6-demo-thread-affinity-body-state-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(
            hashlib.sha256(encoded).hexdigest(),
            "9d763e66bbd1360f9f743464b7750e7b0e7c6dcfd0a3498df9b01a1bf5208257",
        )
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["binary_sha256"],
                         "ca3180188d24c0bc3f2598c15e2a34390007ad3fc869956d457ef1c55bcf8d4e")
        self.assertTrue(receipt["scope"]["read_only"])
        self.assertFalse(receipt["scope"]["xenia_executed"])
        self.assertFalse(receipt["scope"]["xenia_patch_applied"])
        self.assertFalse(receipt["scope"]["ptrace_used"])
        self.assertEqual(receipt["scheduler"]["neutral"], {
            "threads": 23, "runnable": 0, "blocked": 23, "finished": 0,
            "slice_exhaustions": 261, "last_slice_activations": 18,
        })
        self.assertTrue(receipt["scheduler"]["neutral_start_equal"])
        affinity = receipt["affinity_observation"]
        self.assertEqual((affinity["import"], affinity["ordinal"]),
                         ("xboxkrnl.exe:KeSetAffinityThread", 151))
        self.assertEqual(affinity["processor_mapping_status"], "unknown")
        body = receipt["body_state"]
        self.assertEqual((body["indirect_target"], body["observations_per_route"],
                          body["store_rows_per_route"], body["distinct_store_addresses"]),
                         ("0x822F8848", 853, 8530, 10))
        self.assertEqual(body["store_values"],
                         ["0x00000000", "0x829342A0", "0x82934500"])
        self.assertTrue(body["neutral_start_equal"])
        self.assertEqual(receipt["classification"]["xenia-generic"], [
            "Xenon six-hardware-thread topology and any vCPU scheduling design"
        ])
        self.assertIn("first nonzero EDRAM writer and pixel contents",
                      receipt["classification"]["unknown"])
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertTrue(receipt["policy"]["no_xenia_patch_or_ptrace"])

    def test_thread_affinity_static_dynamic_join_is_pal_qualified(self):
        path = WORKSPACE / "analysis/demo" / \
            "ac6-demo-affinity-static-dynamic-join-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(
            hashlib.sha256(encoded).hexdigest(),
            "868a080fa2c7f57d96982d12341727c563db08dfa082994474c311f322f695ff",
        )
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertTrue(receipt["scope"]["read_only"])
        self.assertFalse(receipt["scope"]["xenia_executed"])
        self.assertFalse(receipt["scope"]["xenia_patch_applied"])
        self.assertFalse(receipt["scope"]["ptrace_used"])
        self.assertFalse(receipt["scope"]["retail_evidence_merged"])
        static = receipt["static_pal_join"]
        self.assertEqual((static["entry"], static["call_instruction"]),
                         ("0x821A5390", "0x821A53DC"))
        self.assertEqual(static["classification"], "demo-qualified")
        dynamic = receipt["dynamic"]
        self.assertEqual(dynamic["rows_per_route"], 19)
        self.assertTrue(dynamic["neutral_start_byte_identical"])
        self.assertEqual(dynamic["raw_mask_set"], [
            "0x00000001", "0x00000002", "0x00000004",
            "0x00000008", "0x00000010", "0x00000020",
        ])
        self.assertEqual(dynamic["scheduler_terminal"], {
            "threads": 23, "runnable": 0, "blocked": 23, "finished": 0,
        })
        self.assertEqual(receipt["experimental_stateful_attempt"]["status"],
                         "rejected")
        self.assertTrue(receipt["policy"]["fail_closed"])
        self.assertFalse(receipt["policy"]["retail_evidence_merged"])

    def test_xenia_xma_crossmatch_stays_generic_and_fail_closed(self):
        path = WORKSPACE / "analysis/demo" / "ac6-demo-xma-xenia-generic-crossmatch-v1.json"
        encoded = path.read_bytes()
        receipt = json.loads(encoded)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "cd1ea765ff2063d71f4c6c70cab8ef315c9eae317e05ece28f7edba293c61363")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["generic_authorities"]["xenia"]["commit"],
                         "95a5c3ee250f80c3b9d139658649d9ffb6db3eec")
        self.assertEqual(receipt["generic_authorities"]["rexglue"]["commit"],
                         "cb58065c793429aa92895d778af58d12e9d26d8f")
        self.assertEqual(
            receipt["xenia_generic"]["calls"]["XMAInitializeContext"]["base_reg"],
            "0x1A80")
        self.assertEqual(receipt["demo_evidence"]["pal_dynamic_observation"]["A"],
                         "0x7FEA1A80")
        self.assertEqual(receipt["demo_evidence"]["pal_dynamic_observation"]["V_wire"],
                         "0x01000000")
        self.assertIn("whether PAL 0x7FEA1A80 is the same register as Xenia base 0x1A80",
                      receipt["classification"]["unknown"])
        self.assertFalse(receipt["policy"]["generic_mapping_installed"])
        self.assertFalse(receipt["policy"]["mmio_mapping_added"])
        self.assertTrue(receipt["policy"]["ordinal_548_trap_preserved"])
        self.assertTrue(receipt["policy"]["fail_closed"])

    def test_sdk_callgraph_is_qualified_and_deterministic(self):
        graph_path = WORKSPACE / "analysis/demo/ac6-demo-sdk-callgraph.json"
        evidence = json.loads((WORKSPACE / "analysis/demo" /
                               "ac6-demo-sdk-callgraph-evidence.json").read_text())
        graph_bytes = graph_path.read_bytes()
        graph = json.loads(graph_bytes)
        self.assertEqual(graph["schema"], "ac6-demo-sdk-callgraph/v1")
        self.assertEqual(graph["identity"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(hashlib.sha256(graph_bytes).hexdigest(),
                         evidence["artifacts"]["callgraph"]["sha256"])
        self.assertTrue(evidence["determinism"]["byte_identical"])
        self.assertEqual(evidence["validation"]["result"],
                         "CALLGRAPH_VALIDATION_PASS")
        self.assertEqual(graph["counts"], {
            "callable_imports": 228,
            "edges": 1853,
            "imports": 238,
            "owner_nodes": 743,
            "unresolved_indirect_callsites": 320,
            "variable_imports": 10,
            "xam_imports": 87,
            "xboxkrnl_imports": 151,
        })
        xam_state = next(item for item in graph["imports"]
                         if item["name"] == "XamInputGetState")
        self.assertEqual(xam_state["stub_address"], "0x823764E4")
        self.assertEqual(xam_state["wrapper_frontier_edge_ids"], [
            "edge:0x822F60A4->function:0x82338248:direct_call",
            "edge:0x822F6168->function:0x82338248:direct_call",
        ])
        self.assertEqual(evidence["indirect_policy"]["qualified_indirect_edges"],
                         0)

    def test_edram_source_command_join_is_demo_qualified(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-edram-source-command-join-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["id"], "ac6-demo-xbox360-pal")
        self.assertEqual(receipt["target"]["module"], "Default.xex")
        self.assertEqual(receipt["target"]["basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["target"]["ghidra_project"],
                         "ace-combat-6-demo")
        self.assertEqual(receipt["target"]["ghidra_manifest_sha256"],
                         "576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c")
        self.assertEqual(receipt["target"]["codegen_manifest_sha256"],
                         "9f1fffb0398358331f9bbf575a3d2fb5cf1478f7cbda5a1dbe46c264a935bbfa")
        self.assertEqual(receipt["target"]["boundary_config_sha256"],
                         "4a87dd8c377638f6bddf308ee63025e314c5a3470507767070e2ad15c0e54506")
        self.assertEqual(hashlib.sha256(
            (WORKSPACE / "analysis/demo/ac6-demo-edram-source-command-join-v1.json").read_bytes()
        ).hexdigest(), "6fda1c1d53406f8bdc8ba68a1dca0fae3ebcf12f91d69510f13b7ea78c23d160")
        self.assertEqual([item["offset_dword"] for item in receipt["join"]],
                         [239, 387])
        self.assertEqual([item["guest_writer"]["store_pc"]
                          for item in receipt["join"]],
                         ["0x821B5840", "0x821B7C04"])
        self.assertTrue(receipt["determinism"]["trace_byte_identical"])
        self.assertTrue(receipt["policy"]["read_only_probe"])
        self.assertFalse(receipt["policy"]["xenia_used"])
        self.assertFalse(receipt["policy"]["retail_evidence_used"])
        self.assertFalse(receipt["policy"]["supported_promoted"])

        profile = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-copy-resolve-profile-v1.json").read_text())
        copy_draw = next(item for item in profile["ordered_packets"]
                         if item.get("role") == "RB_COPY")
        self.assertEqual(copy_draw["offset_dword"], 387)
        self.assertEqual(copy_draw["opcode"], "0x36")
        self.assertEqual(copy_draw["register_snapshot_sha256"],
                         profile["artifacts"]["rb_copy_draw_register_snapshot_sha256"])

    def test_normal_draw_coverage_receipt_is_fail_closed(self):
        receipt = json.loads((WORKSPACE / "analysis/demo" /
                              "ac6-demo-normal-draw-coverage-v1.json").read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["module"], "Default.xex")
        self.assertEqual(receipt["target"]["ghidra_project"], "ace-combat-6-demo")
        self.assertEqual(receipt["target"]["basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["scope"]["codegen_manifest_sha256"],
                         "9f1fffb0398358331f9bbf575a3d2fb5cf1478f7cbda5a1dbe46c264a935bbfa")
        self.assertEqual(receipt["artifacts"]["ghidra_manifest_sha256"],
                         "576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c")
        self.assertEqual(hashlib.sha256(
            (WORKSPACE / "analysis/demo/ac6-demo-normal-draw-coverage-v1.json").read_bytes()
        ).hexdigest(), "9f3607c4d544b795dbddee5ee54be0732d342bde49c791b3aa3d52dc9ce7f62f")
        result = receipt["result"]
        self.assertEqual(result["passed_samples"],
                         result["width"] * result["height"] *
                         result["samples_per_pixel"])
        self.assertEqual(result["black_pixels"],
                         result["width"] * result["height"])
        self.assertEqual(result["sentinel_pixels"], 0)
        self.assertEqual(result["other_pixels"], 0)
        self.assertTrue(receipt["determinism"]["stderr_byte_identical"])
        self.assertFalse(receipt["policy"]["synthetic_pixels"])
        self.assertFalse(receipt["policy"]["supported_promoted"])

    def test_start_neutral_5600_ab_receipt_is_qualified(self):
        receipt_path = WORKSPACE / "analysis/demo/ac6-demo-start-neutral-5600-ab-v1.json"
        receipt = json.loads(receipt_path.read_text())
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["target"]["id"], "ac6-demo-xbox360-pal")
        self.assertEqual(receipt["target"]["module"], "Default.xex")
        self.assertEqual(receipt["target"]["ghidra_project"], "ace-combat-6-demo")
        self.assertEqual(receipt["target"]["basefile_sha256"],
                         "b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218")
        self.assertEqual(receipt["artifacts"]["codegen_manifest_sha256"],
                         "9f1fffb0398358331f9bbf575a3d2fb5cf1478f7cbda5a1dbe46c264a935bbfa")
        self.assertEqual(receipt["artifacts"]["ghidra_manifest_sha256"],
                         "576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c")
        self.assertEqual(receipt["artifacts"]["boundary_config_sha256"],
                         "4a87dd8c377638f6bddf308ee63025e314c5a3470507767070e2ad15c0e54506")
        self.assertEqual(hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
                         "3bde5cd5f116c61d660c6f6751dc6b5edce6c4e58b8fc8530ce6b51ced4d1f3b")
        self.assertEqual(receipt["result"]["presents"], 5463)
        self.assertEqual(receipt["result"]["milestones"],
                         {"frontend": False, "mission": False, "terminal": False})
        self.assertEqual(receipt["scope"]["max_ticks"], 5600)
        self.assertEqual(receipt["scope"]["input_divergence"], {
            "trace_sequence": 760, "tick": 252,
            "neutral_buttons": 0, "buttons": 16,
        })
        self.assertEqual(receipt["result"]["scheduler"],
                         {"threads": 23, "blocked": 23, "runnable": 0})
        self.assertTrue(receipt["result"]["graphics_identical"])
        self.assertTrue(receipt["result"]["xma_stderr_identical"])
        self.assertFalse(receipt["policy"]["frontend_promoted"])
        self.assertFalse(receipt["policy"]["native_parity_promoted"])
        self.assertEqual(receipt["artifacts"]["neutral_report_sha256"],
                         "99dba4f56917bbd6f6569718d8788eec5ce52ea17f3788d719f32abb12396533")

    def test_cycle_1760_event_handoff_receipt_is_bounded_and_linked(self):
        receipt_path = WORKSPACE / "reports/cycle-1760-ac6-demo-event-handoff.json"
        receipt = json.loads(receipt_path.read_text())
        self.assertEqual(receipt["schema"], "ac6-demo-event-handoff/v1")
        self.assertEqual(receipt["cycle"], 1760)

        target = receipt["target"]
        self.assertEqual(target["id"], "ac6-demo-xbox360-pal")
        self.assertEqual(target["module"], "Default.xex")
        self.assertEqual(target["ghidra_project"], "ace-combat-6-demo")
        self.assertEqual(target["xex_sha256"], BUILD.EXPECTED_XEX)

        # Cross-check immutable artifact identities against the preceding
        # qualified A/B receipt; do not pin this receipt's self-digest.
        prior = json.loads((WORKSPACE /
                            "analysis/demo/ac6-demo-start-neutral-5600-ab-v1.json"
                            ).read_text())
        self.assertEqual(target["basefile_sha256"],
                         prior["target"]["basefile_sha256"])
        for key in ("codegen_manifest_sha256", "ghidra_manifest_sha256",
                    "boundary_config_sha256"):
            self.assertEqual(target[key], prior["artifacts"][key])
        for key in ("xex_sha256", "basefile_sha256", "codegen_manifest_sha256",
                    "ghidra_manifest_sha256", "boundary_config_sha256",
                    "binary_sha256"):
            value = target[key]
            self.assertIsInstance(value, str)
            self.assertEqual(len(value), 64)
            self.assertTrue(all(char in "0123456789abcdef" for char in value))

        scope = receipt["scope"]
        self.assertEqual(scope["routes"], ["neutral", "buttons_16"])
        self.assertEqual(scope["backend"], "vulkan")
        self.assertEqual(scope["max_ticks"], 5600)
        self.assertIn("AC6_DEMO_WATCH_EVENT_HANDOFF_FOCUSED=1",
                      scope["environment"])

        for route, prior_prefix in (("neutral", "neutral"),
                                    ("buttons_16", "buttons_16")):
            observed = receipt["routes"][route]
            self.assertEqual(observed["return_code"], 4)
            self.assertEqual(observed["report_sha256"],
                             prior["artifacts"][f"{prior_prefix}_report_sha256"])
            self.assertEqual(observed["trace_sha256"],
                             prior["artifacts"][f"{prior_prefix}_trace_sha256"])
            for key in ("report_sha256", "trace_sha256", "stderr_sha256"):
                value = observed[key]
                self.assertEqual(len(value), 64)
                self.assertTrue(all(char in "0123456789abcdef" for char in value))
        self.assertEqual(receipt["routes"]["neutral"]["stderr_sha256"],
                         receipt["routes"]["buttons_16"]["stderr_sha256"])

        result = receipt["result"]
        self.assertEqual(result["outcome"], "max_ticks")
        self.assertEqual(result["completed_ticks"], scope["max_ticks"])
        self.assertEqual(result["presents"], 5463)
        self.assertEqual(result["scheduler"],
                         {"threads": 23, "blocked": 23,
                          "runnable": 0, "finished": 0})
        handoff = result["event_handoff"]
        self.assertEqual((handoff["trace_record_cap"],
                          handoff["last_covered_tick"],
                          handoff["run_max_ticks"]), (32768, 1212, 5600))
        self.assertEqual(handoff["focused_log_ticks"], [0, 1212])
        self.assertEqual(handoff["last_covered_tick"],
                         handoff["focused_log_ticks"][-1])
        self.assertLess(handoff["last_covered_tick"], handoff["run_max_ticks"])
        self.assertEqual((handoff["set_enter"], handoff["event_wake"],
                          handoff["set_exit"]), (2176, 2176, 2176))
        self.assertEqual((handoff["signal_wait_block"],
                          handoff["wake_waiter_1"]), (963, 963))
        self.assertEqual((handoff["signal_wait_resume"],
                          handoff["resume_thread_1"],
                          handoff["complete_set_wake_resume_chains"]),
                         (962, 962, 962))

        self.assertFalse(result["post_resume_guest_access_observed"])
        self.assertEqual(result["post_resume_guest_access_observed_scope"],
                         "bounded_event_handoff_capsule")
        self.assertIn("does not establish",
                      result["post_resume_guest_access_observed_note"])
        self.assertEqual(result["decision"],
                         "STOP_kernel_XAM_event_transport_boundary")
        self.assertEqual(result["decision_scope"], "observation_boundary_only")
        self.assertIn("not a complete semantic conclusion",
                      result["decision_note"])
        self.assertFalse(receipt["policy"]["supported"])
        self.assertFalse(receipt["policy"]["xenia_used"])

        source_report = receipt["source_report"]
        self.assertEqual(source_report,
                         "workspaces/ace-combat-6/reports/"
                         "cycle-1760-ac6-demo-event-handoff.md")
        portfolio = WORKSPACE.parents[1]
        self.assertTrue((portfolio / source_report).is_file())
        current = json.loads((portfolio / "reports/handoff/CURRENT.json").read_text())
        current_ac6 = next(item for item in current["targets"] if item["id"] == "ac6")
        self.assertEqual(current_ac6["cycle"], receipt["cycle"])
        self.assertEqual(current_ac6["source_report"], source_report)

    def test_cycle_1761_post_resume_one_shot_receipt_is_bounded_and_linked(self):
        receipt_path = WORKSPACE / "reports/cycle-1761-ac6-demo-post-resume-one-shot.json"
        receipt = json.loads(receipt_path.read_text())
        self.assertEqual(receipt["schema"], "ac6-demo-post-resume-one-shot-report/v1")
        self.assertEqual(receipt["cycle"], 1761)
        self.assertFalse(receipt["supported"])
        self.assertEqual(receipt["target"]["id"], "ac6-demo-xbox360-pal")
        self.assertEqual(receipt["target"]["project"], "ace-combat-6-demo")
        self.assertEqual(receipt["target"]["xex_sha256"], BUILD.EXPECTED_XEX)
        self.assertEqual(receipt["scope"]["max_ticks"], 5600)
        self.assertEqual(receipt["scope"]["presents_per_route"], 5463)
        self.assertEqual(receipt["scope"]["scheduler_per_route"],
                         {"threads": 23, "blocked": 23, "runnable": 0, "finished": 0})
        self.assertEqual(receipt["post_resume_boundary"]["access_count_per_route"], 1)
        self.assertEqual(receipt["post_resume_boundary"]["handoff_count_per_route"], 1)
        access = receipt["post_resume_boundary"]["access"]
        self.assertEqual((access["address"], access["guest_pc"], access["instruction_bytes"]),
                         ("0x7F0409D8", "0x82327154", "eb 61 ff d0"))
        self.assertTrue(receipt["post_resume_boundary"]["handoff"]["lr_is_not_pc"])
        self.assertEqual(receipt["comparison"]["report_subtrees_json_object_equal"],
                         ["outcome", "milestones", "graphics", "scheduler"])
        self.assertFalse(receipt["comparison"]["traces_equal"])
        self.assertFalse(receipt["comparison"]["frontend"])
        self.assertFalse(receipt["comparison"]["mission"])
        self.assertFalse(receipt["comparison"]["terminal"])
        self.assertEqual(receipt["provenance_only"]["xvfb_lifecycle_cases"], 13)
        self.assertEqual(receipt["post_resume_boundary"]["mapper"]["mapper_tests"], 9)
        source = WORKSPACE / "analysis/demo/ac6-demo-post-resume-ab/sha256/940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8/receipt.json"
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         receipt["source_master"]["sha256"])
        current = json.loads((WORKSPACE.parents[1] / "reports/handoff/CURRENT.json").read_text())
        current_ac6 = next(item for item in current["targets"] if item["id"] == "ac6")
        self.assertEqual(current_ac6["cycle"], 1761)
        self.assertEqual(current_ac6["source_report"],
                         "workspaces/ace-combat-6/reports/cycle-1761-ac6-demo-post-resume-one-shot.md")


if __name__ == "__main__":
    unittest.main()
