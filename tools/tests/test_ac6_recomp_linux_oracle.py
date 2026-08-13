from __future__ import annotations

import fcntl
import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from ac6_recomp_linux_oracle import (  # noqa: E402
    DEFAULT_MANIFEST,
    OracleError,
    execute,
    load_manifest,
    sha256,
    terminate,
    tree_sha256,
)


class LinuxOracleRunnerTests(unittest.TestCase):
    def test_tracked_patch_stack_and_manifest_are_sealed(self) -> None:
        document = load_manifest()
        self.assertEqual(document["status"], "built-boot-title-pass")
        for record in document["patch_stack"]:
            self.assertEqual(sha256(ROOT / record["path"]), record["sha256"])

    def test_tree_hash_includes_paths_and_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "a").write_bytes(b"one")
            before = tree_sha256(root)
            (root / "a").rename(root / "b")
            after = tree_sha256(root)
        self.assertEqual(before[:2], after[:2])
        self.assertNotEqual(before[2], after[2])

    def test_existing_output_is_refused_before_launch_and_is_preserved(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "run"
            output.mkdir()
            sentinel = output / "sentinel"
            sentinel.write_text("owned")
            with self.assertRaisesRegex(OracleError, "output must not exist"):
                execute(
                    Path("/missing"), Path("/missing"), Path("/missing"),
                    output, DEFAULT_MANIFEST, 0, 1,
                )
            self.assertEqual(sentinel.read_text(), "owned")

    def test_checkout_lock_rejects_concurrent_runner(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            checkout = Path(directory).resolve()
            lock_name = hashlib.sha256(str(checkout).encode()).hexdigest()[:16]
            lock_path = Path(tempfile.gettempdir()) / f"ac6-linux-oracle-{lock_name}.lock"
            with lock_path.open("w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                result = subprocess.run(
                    [
                        sys.executable, str(TOOLS / "ac6_recomp_linux_oracle.py"),
                        "--checkout", str(checkout), "--build-dir", str(checkout),
                        "--content-dir", str(checkout), "--verify-only",
                    ],
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checkout already active", result.stderr)

    def test_terminate_only_stops_owned_process_group(self) -> None:
        owned = subprocess.Popen(["sleep", "30"], start_new_session=True)
        unrelated = subprocess.Popen(["sleep", "30"], start_new_session=True)
        try:
            terminate(owned)
            self.assertIsNotNone(owned.poll())
            self.assertIsNone(unrelated.poll())
        finally:
            terminate(owned)
            terminate(unrelated)


if __name__ == "__main__":
    unittest.main()
