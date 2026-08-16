"""Focused ownership and fail-closed tests for the demo-recomp IPC seam."""

from __future__ import annotations

import socket
import struct
import os
import unittest
from unittest.mock import patch

from tools.emu_agent.backends.demo_recomp.transport import (
    DemoRecompTransport,
    DemoRecompTransportError,
    FrameError,
    MAX_TIMEOUT_SECONDS,
    recv_frame,
)
from tools.emu_agent.mcp_server import server as mcp_server
from tools.emu_agent.protocol.v2 import ACTION_SCHEMA, TARGET_ID, XEX_SHA256


class _Process:
    def __init__(self, *_args, **_kwargs):
        self.returncode = None
        self.terminated = False
        self.killed = False

    def poll(self):
        return self.returncode

    def terminate(self):
        self.terminated = True
        self.returncode = -15

    def kill(self):
        self.killed = True
        self.returncode = -9

    def wait(self, timeout=None):
        return self.returncode


class _Transport(DemoRecompTransport):
    instances: list["_Transport"] = []

    def __init__(self, _binary):
        self.closed = False
        self.tick = 0
        type(self).instances.append(self)

    def start(self):
        return None

    def step(self, _xinput):
        self.tick += 1
        return {"ok": True, "tick": self.tick, "present": 0}

    def observe(self):
        return {"ok": True, "tick": self.tick, "present": 0}

    def close(self):
        self.closed = True


def _action(session_id: str, sequence: int = 0) -> dict:
    return {
        "schema": ACTION_SCHEMA, "action_id": f"action-{sequence}",
        "session_id": session_id, "sequence": sequence, "tick": sequence,
        "xinput": {"buttons": 0, "left_trigger": 0, "right_trigger": 0,
                   "left_stick": {"x": 0, "y": 0},
                   "right_stick": {"x": 0, "y": 0}, "connected": True},
    }


class DemoRecompFrameTests(unittest.TestCase):
    def test_real_binary_sequence_when_qualified_store_is_explicitly_gated(self):
        binary = os.environ.get("AC6_DEMO_RECOMP_BINARY")
        xdg_data = os.environ.get("AC6_DEMO_RECOMP_XDG_DATA_HOME")
        if not binary or not xdg_data:
            self.skipTest("real demo IPC requires explicit qualified-store test gate")
        prior = os.environ.get("XDG_DATA_HOME")
        os.environ["XDG_DATA_HOME"] = xdg_data
        try:
            transport = DemoRecompTransport(binary, timeout=MAX_TIMEOUT_SECONDS)
            transport.start()
            frame = {"buttons": 0, "left_trigger": 0, "right_trigger": 0,
                     "left_stick": {"x": 0, "y": 0},
                     "right_stick": {"x": 0, "y": 0}, "connected": True}
            stepped, observed = transport.step(frame), transport.observe()
            self.assertEqual((stepped["tick"], stepped["present"]),
                             (observed["tick"], observed["present"]))
            transport.close()
            self.assertFalse(transport.is_open)
        finally:
            if prior is None:
                os.environ.pop("XDG_DATA_HOME", None)
            else:
                os.environ["XDG_DATA_HOME"] = prior
    def test_truncated_oversized_and_timeout_frames_fail_closed(self):
        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        self.addCleanup(left.close); self.addCleanup(right.close)
        right.sendall(struct.pack(">I", 4) + b"{")
        right.close()
        with self.assertRaises(FrameError):
            recv_frame(left, timeout=0.1)

        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        self.addCleanup(left.close); self.addCleanup(right.close)
        right.sendall(struct.pack(">I", 65537))
        with self.assertRaises(FrameError):
            recv_frame(left, timeout=0.1)
        with self.assertRaises(DemoRecompTransportError):
            recv_frame(left, timeout=0.01)

    def test_duplicate_and_noncanonical_frames_are_rejected(self):
        for payload in (b'{"ok":true,"ok":true}', b'{"token":"a","ok":true}'):
            left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
            self.addCleanup(left.close); self.addCleanup(right.close)
            right.sendall(struct.pack(">I", len(payload)) + payload)
            with self.assertRaises(FrameError):
                recv_frame(left, timeout=0.1)

    def test_invalid_timeouts_never_spawn(self):
        calls = []
        for timeout in (0, -1, float("nan"), float("inf"), MAX_TIMEOUT_SECONDS + 1):
            with patch("tools.emu_agent.backends.demo_recomp.transport.subprocess.Popen", lambda *a, **k: calls.append((a, k))), \
                 patch("tools.emu_agent.backends.demo_recomp.transport.Path.is_file", return_value=True):
                with self.assertRaises(ValueError):
                    DemoRecompTransport("/configured/ac6-demo-recomp", timeout=timeout)
        self.assertEqual(calls, [])

    def test_spawn_is_fixed_shell_free_and_close_is_owned_only(self):
        calls = []

        def spawn(*args, **kwargs):
            calls.append((args, kwargs))
            return _Process()

        external_left, external_right = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        self.addCleanup(external_left.close); self.addCleanup(external_right.close)
        with patch("tools.emu_agent.backends.demo_recomp.transport.subprocess.Popen", spawn), \
             patch("tools.emu_agent.backends.demo_recomp.transport.Path.is_file", return_value=True):
            transport = DemoRecompTransport("/configured/ac6-demo-recomp")
            transport.open()
            process = transport.process
            self.assertEqual(calls[0][0][0][:2], ["/configured/ac6-demo-recomp", "--emu-agent-ipc-fd"])
            self.assertFalse(calls[0][1]["shell"])
            self.assertEqual(len(calls[0][1]["pass_fds"]), 1)
            transport.close(); transport.close()
        self.assertTrue(process.terminated)
        self.assertEqual(external_left.family, socket.AF_UNIX)
        self.assertFalse(transport.is_open)

    def test_crashed_child_is_explicit(self):
        with patch("tools.emu_agent.backends.demo_recomp.transport.subprocess.Popen", _Process), \
             patch("tools.emu_agent.backends.demo_recomp.transport.Path.is_file", return_value=True):
            transport = DemoRecompTransport("/configured/ac6-demo-recomp")
            transport.open()
            transport.process.returncode = 9
            with self.assertRaisesRegex(DemoRecompTransportError, "exited"):
                transport._request("start")
            transport.close()
            self.assertFalse(transport.is_open)

    def test_spawn_error_closes_transport(self):
        with patch("tools.emu_agent.backends.demo_recomp.transport.subprocess.Popen", side_effect=OSError("spawn failed")), \
             patch("tools.emu_agent.backends.demo_recomp.transport.Path.is_file", return_value=True):
            transport = DemoRecompTransport("/configured/ac6-demo-recomp")
            with self.assertRaises(OSError):
                transport.open()
            self.assertFalse(transport.is_open)
            with self.assertRaises(Exception):
                transport.open()


class DemoRecompMcpTests(unittest.TestCase):
    def _sequence(self) -> tuple[int, int | None, str]:
        target = {"target_id": TARGET_ID, "program_sha256": XEX_SHA256, "module": "Default.xex"}
        server = mcp_server.EmuMcpServer(demo_recomp_binary="/configured/ac6-demo-recomp")
        opened = server.call_tool("emu_open_session", {"target": target, "backend": "demo-recomp"})
        session_id = opened["session_id"]
        observation = server.call_tool("emu_step", {"session_id": session_id, "action": _action(session_id)})["observation"]
        server.call_tool("emu_close_session", {"session_id": session_id})
        return observation["tick"], observation["present"], observation["availability"]

    def test_start_step_observe_stop_and_replay_boot_are_unqualified(self):
        _Transport.instances.clear()
        target = {"target_id": TARGET_ID, "program_sha256": XEX_SHA256, "module": "Default.xex"}
        with patch.object(mcp_server, "DemoRecompTransport", _Transport):
            server = mcp_server.EmuMcpServer(demo_recomp_binary="/configured/ac6-demo-recomp")
            opened = server.call_tool("emu_open_session", {"target": target, "backend": "demo-recomp"})
            self.assertEqual(opened["status"], "completed")
            session_id = opened["session_id"]
            stepped = server.call_tool("emu_step", {"session_id": session_id, "action": _action(session_id)})
            self.assertEqual(stepped["observation"]["tick"], 1)
            self.assertEqual(stepped["observation"]["availability"], "unavailable")
            observed = server.call_tool("emu_observe", {"session_id": session_id})
            self.assertEqual(observed["present"], 0)
            receipt = server.call_tool("emu_run_until", {"session_id": session_id, "actions": [], "max_steps": 1})
            replayed = server.call_tool("emu_replay", {"session_id": session_id, "receipt_id": receipt["receipt_id"], "actions": [_action(session_id)]})
            self.assertEqual(replayed["status"], "completed")
            closed = server.call_tool("emu_close_session", {"session_id": session_id})
            self.assertEqual(closed["status"], "closed")
            self.assertTrue(_Transport.instances[0].closed)

    def test_identical_boot_sequences_have_identical_safe_observations(self):
        _Transport.instances.clear()
        with patch.object(mcp_server, "DemoRecompTransport", _Transport):
            self.assertEqual(self._sequence(), self._sequence())

    def test_demo_native_remains_unavailable(self):
        target = {"target_id": TARGET_ID, "program_sha256": XEX_SHA256, "module": "Default.xex"}
        opened = mcp_server.EmuMcpServer().call_tool("emu_open_session", {"target": target, "backend": "demo-native"})
        self.assertEqual(opened["status"], "backend_unavailable")

    def test_failed_transport_rejects_observe_step_and_empty_run_until(self):
        class FailingTransport(_Transport):
            def observe(self):
                raise DemoRecompTransportError("disconnect")

        target = {"target_id": TARGET_ID, "program_sha256": XEX_SHA256, "module": "Default.xex"}
        with patch.object(mcp_server, "DemoRecompTransport", FailingTransport):
            server = mcp_server.EmuMcpServer(demo_recomp_binary="/configured/ac6-demo-recomp")
            session_id = server.call_tool("emu_open_session", {"target": target, "backend": "demo-recomp"})["session_id"]
            with self.assertRaisesRegex(Exception, "disconnect"):
                server.call_tool("emu_observe", {"session_id": session_id})
            for name, arguments in (("emu_observe", {"session_id": session_id}),
                                    ("emu_step", {"session_id": session_id, "action": _action(session_id)}),
                                    ("emu_run_until", {"session_id": session_id, "actions": [], "max_steps": 1})):
                with self.assertRaisesRegex(Exception, "backend_unavailable"):
                    server.call_tool(name, arguments)

    def test_replay_failure_rolls_back_all_new_sessions(self):
        class ReplayFailTransport(_Transport):
            created = 0
            def __init__(self, binary):
                super().__init__(binary); type(self).created += 1
            def step(self, xinput):
                if type(self).created > 1:
                    raise DemoRecompTransportError("replay step failed")
                return super().step(xinput)

        target = {"target_id": TARGET_ID, "program_sha256": XEX_SHA256, "module": "Default.xex"}
        with patch.object(mcp_server, "DemoRecompTransport", ReplayFailTransport):
            server = mcp_server.EmuMcpServer(demo_recomp_binary="/configured/ac6-demo-recomp")
            session_id = server.call_tool("emu_open_session", {"target": target, "backend": "demo-recomp"})["session_id"]
            server.call_tool("emu_step", {"session_id": session_id, "action": _action(session_id)})
            receipt = server.call_tool("emu_run_until", {"session_id": session_id, "actions": [], "max_steps": 1})
            for _ in range(65):
                with self.assertRaisesRegex(Exception, "replay step failed"):
                    server.call_tool("emu_replay", {"session_id": session_id, "receipt_id": receipt["receipt_id"], "actions": [_action(session_id)]})
                self.assertEqual(set(server._v2_sessions), {session_id})


if __name__ == "__main__":
    unittest.main()
