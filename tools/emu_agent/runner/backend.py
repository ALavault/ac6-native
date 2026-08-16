"""Backend interface and the deterministic in-process emu-agent backend."""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Mapping

from tools.emu_agent.protocol import normalize_json, sha256_json
from tools.emu_agent.protocol.errors import ProtocolError, ValidationError


class Backend(ABC):
    """Small backend seam consumed by :class:`LocalRunner`.

    Implementations must not read wall-clock time, process identity, global
    randomness, or mutable host state while handling a request.  ``start``
    receives the already validated canonical request, ``step`` receives one
    canonical timeline event, and ``finish`` returns a JSON state snapshot.
    """

    name: str

    @abstractmethod
    def start(self, request: Mapping[str, Any]) -> None:
        """Initialise backend state for one request."""

    @abstractmethod
    def step(self, event: Mapping[str, Any]) -> Mapping[str, Any]:
        """Apply one event and return a JSON state snapshot."""

    @abstractmethod
    def finish(self) -> Mapping[str, Any]:
        """Return the final JSON state snapshot."""


EmulatorBackend = Backend


class SimulatedBackend(Backend):
    """Minimal deterministic backend used when no emulator is configured.

    It intentionally models only event ordering and state identity.  It does
    not claim game behavior or renderer parity; its purpose is to exercise the
    protocol and replay plumbing independently of retail files and a guest.
    """

    name = "simulated"

    def __init__(self) -> None:
        self._seed = 0
        self._event_count = 0
        self._last_frame = 0
        self._digest = ""
        self._started = False

    def start(self, request: Mapping[str, Any]) -> None:
        if request.get("backend") != self.name:
            raise ProtocolError(
                f"backend request is {request.get('backend')!r}, not {self.name!r}"
            )
        seed = request.get("seed")
        if isinstance(seed, bool) or not isinstance(seed, int) or seed < 0:
            raise ValidationError("backend seed is invalid", path="$.seed")
        self._seed = seed
        self._event_count = 0
        self._last_frame = 0
        self._digest = sha256_json({"seed": seed, "inputs": request.get("inputs", {})})
        self._started = True

    def step(self, event: Mapping[str, Any]) -> Mapping[str, Any]:
        if not self._started:
            raise ProtocolError("backend has not been started")
        event_copy = normalize_json(event, path="$.timeline.event")
        if not isinstance(event_copy, Mapping):
            raise ValidationError("backend event must be an object", path="$.timeline.event")
        frame = event_copy.get("frame")
        if isinstance(frame, bool) or not isinstance(frame, int) or frame < self._last_frame:
            raise ValidationError("backend event frame is out of order", path="$.timeline.event.frame")
        self._last_frame = frame
        self._event_count += 1
        self._digest = sha256_json({"previous": self._digest, "event": event_copy})
        return self.snapshot()

    def snapshot(self) -> dict[str, Any]:
        return {
            "backend": self.name,
            "seed": self._seed,
            "frame": self._last_frame,
            "event_count": self._event_count,
            "digest": self._digest,
            # The simulator exposes the shape consumed by the real observer
            # contract without inventing guest facts.  Empty/null lanes are
            # explicit simulated values, never inferred EE/VIF/VU/GIF/GS
            # observations.
            "observation": {
                "evidence_class": "simulated",
                "guest_clock": {
                    "frame": self._last_frame,
                    "vblank": None,
                    "input_poll": None,
                },
                "ee": {"pc": None, "pc_type": None, "jit_block_pc": None, "caller": None, "return": None},
                "vif1": {"events": [], "positive_control": False},
                "vu1": {"program": None, "entry": None, "pc": None, "memory_sha256": None},
                "gif": {"xgkick": [], "transaction": None},
                "gs": {"context": None, "changed_vram_pages_sha256": [], "framebuffer_sha256": None, "present_serial": None},
                "presentation": {"completed": False},
            },
        }

    def finish(self) -> Mapping[str, Any]:
        if not self._started:
            raise ProtocolError("backend has not been started")
        return self.snapshot()


DeterministicBackend = SimulatedBackend
