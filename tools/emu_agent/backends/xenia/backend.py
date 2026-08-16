"""Fail-closed Xenia Edge backend for ``emu-agent/v1``.

The pinned Edge bridge has a deterministic injection seam, but its transport
is not present in this workspace: an action must be submitted to
``AgentBridge::SubmitAction`` before the guest poll.  This backend therefore
validates the complete campaign and records a bounded execution plan, while
refusing to claim that Xenia accepted or executed the timeline.  A future
transport can implement the same ``Backend`` seam without changing the
protocol or exposing shell/memory operations.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Mapping

from ...identity import PROGRAM_SHA256, XENIA_EDGE_APPIMAGE_SHA256, collect_identity
from ...protocol import normalize_json, sha256_json
from ...protocol.errors import ProtocolError, ValidationError
from ...runner.backend import Backend


class XeniaBackendError(ProtocolError):
    """Xenia identity or transport cannot satisfy an episode."""


_CAPABILITIES: dict[str, dict[str, Any]] = {
    "pause_resume": {"status": "unavailable", "evidence": "no transport-safe-point"},
    "frame_present_stepping": {"status": "hook-exists", "evidence": "Edge AgentBridge OnCompletedPresent"},
    "xam_input_poll_step": {"status": "unavailable", "evidence": "no SubmitAction transport"},
    "timeline_preprogrammed": {"status": "planned-only", "evidence": "timeline validator; no guest acceptance"},
    "savestate_exact": {"status": "unavailable", "evidence": "no qualified save/restore seam"},
    "bootstrap_state_driven": {"status": "diagnostic", "evidence": "route documented, state observer absent"},
    "guest_memory_read": {"status": "unavailable", "evidence": "no bounded memory transport"},
    "powerpc_registers": {"status": "unavailable", "evidence": "no register transport"},
    "breakpoints_watchpoints": {"status": "unavailable", "evidence": "rr is separate and not a Xenia transport"},
    "coverage_or_hit_sequence": {"status": "unavailable", "evidence": "no guest coverage transport"},
    "xam_controller_state": {"status": "hook-exists", "evidence": "InputSystem::GetState AgentBridge seam"},
    "cpu_gpu_barrier": {"status": "hook-exists", "evidence": "completed XE_SWAP boundary only"},
    "framebuffer_capture": {"status": "unavailable", "evidence": "frontbuffer receipt is not a pixel capture"},
    "artifact_references": {"status": "available", "evidence": "references and hashes only"},
}


def capability_matrix() -> dict[str, Any]:
    return {
        "schema": "ac6-emu-agent-xenia-capabilities/v1",
        "backend": "xenia",
        "target": "ac6-pal",
        "xenia_is_oracle": True,
        "capabilities": {key: dict(value) for key, value in _CAPABILITIES.items()},
        "input_clock": {"xam_input_poll_step": False, "fallback": "diagnostic-only"},
        "safety": {"allow_guest_write": False, "allow_synthetic_state": False, "allow_shell": False},
        "transport": {"kind": "none", "submit_action": False, "read_observation": False},
        "claims": {"accepted_input": False, "guest_progress": False, "parity": False},
    }


def _workspace_root() -> Path:
    # backend.py -> xenia -> backends -> emu_agent -> tools -> workspace
    return Path(__file__).resolve().parents[4]


class XeniaBackend(Backend):
    """Identity-checking, metadata-only Xenia backend.

    ``execute`` is intentionally rejected.  The fixed Edge launcher is an
    interactive oracle route and cannot be used as an agent transport until a
    local authenticated SubmitAction/observation channel is qualified.
    """

    name = "xenia"

    def __init__(self, *, workspace_root: Path | None = None) -> None:
        self.workspace_root = (workspace_root or _workspace_root()).resolve()
        self._started = False
        self._request: dict[str, Any] = {}
        self._identity: dict[str, Any] = {}
        self._poll = 0
        self._present = 0
        self._events = 0
        self._last_state: dict[str, Any] = {}
        self._errors: list[str] = []

    def start(self, request: Mapping[str, Any]) -> None:
        if request.get("backend") != self.name:
            raise XeniaBackendError(f"backend request is {request.get('backend')!r}, not 'xenia'")
        safety = request.get("safety", {})
        if not isinstance(safety, Mapping):
            raise ValidationError("safety must be an object", path="$.safety")
        if any(safety.get(key) is not False for key in ("allow_guest_write", "allow_synthetic_state", "allow_shell")):
            raise XeniaBackendError("Xenia backend requires all safety mutation flags to be false")
        options = request.get("options", {})
        if not isinstance(options, Mapping):
            raise ValidationError("options must be an object", path="$.options")
        if options.get("execute") is True or options.get("transport") not in (None, "none"):
            raise XeniaBackendError("no qualified local Xenia transport is installed; execution refused")
        config_path = options.get("config_path")
        config = Path(config_path) if isinstance(config_path, str) else None
        profile_path = options.get("profile_root")
        profile = Path(profile_path) if isinstance(profile_path, str) else None
        target = request.get("target", {})
        if not isinstance(target, Mapping):
            raise ValidationError("target must be an object", path="$.target")
        if target.get("program_sha256") not in (None, PROGRAM_SHA256):
            raise XeniaBackendError("request program_sha256 does not match the qualified PAL demo")
        requested_emulator = target.get("emulator_sha256")
        if requested_emulator not in (None, XENIA_EDGE_APPIMAGE_SHA256):
            raise XeniaBackendError("request emulator_sha256 does not match pinned Xenia Edge")
        if target.get("emulator_config_sha256") not in (None,):
            if config is None or not config.is_file():
                raise XeniaBackendError("request requires a configuration hash but no config path is available")
        try:
            self._identity = collect_identity(self.workspace_root, config=config, profile_root=profile)
        except (OSError, ValueError) as error:
            raise XeniaBackendError(str(error)) from error
        requested_config = target.get("emulator_config_sha256")
        actual_config = (self._identity.get("xenia", {}).get("config") or {}).get("sha256")
        if requested_config is not None and requested_config != actual_config:
            raise XeniaBackendError("request emulator_config_sha256 does not match the supplied config")
        self._request = dict(request)
        self._poll = 0
        self._present = 0
        self._events = 0
        self._last_state = {}
        self._errors = []
        self._started = True

    def step(self, event: Mapping[str, Any]) -> Mapping[str, Any]:
        if not self._started:
            raise XeniaBackendError("backend has not been started")
        event_copy = normalize_json(event, path="$.timeline.event")
        if not isinstance(event_copy, Mapping):
            raise ValidationError("event must be an object", path="$.timeline.event")
        payload = event_copy.get("payload", {})
        duration = payload.get("duration", {}) if isinstance(payload, Mapping) else {}
        count = duration.get("count", 1) if isinstance(duration, Mapping) else 1
        if not isinstance(count, int) or count <= 0:
            raise ValidationError("controller duration must be positive", path="$.timeline.event.payload.duration.count")
        at = payload.get("at", {}) if isinstance(payload, Mapping) else {}
        clock = at.get("clock") if isinstance(at, Mapping) else None
        if clock == "xam_input_poll":
            self._poll += count
        elif clock == "present":
            self._present += count
        self._events += 1
        self._last_state = {
            "evidence_class": "needs-dynamic-evidence",
            "guest_executed": False,
            "input_accepted": False,
            "planned_event": dict(event_copy),
            "xam_input_poll": self._poll,
            "present": self._present,
            "raw_device": None,
            "canonical_device": None,
            "first_reader": None,
            "player": None,
            "child": None,
            "transform": None,
        }
        return self._last_state

    def finish(self) -> Mapping[str, Any]:
        if not self._started:
            raise XeniaBackendError("backend has not been started")
        failures = [
            "xenia-submit-action-transport-unavailable",
            "xam-input-poll-step-not-qualified",
            "guest-bootstrap-and-observer-not-qualified",
        ]
        return {
            "backend": self.name,
            "identity": self._identity,
            "capabilities": capability_matrix(),
            "events_planned": self._events,
            "timeline_digest": sha256_json(self._request.get("timeline", [])),
            "_result_metadata": {
                "qualified": False,
                "qualification_failures": failures,
                "stop_reason": "xenia-transport-unavailable",
                "guest_progress": {
                    "events": 0,
                    "start_xam_poll": 0,
                    "end_xam_poll": 0,
                    "start_present": 0,
                    "end_present": 0,
                    "guest_observed": False,
                },
                "observer_liveness": {
                    "positive_control_expected": True,
                    "positive_control_seen": False,
                    "events_before_filter": 0,
                    "events_after_filter": 0,
                },
                "artifacts": [],
                "status": "stopped",
                "error": None,
            },
        }


__all__ = ["XeniaBackend", "XeniaBackendError", "capability_matrix"]
