from __future__ import annotations

import importlib.util
import hashlib
import json
import tempfile
import unittest
from subprocess import CompletedProcess
from pathlib import Path
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "tools/run_gate.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_run_gate", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class RunGateTests(unittest.TestCase):
    def test_missing_route_sync_markers_reports_exact_capability(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "host"
            binary.write_bytes(b"prefix type28= selector44= suffix")
            self.assertEqual(
                MODULE.missing_route_sync_markers(binary),
                [
                    "state40=", "[ac6-campaign-transition]", "[ac6-visual-phase]",
                    "[ac6-current-level]", "[ac6-current-level-set]",
                    "[ac6-save-manager]", "[ac6-post-mission]",
                ],
            )

    def test_missing_route_sync_markers_accepts_complete_host(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "host"
            binary.write_bytes(b"\0".join(MODULE.ROUTE_SYNC_MARKERS))
            self.assertEqual(MODULE.missing_route_sync_markers(binary), [])

    def test_static_receipt_from_another_target_is_rejected(self) -> None:
        manifest = {"schema": "ac6.retail-target.v1", "target": "ntsc-uj"}
        receipt = {"status": "pass", "target": "pal", "manifest": manifest}
        with self.assertRaisesRegex(RuntimeError, "target/manifest"):
            MODULE.validate_static_receipt("ntsc-uj", manifest, receipt)

    def test_v1_gameplay_receipt_is_rejected(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "legacy or unknown"):
            MODULE.validate_gameplay_receipt_schema({
                "schema": "ac6.retail-gameplay-gate.v1"
            })

    def test_gameplay_visuals_reject_cinematic_capture_names_and_phase(self) -> None:
        captures = {name: Path(name) for name in MODULE.GAMEPLAY_CAPTURES}
        with patch.object(MODULE, "gameplay_image_metrics", return_value={
            "mean": 0.2, "stddev": 0.2, "nonblack_fraction": 0.8,
            "hud_green_fraction": 0.01,
        }), patch.object(MODULE, "changed_pixels", return_value=9000):
            _, _, errors = MODULE.validate_gameplay_visuals(
                captures, ["cinematic=1 world=1 hud=1 stable=30"]
            )
        self.assertIn("gameplay phase never became stable for 30 frames", errors)

    def test_gameplay_visuals_reject_hud_over_black_world(self) -> None:
        captures = {name: Path(name) for name in MODULE.GAMEPLAY_CAPTURES}
        with patch.object(MODULE, "gameplay_image_metrics", return_value={
            "mean": 0.01, "stddev": 0.01, "nonblack_fraction": 0.1,
            "hud_green_fraction": 0.01,
        }), patch.object(MODULE, "changed_pixels", return_value=9000):
            _, _, errors = MODULE.validate_gameplay_visuals(
                captures, ["cinematic=0 world=1 hud=1 stable=30"]
            )
        self.assertEqual(len(errors), 3)

    def test_gameplay_visuals_reject_controls_without_visible_effect(self) -> None:
        captures = {name: Path(name) for name in MODULE.GAMEPLAY_CAPTURES}
        with patch.object(MODULE, "gameplay_image_metrics", return_value={
            "mean": 0.2, "stddev": 0.2, "nonblack_fraction": 0.8,
            "hud_green_fraction": 0.01,
        }), patch.object(MODULE, "changed_pixels", return_value=5000):
            _, _, errors = MODULE.validate_gameplay_visuals(
                captures, ["cinematic=0 world=1 hud=1 stable=30"]
            )
        self.assertTrue(any("controls change at most 5000 pixels" in e for e in errors))

    def test_gameplay_visuals_reject_world_without_hud_layer(self) -> None:
        captures = {name: Path(name) for name in MODULE.GAMEPLAY_CAPTURES}
        with patch.object(MODULE, "gameplay_image_metrics", return_value={
            "mean": 0.2, "stddev": 0.2, "nonblack_fraction": 0.8,
            "hud_green_fraction": 0.0,
        }), patch.object(MODULE, "changed_pixels", return_value=9000):
            _, _, errors = MODULE.validate_gameplay_visuals(
                captures, ["cinematic=0 world=1 hud=1 stable=30"]
            )
        self.assertIn("qualified green HUD layer is not visible over the world", errors)

    def test_qualified_route_is_v2_shape(self) -> None:
        operations = [line for line in MODULE.ROUTE.read_text().splitlines()
                      if line and not line.startswith("#")]
        captures = [line.split("\t", 1)[1] for line in operations
                    if line.startswith("capture\t")]
        self.assertEqual(len(operations), 96)
        self.assertEqual(len(captures), 27)
        self.assertEqual(captures[-11:-6], [f"cinematic-view-{n}" for n in range(1, 6)])
        self.assertEqual(captures[-6:], list(MODULE.GAMEPLAY_CAPTURES))
        self.assertNotIn("flight-hud-baseline", captures)

    def test_qualified_route_settles_before_escape(self) -> None:
        operations = [line for line in MODULE.ROUTE.read_text().splitlines()
                      if line and not line.startswith("#")]
        boundary = operations.index("capture\tcinematic-view-2") + 2
        self.assertEqual(operations[boundary], "sleep\t15")
        self.assertEqual(operations[boundary + 1], "capture\tcinematic-view-3")
        self.assertEqual(operations[boundary + 2], "capture\tcinematic-view-4")
        self.assertEqual(operations[boundary + 3], "capture\tcinematic-view-5")
        self.assertEqual(operations[boundary + 4], "key\tEscape\t0.6")

    def test_qualified_route_uses_stock_mnk_flight_bindings(self) -> None:
        operations = [line for line in MODULE.ROUTE.read_text().splitlines()
                      if line and not line.startswith("#")]
        expected = {
            "flight-pitch": "key\td\t1.0",
            "flight-roll": "key\tq\t1.0",
            "flight-yaw": "mouse\t1\t1.0",
            "flight-throttle": "mouse\t2\t1.0",
        }
        for capture, edge in expected.items():
            index = operations.index(f"capture\t{capture}")
            self.assertEqual(operations[index + 1], edge)

    def test_stock_input_log_is_read_only_seam_diagnostic(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-stock-input-log", source)
        self.assertIn('"--ac6_kbm_log=true"', source)
        self.assertIn("Windows-only AC6 keyboard injector", source)

    def test_stock_input_record_extends_only_the_startup_wait(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-stock-input-record", source)
        self.assertIn('"Escape@600"', source)
        self.assertIn("Recording serializes every qualified poll", source)

    def test_probe_key_transform_uses_capture_anchor(self) -> None:
        steps = [
            ("capture", "post-weapon-confirm", ""),
            ("sleep", "1", ""),
            ("key", "space", "0.1"),
        ]
        self.assertEqual(
            MODULE.replace_first_key_after_capture(
                steps, "post-weapon-confirm", "space", "0.6"
            ),
            [
                ("capture", "post-weapon-confirm", ""),
                ("sleep", "1", ""),
                ("key", "space", "0.6"),
            ],
        )

    def test_cache_seed_requires_both_host_cache_trees(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            seed = Path(temporary)
            (seed / "cache").mkdir()
            with self.assertRaisesRegex(RuntimeError, "cache-root"):
                MODULE.validate_cache_seed(seed)
            (seed / "cache-root").mkdir()
            MODULE.validate_cache_seed(seed)

    def test_empty_storage_manifest_matches_replay_protocol(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            self.assertEqual(
                MODULE.tree_manifest_sha256(Path(temporary)),
                hashlib.sha256(b"").hexdigest(),
            )

    def test_storage_manifest_rejects_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "file").write_text("state")
            (root / "link").symlink_to(root / "file")
            with self.assertRaisesRegex(RuntimeError, "symlink"):
                MODULE.tree_manifest_sha256(root)

    def test_replay_header_loader_requires_v4(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            replay = Path(temporary) / "replay.jsonl"
            replay.write_text(json.dumps({"schema": "legacy"}) + "\n")
            with self.assertRaisesRegex(RuntimeError, "schema"):
                MODULE.load_replay_header(replay)

    def test_full_mission_recording_route_accepts_512_steps(self) -> None:
        steps = [("sleep", "1", "")] * (512 - len(MODULE.FULL_MISSION_CAPTURES))
        steps.extend(("capture", name, "") for name in MODULE.FULL_MISSION_CAPTURES)
        MODULE.validate_full_mission_route(steps)

    def test_full_mission_recording_route_requires_release_captures(self) -> None:
        steps = [("capture", name, "") for name in MODULE.GAMEPLAY_CAPTURES]
        with self.assertRaisesRegex(RuntimeError, "debrief, post-mission"):
            MODULE.validate_full_mission_route(steps)

    def test_full_mission_recording_route_rejects_more_than_512_steps(self) -> None:
        steps = [("sleep", "1", "")] * 513
        steps.extend(("capture", name, "") for name in MODULE.FULL_MISSION_CAPTURES)
        with self.assertRaisesRegex(RuntimeError, "1..512"):
            MODULE.validate_full_mission_route(steps)

    def test_campaign_interface_is_fail_closed(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn('"--mission-id"', source)
        self.assertIn('"--input-replay"', source)
        self.assertIn('"--storage-seed"', source)
        self.assertIn("Mission 02..15 require a strict --input-replay", source)
        self.assertIn("Mission 01 release validation must build its cache from empty", source)
        self.assertIn('"execution_mode": "strict-replay"', source)

    def test_renderdoc_wrapper_preserves_runner_working_directory(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn('"renderdoccmd", "capture", "--wait-for-exit"', source)
        self.assertIn('"--opt-disallow-vsync"', source)
        self.assertIn('["xdotool", "key", "F12"]', source)
        self.assertIn('"--working-dir", str(self.args.output)', source)

    def test_route_selects_yes_only_after_create_prompt_is_visible(self) -> None:
        operations = [
            line for line in MODULE.ROUTE.read_text().splitlines()
            if line and not line.startswith("#")
        ]
        prompt = operations.index("wait\ttype28=[^ ]*37\t20")
        self.assertEqual(operations[prompt + 1], "key\tLeft\t0.1")
        self.assertEqual(operations[prompt + 2], "key\tspace\t0.1")
        self.assertEqual(operations[prompt + 3], "wait\ttype28=[^ ]*35\t20")

    def test_clean_diagnostic_compares_selection_before_confirmation(self) -> None:
        route = MODULE.PRODUCT / "routes/us-type37-clean-capture.steps"
        operations = [
            line for line in route.read_text().splitlines()
            if line and not line.startswith("#")
        ]
        default = operations.index("capture\ttype28-37-default-no-clean")
        self.assertEqual(operations[default + 1], "key\tLeft\t0.1")
        self.assertEqual(
            operations[default + 3], "capture\ttype28-37-selected-yes-clean"
        )
        self.assertEqual(operations[default + 4], "key\tspace\t0.1")
        self.assertFalse(any(operation.startswith("present\t") for operation in operations))

    def test_runner_keeps_host_diagnostics_closure_diagnostic_only(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn('"--hide-diagnostics"', source)
        self.assertIn("close_graphics_diagnostics", source)
        self.assertIn("self.focus()", source)

    def test_graphics_diagnostics_are_disabled_by_default_in_the_host(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/src/ac6_native_graphics_overlay.cpp"
        ).read_text()
        self.assertIn(
            "REXCVAR_DEFINE_BOOL(ac6_graphics_diagnostics, false", source
        )
        self.assertIn("REXCVAR_GET(ac6_graphics_diagnostics)", source)

    def test_qualified_vulkan_path_is_preserved_in_upstream_source(self) -> None:
        source = (MODULE.PRODUCT / "upstream/AC6_recomp/src/main.cpp").read_text()
        self.assertIn(
            'SetSessionDefault("render_target_path_vulkan", "fsi",', source
        )

    def test_vulkan_spirv_consumes_ac6_deswizzle_allowlist(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/pipeline/shader/"
            "spirv_translator_fetch.cpp"
        ).read_text()
        self.assertIn(
            "REXCVAR_DECLARE(std::string, ac6_neutralize_deswizzle_hashes)", source
        )
        self.assertIn("UcodeHashSlotInList", source)
        self.assertIn("GetPsParamGenInterpolator() != UINT32_MAX", source)
        self.assertIn("input_fragment_coordinates_", source)
        self.assertIn("host_size_x", source)
        self.assertIn("host_size_y", source)

    def test_vulkan_motion_blur_uses_defined_lod_for_single_mip_fetches(self) -> None:
        main_source = (
            MODULE.PRODUCT / "upstream/AC6_recomp/src/main.cpp"
        ).read_text()
        fetch_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/pipeline/shader/"
            "spirv_translator_fetch.cpp"
        ).read_text()
        self.assertIn(
            'REXCVAR_DEFINE_BOOL(ac6_fix_motion_blur, true', main_source
        )
        self.assertIn(
            '"08694231e8d17665:2+12", "ac6_fix_motion_blur"', main_source
        )
        self.assertIn("force_explicit_lod_zero", fetch_source)
        self.assertIn(
            "texture_parameters.lod = force_explicit_lod_zero ? const_float_0_ : lod",
            fetch_source,
        )

    def test_vulkan_present_uses_the_guest_composed_frontbuffer_by_default(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "texture_cache.cpp"
        ).read_text()
        self.assertIn(
            "REXCVAR_DEFINE_BOOL(ac6_present_compose_fallback, false", source
        )

    def test_startup_diagnostic_mirrors_the_sealed_route_prefix(self) -> None:
        route = MODULE.PRODUCT / "routes/us-pretype28-startup.steps"
        operations = [
            line for line in route.read_text().splitlines()
            if line and not line.startswith("#")
        ]
        self.assertEqual(operations[:2], ["key\tEscape\t0.1", "sleep\t2"])
        self.assertIn("key\tspace\t0.1", operations)
        self.assertIn("wait-pulse\ttype28=30\tEscape+space", operations)

    def test_changed_pixels_accepts_imagemagick_normalized_metric(self) -> None:
        with patch.object(
            MODULE.subprocess,
            "run",
            return_value=CompletedProcess([], 1, "", "0 (0)"),
        ):
            self.assertEqual(MODULE.changed_pixels(Path("left"), Path("right")), 0)

    def test_mission_launch_probe_keeps_only_the_first_launch_confirmation(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-launch-hold", source)
        self.assertIn('steps[:72] + [("key", "space", launch_hold)] + steps[73:75]', source)
        self.assertIn("mission-load-100s", source)

    def test_mission_launch_long_press_is_available_for_the_single_a_boundary(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-launch-long-press", source)
        self.assertIn('launch_hold = "0.6" if arguments.mission_launch_long_press else "0.1"', source)

    def test_hangar_probe_confirms_before_the_route_returns_to_weapons(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-hangar-confirm", source)
        self.assertIn("steps = steps[:69]", source)
        self.assertIn('("capture", "post-hangar-a-65s", "")', source)

    def test_map_probe_adds_only_the_visible_second_confirmation(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-map-confirm", source)
        self.assertIn('("capture", "mission-tactical-map", "")', source)
        self.assertIn('("capture", "post-map-a-65s", "")', source)

    def test_cinematic_handoff_preserves_the_terminal_escape_only_recipe(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-cinematic-handoff", source)
        self.assertIn('("sleep", "20", ""),', source)
        self.assertIn('("wait-pulse", "type28=30", "Escape@90"),', source)
        self.assertIn("] + steps[4:69] + [", source)
        self.assertNotIn('("capture", "post-cinematic-a-15s", "")', source)
        self.assertIn('("capture", "cinematic-view-5", "")', source)
        self.assertIn('("sync-log", "", "")', source)
        self.assertIn('("key", "Escape", "0.6")', source)
        self.assertIn('("capture", "gameplay-hud", "")', source)
        self.assertIn('cinematic=0.*world=1.*hud=1.*stable=30", "120"', source)
        self.assertIn('OracleRun.capture(run, "failure-observation")', source)

    def test_sync_log_discards_historical_predicate_text(self) -> None:
        source = MODULE.RUNNER_PATH.read_text()
        self.assertIn('elif operation == "sync-log":', source)
        self.assertIn('self.pending_log_text = ""', source)
        self.assertIn("self.new_log_text()", source)

    def test_cinematic_handoff_settles_campaign_confirmation(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("campaign_capture = next(", source)
        self.assertIn('("sleep", "8", "")', source)
        self.assertIn('("key", "space", "0.6")', source)
        self.assertIn('"cinematic handoff campaign markers missing"', source)

    def test_wait_pulse_supports_a_bounded_retry_window(self) -> None:
        source = MODULE.RUNNER_PATH.read_text()
        self.assertIn('if "@" in limit:', source)
        self.assertIn('pulse, timeout_text = limit.rsplit("@", 1)', source)
        self.assertIn("timeout = min(timeout, float(timeout_text))", source)

    def test_render_summary_uses_the_same_handoff_and_enables_frame_capture(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("--mission-render-summary", source)
        self.assertIn(
            "arguments.mission_cinematic_handoff or arguments.mission_render_summary",
            source,
        )
        self.assertIn('steps[4:69] + [\n                # The hangar is visible', source)
        self.assertIn('(\"sleep\", \"2\", \"\")', source)
        self.assertIn('"--ac6_render_capture=true"', source)
        self.assertIn('"--ac6_backend_log_signatures=true"', source)
        self.assertIn('"--ac6_log_frontier_passes=true"', source)
        self.assertIn('"--ac6_log_resolve_info=true"', source)
        self.assertIn('"--ac6_log_swap_texture=true"', source)
        self.assertIn('"--ac6_log_texture_load=true"', source)
        self.assertIn('"--ac6_log_world_submission_owner=true"', source)
        self.assertIn('"--ac6_log_campaign_service_owner=true"', source)
        self.assertIn('"--ac6_log_rt_transfers=true"', source)
        self.assertIn('"--ac6_log_world_submission_owner=true"', source)
        self.assertIn('"--ac6_graphics_diagnostics=false"', source)

    def test_campaign_service_probe_is_read_only_and_state_bounded(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/src/ac6_world_submission_owner_probe.cpp"
        ).read_text()
        self.assertIn("PPC_FUNC_IMPL(rex_sub_821D5F48)", source)
        self.assertIn("PPC_FUNC_IMPL(rex_sub_82124930)", source)
        self.assertIn("state_cecc", source)
        self.assertIn("[ac6-us-campaign-service]", source)
        self.assertNotIn("PPC_STORE", source)

    def test_postprocess_order_probe_is_bounded_and_read_only(self) -> None:
        runner_source = SCRIPT.read_text()
        command_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "command_processor.cpp"
        ).read_text()
        self.assertIn("--mission-postprocess-order", runner_source)
        self.assertIn('"--ac6_log_postprocess_order=true"', runner_source)
        self.assertIn('"--ac6_graphics_diagnostics=false"', runner_source)
        self.assertIn("or arguments.mission_postprocess_order", runner_source)
        self.assertNotIn(
            "--mission-postprocess-order requires --mission-render-summary",
            runner_source,
        )
        self.assertIn("g_ac6_postprocess_gameplay_frames < 30", command_source)
        self.assertIn("!ac6::IsCinematicActive()", command_source)
        self.assertIn("ac6::WorldRenderActiveRecently()", command_source)
        self.assertIn("[ac6-postprocess-draw]", command_source)
        self.assertIn("[ac6-postprocess-resolve]", command_source)
        self.assertIn("[ac6-postprocess-content]", command_source)
        validate_source = (MODULE.PRODUCT / "tools/validate.py").read_text()
        self.assertIn('b"[ac6-postprocess-draw]"', validate_source)

    def test_backend_signature_log_exposes_the_black_world_resolve_boundary(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/src/ac6_backend_fixes/ac6_backend_hooks.cpp"
        ).read_text()
        for marker in (
            "clears={}",
            "rt_unique={}",
            "rt_first=0x{:08X}",
            "resolve_first_lr=0x{:08X}",
            "resolve_last_dest=0x{:08X}",
            "frontbuffer_pa=0x{:08X}",
        ):
            self.assertIn(marker, source)

    def test_compose_resolve_catalog_is_bounded_and_records_guest_ownership(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/src/ac6_backend_fixes/ac6_backend_hooks.cpp"
        ).read_text()
        self.assertIn("g_compose_candidate_frames", source)
        self.assertIn("capture_summary.draw_count >= 500", source)
        self.assertIn("capture_summary.resolve_count >= 40", source)
        self.assertIn("(sample % 60) == 0", source)
        self.assertIn("[ac6-compose-resolve]", source)
        self.assertIn("resolve.guest_lr", source)
        self.assertIn("resolve.owner_lr", source)
        self.assertIn("resolve.args[2]", source)
        self.assertIn("resolve.shadow_state.render_targets[0]", source)

        d3d_source = (
            MODULE.PRODUCT / "upstream/AC6_recomp/src/d3d_hooks.cpp"
        ).read_text()
        self.assertIn("record.guest_lr == 0x8234D5F4", d3d_source)
        self.assertIn("ctx.r1.u32 + 152u", d3d_source)

    def test_resolve_info_catalog_is_bounded_and_read_only(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "render_target_cache.cpp"
        ).read_text()
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_log_resolve_info, false", source)
        self.assertIn("sequence <= 8 || (sequence % 256) == 0", source)
        self.assertIn("[ac6-resolve-dump]", source)
        self.assertIn("dump_rectangles_.size()", source)
        for marker in (
            "resolve_info.copy_dest_base",
            "resolve_info.copy_dest_extent_start",
            "resolve_info.copy_dest_extent_length",
            "resolve_info.color_edram_info.base_tiles",
            "resolve_info.GetCopyEdramTileSpan",
        ):
            self.assertIn(marker, source)

        swap_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "texture_cache.cpp"
        ).read_text()
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_log_swap_texture, false", swap_source)
        self.assertIn("[ac6-swap-texture]", swap_source)
        self.assertIn("status=", swap_source)
        self.assertIn("log_sequence % 256", swap_source)

        load_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/pipeline/texture/"
            "cache.cpp"
        ).read_text()
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_log_texture_load, false", load_source)
        self.assertIn("[ac6-texture-load]", load_source)
        self.assertIn('log_decision("prepared"', load_source)
        self.assertIn('log_decision(committed ? "commit-ok"', load_source)

        command_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "command_processor.cpp"
        ).read_text()
        self.assertIn("[ac6-d5b4-texture]", command_source)
        self.assertIn("kAc6D5b4PixelShader", command_source)
        self.assertIn("rb_color_mask=0x{:08X}", command_source)
        self.assertIn("normalized_color_mask=0x{:08X}", command_source)
        self.assertIn("render_pass=0x{:08X}", command_source)
        self.assertIn("attachments=0x{:X}", command_source)

    def test_resolve_content_probe_is_opt_in_and_final_range_bounded(self) -> None:
        source = SCRIPT.read_text()
        command_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "command_processor.cpp"
        ).read_text()
        self.assertIn("--mission-resolve-content", source)
        self.assertIn('"--readback_resolve=fast"', source)
        self.assertIn('"--ac6_log_resolve_content=true"', source)
        self.assertIn("ac6_log_resolve_content, false", command_source)
        self.assertIn("written_address == 0x1AB60000", command_source)
        self.assertIn("written_address == 0x1B9C0000", command_source)
        self.assertIn("sample_window", command_source)

    def test_ac6_present_does_not_merge_pre_gamma_scene_and_hud(self) -> None:
        texture_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "texture_cache.cpp"
        ).read_text()
        command_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "command_processor.cpp"
        ).read_text()
        self.assertIn("current_base_page == 0x1AB60", texture_source)
        self.assertIn("compose_base_page != current_base_page", texture_source)
        self.assertNotIn("RequestAc6HudTexture", texture_source)
        self.assertNotIn("vec3 composed = clamp", command_source)
        self.assertNotIn("kSwapApplyGammaDescriptorSetAc6HudSource", command_source)
        self.assertNotIn("additive_pipeline_", command_source)

    def test_compute_write_barrier_declares_shader_write_access(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "shared_memory.cpp"
        ).read_text()
        start = source.index("case Usage::kComputeWrite:")
        end = source.index("case Usage::kTransferDestination:", start)
        compute_write = source[start:end]
        self.assertIn("VK_ACCESS_SHADER_WRITE_BIT", compute_write)
        self.assertNotIn("VK_ACCESS_SHADER_READ_BIT", compute_write)

    def test_d5b4_texture_probe_is_attached_to_frontier_record(self) -> None:
        command_source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "command_processor.cpp"
        ).read_text()
        record = command_source.index("Ac6FrontierPass record{")
        probe = command_source.index("[ac6-d5b4-texture]")
        self.assertGreater(probe, record)
        self.assertLess(probe, command_source.index("auto same_pass", record))

    def test_rt_transfer_probe_is_bounded_to_color_base_zero_msaa_changes(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/"
            "render_target_cache.cpp"
        ).read_text()
        self.assertIn("ac6_log_rt_transfers, false", source)
        self.assertIn("[ac6-rt-transfer]", source)
        self.assertIn("dest_rt_key.base_tiles == 0", source)
        self.assertIn("source_rt_key.msaa_samples == dest_rt_key.msaa_samples", source)

    def test_frontier_catalog_is_bounded_and_renderer_owned(self) -> None:
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/command_processor.cpp"
        ).read_text()
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_log_frontier_passes, false", source)
        self.assertIn("draw_count >= 500", source)
        self.assertIn("contains_d5b4", source)
        self.assertIn("pass.pixel_shader == kAc6D5b4PixelShader", source)
        self.assertIn("contains_d5b4 ||", source)
        self.assertIn("(sample % 60) == 0", source)
        self.assertIn("[ac6-frontier-pass]", source)
        self.assertIn("[ac6-frontier-resolve]", source)
        self.assertIn("viewport={},{},{}x{} scissor={},{},{}x{}", source)
        self.assertIn("draw_util::GetScissor(regs, frontier_scissor)", source)

    def test_cinematic_handoff_receipt_rejects_an_unchanged_hangar(self) -> None:
        source = SCRIPT.read_text()
        self.assertIn("mission_handoff_progressed = changed_pixels", source)
        self.assertIn('captures["post-weapon-confirm"]', source)
        self.assertIn('captures["mission-tactical-map"]', source)
        self.assertIn("mission handoff did not leave first hangar", source)

    def test_d5b4_final_output_probe_is_opt_in_and_shader_scoped(self) -> None:
        runner = SCRIPT.read_text()
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/pipeline/shader/spirv_translator_rb.cpp"
        ).read_text()
        self.assertIn("--mission-d5b4-final-white", runner)
        self.assertIn('"--ac6_d5b4_final_white=true"', runner)
        self.assertIn("requires --mission-render-summary", runner)
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_d5b4_final_white, false", source)
        self.assertIn("UINT64_C(0xD5B4F4A878949938)", source)
        self.assertIn("builder_->createStore(const_float4_1_", source)
        self.assertIn("[ac6-d5b4-final-white]", source)

    def test_d5b4_depth_bypass_is_opt_in_and_keeps_final_white_as_baseline(self) -> None:
        runner = SCRIPT.read_text()
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/command_processor.cpp"
        ).read_text()
        self.assertIn("--mission-d5b4-depth-bypass", runner)
        self.assertIn("requires --mission-d5b4-final-white", runner)
        self.assertIn('"--ac6_d5b4_depth_stencil_bypass=true"', runner)
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_d5b4_depth_stencil_bypass, false", source)
        self.assertIn("pixel_shader->ucode_data_hash() == UINT64_C(0xD5B4F4A878949938)", source)
        self.assertIn("normalized_depth_control.stencil_enable = 0", source)
        self.assertIn("normalized_depth_control.z_enable = 0", source)
        self.assertIn("[ac6-d5b4-depth-stencil-bypass]", source)

    def test_d5b4_cull_bypass_is_opt_in_and_keeps_prior_arms_as_baseline(self) -> None:
        runner = SCRIPT.read_text()
        source = (
            MODULE.PRODUCT
            / "upstream/AC6_recomp/thirdparty/rexglue-sdk/src/graphics/vulkan/pipeline_cache.cpp"
        ).read_text()
        self.assertIn("--mission-d5b4-cull-bypass", runner)
        self.assertIn("requires --mission-d5b4-depth-bypass", runner)
        self.assertIn('"--ac6_d5b4_cull_bypass=true"', runner)
        self.assertIn("REXCVAR_DEFINE_BOOL(ac6_d5b4_cull_bypass, false", source)
        self.assertIn("pixel_shader->shader().ucode_data_hash() == UINT64_C(0xD5B4F4A878949938)", source)
        self.assertIn("cull_front = false", source)
        self.assertIn("cull_back = false", source)
        self.assertIn("[ac6-d5b4-cull-bypass]", source)


if __name__ == "__main__":
    unittest.main()
