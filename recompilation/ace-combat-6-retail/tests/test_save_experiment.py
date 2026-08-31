from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools/run_save_experiment.py"
SPEC = importlib.util.spec_from_file_location("ac6_retail_save_experiment", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class Route:
    def parse_steps(self, path: Path) -> list[tuple[str, str, str]]:
        return [
            ("capture", "slot", ""),
            ("wait", "type28=6", "20"),
        ] * 48


def test_create_route_is_bounded_at_slot_confirmation() -> None:
    steps = MODULE.phase_steps(Route(), "create")
    assert len(steps) == MODULE.CREATE_STEPS + 1
    assert steps[-1] == ("sleep", "5", "")


def test_load_route_uses_existing_profile_pacing_and_read_only_markers() -> None:
    steps = MODULE.phase_steps(Route(), "load")
    assert steps[:4] == [
        ("key", "Escape", "0.1"),
        ("sleep", "2", ""),
        ("key", "space", "0.1"),
        ("wait-pulse", "selector44=3", "Escape+space@120"),
    ]
    type6 = steps.index(("wait", "type28=[^ ]*6", "20"))
    assert steps[type6 + 1] == ("capture", "existing-save-type6", "")
    assert steps[type6 + 2 : type6 + 5] == [
        ("sleep", "1", ""),
        ("key", "Left", "0.1"),
        ("sleep", "1", ""),
    ]
    assert ("capture", "existing-save-loaded", "") in steps
    assert ("capture", "save-load-complete", "") in steps


def test_load_only_requires_an_explicit_regular_seed() -> None:
    source = SCRIPT.read_text(encoding="utf-8")
    assert "--load-only" in source
    assert "--storage-seed" in source
    assert "storage seed must be a regular directory" in source
    assert "tree_manifest(arguments.storage_seed)" in source


def test_expected_level_uses_qualified_getter_observation() -> None:
    runtime = "\n".join([
        "[ac6-current-level] profile=0x1 selector=0 value=0 lr=0x2",
        "[ac6-current-level] profile=0x1 selector=1 value=2 lr=0x3",
    ])
    assert MODULE.current_level_observations(runtime) == [
        {"selector": 0, "value": 0},
        {"selector": 1, "value": 2},
    ]


def test_expected_level_is_load_only_and_fail_closed() -> None:
    source = SCRIPT.read_text(encoding="utf-8")
    assert "--expected-level" in source
    assert "--expected-level requires --load-only" in source
    assert 'observation["selector"] == 1' in source
    assert 'observation["value"] == expected_level' in source
    assert "and level_verified" in source


def test_tree_manifest_rejects_symlinked_seed_entries() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / "seed"
        target = Path(temporary) / "outside"
        root.mkdir()
        target.write_bytes(b"not part of the seed")
        (root / "escape").symlink_to(target)
        try:
            MODULE.tree_manifest(root)
        except RuntimeError as error:
            assert "symlink" in str(error)
        else:
            raise AssertionError("symlinked seed entry was accepted")


def test_save_container_summary_checks_magic_version_and_sentinel() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        save = root / "profile" / "sav_acecombat6" / "save.dat"
        save.parent.mkdir(parents=True)
        save.write_bytes(
            b"\0\0\0S\0\0\0A\0\0\0V\0\0\0E"
            b"\0\0\0\6\0\0\0\0\xfe\xfe\xfe\xfe" + b"\0" * 32
        )
        summary = MODULE.save_container_summary(root)
        assert summary["valid"] is True
        assert summary["version"] == 6
        assert summary["sentinel"] == "0xFEFE"


def test_save_container_summary_rejects_wrong_header() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        save = root / "profile" / "sav_acecombat6" / "save.dat"
        save.parent.mkdir(parents=True)
        save.write_bytes(b"not-a-save")
        summary = MODULE.save_container_summary(root)
        assert summary["valid"] is False
        assert summary["reason"] == "invalid SAVE header"


def test_phase_success_requires_clean_process_teardown() -> None:
    source = SCRIPT.read_text(encoding="utf-8")
    assert "and clean_shutdown" in source
    assert "game_status == 0" in source
    assert "xvfb_status == 0" in source
