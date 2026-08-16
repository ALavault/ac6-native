"""Owned-only AF_UNIX IPC transport for ``ac6-demo-recomp``.

The command line and socket endpoint are deliberately not protocol inputs.
Only a server-configured executable is spawned, with a private socketpair and
an unguessable in-band token.  This module never promotes a live process to
PAL qualification; that remains evidence-gated by the v2 protocol.
"""

from __future__ import annotations

import json
import math
import os
import secrets
import select
import socket
import struct
import subprocess
import time
from pathlib import Path
from typing import Any, Mapping

from ...protocol import canonical_json

MAX_FRAME_BYTES = 64 * 1024
DEFAULT_TIMEOUT_SECONDS = 30.0
MAX_TIMEOUT_SECONDS = 120.0
_TOKEN_BYTES = 32


class DemoRecompTransportError(RuntimeError):
    """The owned demo-recomp process or its private IPC failed."""


class FrameError(DemoRecompTransportError):
    """A bounded IPC frame was malformed, truncated, or oversized."""


class TransportClosedError(DemoRecompTransportError):
    """A caller attempted an operation after owned resources were closed."""


def encode_frame(value: Mapping[str, Any], *, max_bytes: int = MAX_FRAME_BYTES) -> bytes:
    """Return one canonical length-prefixed JSON frame."""

    if not isinstance(value, Mapping):
        raise FrameError("IPC payload must be an object")
    payload = canonical_json(dict(value)).encode("utf-8")
    if not payload or len(payload) > max_bytes:
        raise FrameError("IPC frame exceeds maximum size")
    return struct.pack(">I", len(payload)) + payload


def _deadline(timeout: float) -> float:
    if (not isinstance(timeout, (int, float)) or isinstance(timeout, bool) or
            not math.isfinite(timeout) or not 0 < timeout <= MAX_TIMEOUT_SECONDS):
        raise ValueError("timeout must be finite and in the bounded positive range")
    return time.monotonic() + float(timeout)


def _read_exact(sock: socket.socket, length: int, deadline: float) -> bytes:
    chunks: list[bytes] = []
    remaining = length
    while remaining:
        wait = deadline - time.monotonic()
        if wait <= 0:
            raise DemoRecompTransportError("IPC timeout")
        readable, _, _ = select.select([sock], [], [], wait)
        if not readable:
            raise DemoRecompTransportError("IPC timeout")
        try:
            data = sock.recv(remaining)
        except OSError as error:
            raise DemoRecompTransportError("IPC receive failed") from error
        if not data:
            raise FrameError("IPC disconnected while reading frame")
        chunks.append(data)
        remaining -= len(data)
    return b"".join(chunks)


def recv_frame(sock: socket.socket, *, timeout: float = DEFAULT_TIMEOUT_SECONDS,
               max_bytes: int = MAX_FRAME_BYTES) -> dict[str, Any]:
    """Read exactly one strict JSON object frame before ``timeout``."""

    deadline = _deadline(timeout)
    length = struct.unpack(">I", _read_exact(sock, 4, deadline))[0]
    if length == 0 or length > max_bytes:
        raise FrameError("IPC frame length is invalid")
    raw = _read_exact(sock, length, deadline)
    try:
        def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
            result: dict[str, Any] = {}
            for key, value in pairs:
                if key in result:
                    raise FrameError("IPC JSON has duplicate object keys")
                result[key] = value
            return result
        decoded = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise FrameError("IPC JSON is invalid") from error
    if not isinstance(decoded, dict):
        raise FrameError("IPC payload must be an object")
    if canonical_json(decoded).encode("utf-8") != raw:
        raise FrameError("IPC JSON is not canonical")
    return decoded


class DemoRecompTransport:
    """A single owned child process and its one private AF_UNIX socket."""

    def __init__(self, binary_path: str | os.PathLike[str], *,
                 timeout: float = DEFAULT_TIMEOUT_SECONDS,
                 max_frame_bytes: int = MAX_FRAME_BYTES) -> None:
        _deadline(timeout)
        if max_frame_bytes <= 0 or max_frame_bytes > MAX_FRAME_BYTES:
            raise ValueError("max_frame_bytes is invalid")
        path = Path(binary_path)
        if not path.is_file():
            raise DemoRecompTransportError("configured demo-recomp binary is unavailable")
        self._binary_path = path
        self._timeout = float(timeout)
        self._max_frame_bytes = max_frame_bytes
        self._socket: socket.socket | None = None
        self._process: subprocess.Popen[bytes] | None = None
        self._token: str | None = None
        self._started = False
        self._closed = False

    @property
    def process(self) -> subprocess.Popen[bytes] | None:
        return self._process

    @property
    def socket(self) -> socket.socket | None:
        return self._socket

    @property
    def is_open(self) -> bool:
        return (not self._closed and self._started and self._socket is not None and
                self._process is not None and self._process.poll() is None)

    def open(self) -> None:
        if self._closed:
            raise TransportClosedError("transport is closed")
        if self._socket is not None:
            return
        parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            # The fd is the only variable input; argv itself is fixed and
            # callers can neither influence it nor supply a socket path.
            argv = [os.fspath(self._binary_path), "--emu-agent-ipc-fd", str(child.fileno())]
            self._process = subprocess.Popen(
                argv, shell=False, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                close_fds=True, pass_fds=(child.fileno(),),
            )
            self._socket = parent
            self._token = secrets.token_hex(_TOKEN_BYTES)
        except Exception:
            parent.close()
            self._closed = True
            raise
        finally:
            child.close()

    def _request(self, op: str, **fields: Any) -> dict[str, Any]:
        if self._closed or self._socket is None or self._token is None:
            raise TransportClosedError("transport is not open")
        if self._process is None or self._process.poll() is not None:
            self.close()
            raise DemoRecompTransportError("demo-recomp child exited")
        message = {"op": op, "token": self._token, **fields}
        try:
            self._socket.sendall(encode_frame(message, max_bytes=self._max_frame_bytes))
            response = recv_frame(self._socket, timeout=self._timeout,
                                  max_bytes=self._max_frame_bytes)
        except (OSError, DemoRecompTransportError) as error:
            self.close()
            raise DemoRecompTransportError(str(error)) from error
        if set(response) - {"ok", "error", "token", "tick", "present"}:
            self.close()
            raise FrameError("IPC response has unsupported fields")
        if response.get("token") != self._token or not isinstance(response.get("ok"), bool):
            self.close()
            raise FrameError("IPC response token or status is invalid")
        if not response["ok"]:
            error = response.get("error")
            self.close()
            raise DemoRecompTransportError(error if isinstance(error, str) and error else "demo-recomp rejected IPC request")
        if op in {"step", "observe"}:
            if set(response) != {"ok", "token", "tick", "present"} or isinstance(response["tick"], bool) or not isinstance(response["tick"], int) or response["tick"] < 0 or (response["present"] is not None and (isinstance(response["present"], bool) or not isinstance(response["present"], int) or response["present"] < 0)):
                self.close()
                raise FrameError("IPC observation response is invalid")
        return response

    def start(self) -> None:
        self.open()
        self._request("start")
        self._started = True

    def step(self, xinput: Mapping[str, Any]) -> dict[str, Any]:
        if not self._started:
            raise DemoRecompTransportError("demo-recomp session is not started")
        if not isinstance(xinput, Mapping):
            raise DemoRecompTransportError("xinput must be an object")
        return self._request("step", xinput=dict(xinput))

    def observe(self) -> dict[str, Any]:
        if not self._started:
            raise DemoRecompTransportError("demo-recomp session is not started")
        return self._request("observe")

    def _neutralize(self) -> None:
        if self._started and self._socket is not None and self._token is not None:
            try:
                self._socket.sendall(encode_frame({"op": "stop", "token": self._token}, max_bytes=self._max_frame_bytes))
            except (OSError, DemoRecompTransportError):
                pass
        self._started = False

    def close(self) -> None:
        if self._closed:
            return
        self._neutralize()
        sock, process = self._socket, self._process
        self._socket = None
        self._process = None
        self._token = None
        self._closed = True
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=self._timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=self._timeout)

    def __enter__(self) -> "DemoRecompTransport":
        self.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
