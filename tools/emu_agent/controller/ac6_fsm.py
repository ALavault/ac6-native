"""Fail-closed, bounded AC6 controller policy.

The controller only translates qualified observations into XInput frames.  It
does not model guest state: every frame is sent through ``emu_step`` and a
non-positive backend result ends the episode with a neutral frame.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Mapping

from ..protocol.v2 import ACTION_SCHEMA, OBSERVATION_SCHEMA, digest, validate_action, validate_observation

STATES = ("wait_boot", "press_start", "navigate_frontend", "loading", "stabilize_flight", "navigate", "acquire", "attack", "track_objective", "terminal")
# Terminal is an outcome-only state: it emits only the bounded close frame and
# has no policy loop, so it intentionally has no per-state action budget.
BUDGET_STATES = ("wait_boot", "press_start", "navigate_frontend", "loading", "stabilize_flight", "navigate", "acquire", "attack", "track_objective")
_REQUIRED = {
    "wait_boot": (), "press_start": (), "navigate_frontend": (), "loading": (),
    "stabilize_flight": ("player", "camera", "flight"),
    "navigate": ("player", "camera", "flight", "objective"),
    "acquire": ("camera", "flight", "target"), "attack": ("flight", "target"),
    "track_objective": ("flight", "target", "objective"), "terminal": (),
}
_SUCCESS_RESULTS = frozenset({"ok", "completed", "advanced"})
_TERMINAL_ACTIVE = "active"
_TERMINAL_SUCCESS = "succeeded"
_TERMINAL_FAILURES = frozenset({"failed", "retry", "aborted", "divergence"})


@dataclass(frozen=True)
class FSMResult:
    state: str
    action: dict[str, Any] | None
    status: str
    reason: str | None = None
    outcome: str | None = None


def _neutral() -> dict[str, Any]:
    return {"buttons": 0, "left_trigger": 0, "right_trigger": 0,
            "left_stick": {"x": 0, "y": 0}, "right_stick": {"x": 0, "y": 0}, "connected": True}


def _clamp_int16(value: float) -> int:
    if not math.isfinite(value):
        raise ValueError("stick value must be finite")
    return max(-32768, min(32767, int(round(value))))


class AC6AgentFSM:
    """Deterministic policy with a strict v2 ``emu_step`` acknowledgement.

    ``max_steps`` counts *all* frames, including the neutral release on a
    failure.  Active frames are therefore never emitted in the final slot.
    A neutral terminal frame can use that slot because it is itself the release.
    """

    def __init__(self, emu_step: Callable[[dict[str, Any]], Any], config: Mapping[str, Any] | str | Path):
        self._emu_step = emu_step
        source = json.loads(Path(config).read_text(encoding="utf-8")) if isinstance(config, (str, Path)) else config
        self.config = json.loads(json.dumps(source, sort_keys=True))
        self._validate_config()
        self.config_sha256 = digest(self.config)
        self.state = "wait_boot"
        self.status = "running"
        self.reason: str | None = None
        self.outcome: str | None = None
        self.sequence = 0
        self.tick = -1
        self.state_steps = 0
        self.session_id: str | None = None
        self._last_observation_sequence = -1
        self._last_observation_tick = -1

    def _validate_config(self) -> None:
        cfg = self.config
        if set(cfg) != {"schema", "name", "max_steps", "state_budgets", "delays", "pid", "waypoint", "buttons"} or cfg.get("schema") != "ac6-agent-fsm/v1":
            raise ValueError("unsupported FSM config")
        if not isinstance(cfg["name"], str) or not cfg["name"] or len(cfg["name"]) > 128:
            raise ValueError("name is malformed")
        if isinstance(cfg["max_steps"], bool) or not isinstance(cfg["max_steps"], int) or not 1 <= cfg["max_steps"] <= 0xFFFFFFFF:
            raise ValueError("max_steps must be a positive integer")
        if set(cfg["state_budgets"]) != set(BUDGET_STATES) or any(isinstance(v, bool) or not isinstance(v, int) or not 1 <= v <= 0xFFFFFFFF for v in cfg["state_budgets"].values()):
            raise ValueError("state budgets must cover exactly the active states")
        if set(cfg["delays"]) != {"press_start", "frontend_confirm", "attack_burst"} or any(isinstance(v, bool) or not isinstance(v, int) or not 0 <= v <= 0xFFFFFFFF for v in cfg["delays"].values()):
            raise ValueError("delays are malformed")
        if cfg["delays"]["attack_burst"] < 1:
            raise ValueError("attack_burst must be positive")
        if set(cfg["pid"]) != {"navigate"} or any(set(gain) != {"kp", "output"} for gain in cfg["pid"].values()):
            raise ValueError("only deterministic P gains are supported")
        for gain in cfg["pid"].values():
            if isinstance(gain["kp"], bool) or not isinstance(gain["kp"], (int, float)) or not math.isfinite(gain["kp"]) or isinstance(gain["output"], bool) or not isinstance(gain["output"], int) or not 0 <= gain["output"] <= 32767:
                raise ValueError("invalid P gain")
        if set(cfg["buttons"]) != {"start", "confirm", "fire"} or any(isinstance(v, bool) or not isinstance(v, int) or not 0 <= v <= 0xFFFF for v in cfg["buttons"].values()):
            raise ValueError("buttons are malformed")
        if not isinstance(cfg["waypoint"], list) or len(cfg["waypoint"]) != 3 or any(isinstance(v, bool) or not isinstance(v, int) or not -1_000_000 <= v <= 1_000_000 for v in cfg["waypoint"]):
            raise ValueError("waypoint must be three integers")

    @staticmethod
    def _milestone(obs: Mapping[str, Any], *names: str) -> bool:
        return bool(set(obs.get("milestones", ())).intersection(names))

    def _available(self, obs: Mapping[str, Any]) -> bool:
        return all(obs["domains"][name]["availability"] == "available" and obs["domains"][name]["provenance"].get("status") == "qualified" for name in _REQUIRED[self.state])

    def _observe(self, observation: Mapping[str, Any]) -> tuple[dict[str, Any] | None, str | None]:
        try:
            obs = validate_observation(observation)
        except Exception:
            return None, "invalid_observation"
        if obs["schema"] != OBSERVATION_SCHEMA or obs["availability"] != "available":
            return obs, "observation_unavailable"
        if self.session_id is None:
            self.session_id = obs["session_id"]
        elif obs["session_id"] != self.session_id:
            return obs, "session_mismatch"
        if obs["sequence"] <= self._last_observation_sequence or obs["tick"] <= self._last_observation_tick:
            return obs, "stale_observation"
        if obs["sequence"] != self.sequence or obs["tick"] < self.tick:
            return obs, "observation_action_mismatch"
        self._last_observation_sequence, self._last_observation_tick = obs["sequence"], obs["tick"]
        self.tick = obs["tick"]
        return obs, None

    def _result_ok(self, result: Any) -> bool:
        return isinstance(result, Mapping) and result.get("status") in _SUCCESS_RESULTS and result.get("session_id") == self.session_id and not result.get("divergence", False)

    def _emit(self, xinput: Mapping[str, Any]) -> tuple[dict[str, Any] | None, bool]:
        if self.session_id is None or self.sequence >= int(self.config["max_steps"]):
            return None, False
        action = {"schema": ACTION_SCHEMA, "action_id": f"ac6-fsm-{self.sequence}", "session_id": self.session_id,
                  "sequence": self.sequence, "tick": self.tick, "xinput": dict(xinput)}
        validate_action(action)
        self.sequence += 1
        try:
            result = self._emu_step(action)
        except Exception:
            return action, False
        return action, self._result_ok(result)

    def _fail(self, obs: Mapping[str, Any] | None, reason: str, *, already_neutral: bool = False) -> FSMResult:
        self.state, self.status, self.reason, self.outcome = "terminal", "failed", reason, "failed"
        action = None
        # On malformed input, retain only the last validated session/tick and
        # attempt its neutral release; no data from the malformed envelope is used.
        if (obs is not None or self.session_id is not None) and not already_neutral:
            action, _ = self._emit(_neutral())
        return FSMResult(self.state, action, self.status, reason, self.outcome)

    def _succeed(self) -> FSMResult:
        self.state, self.status, self.reason, self.outcome = "terminal", "succeeded", None, "succeeded"
        action, ok = self._emit(_neutral())
        if not ok:
            self.status, self.reason, self.outcome = "failed", "backend_error", "failed"
            return FSMResult(self.state, action, self.status, self.reason, self.outcome)
        return FSMResult(self.state, action, self.status, outcome=self.outcome)

    def _transition(self, obs: Mapping[str, Any]) -> bool:
        old = self.state
        m = lambda *x: self._milestone(obs, *x)
        if self.state == "wait_boot" and m("boot", "title", "press_start"): self.state = "press_start"
        elif self.state == "press_start" and m("frontend", "menu"): self.state = "navigate_frontend"
        elif self.state == "navigate_frontend" and m("loading"): self.state = "loading"
        elif self.state == "loading" and m("flight", "flight_ready"): self.state = "stabilize_flight"
        elif self.state == "stabilize_flight" and m("stable", "flight_stable"): self.state = "navigate"
        elif self.state == "navigate" and m("acquire", "target_visible"): self.state = "acquire"
        elif self.state == "acquire" and (m("target_acquired", "locked") or obs["domains"]["target"]["value"].get("locked") is True): self.state = "attack"
        elif self.state == "attack" and m("track", "objective", "objective_update"): self.state = "track_objective"
        elif self.state == "track_objective" and m("mission_accomplished", "complete", "terminal_success"): self.state = "terminal"
        changed = self.state != old
        if changed:
            self.state_steps = 0
        return changed

    def _pid_stick(self, obs: Mapping[str, Any]) -> dict[str, int]:
        position = obs["domains"]["player"]["value"].get("position", [0, 0, 0])
        waypoint, gain = self.config["waypoint"], self.config["pid"]["navigate"]
        limit = gain["output"]
        bounded = lambda value: _clamp_int16(max(-limit, min(limit, value)))
        return {"x": bounded((waypoint[0] - position[0]) * float(gain["kp"])), "y": bounded((waypoint[1] - position[1]) * float(gain["kp"]))}

    def _frame(self, obs: Mapping[str, Any]) -> dict[str, Any]:
        frame = _neutral()
        if self.state == "press_start" and self.state_steps >= self.config["delays"]["press_start"]: frame["buttons"] = self.config["buttons"]["start"]
        elif self.state == "navigate_frontend" and self.state_steps >= self.config["delays"]["frontend_confirm"]: frame["buttons"] = self.config["buttons"]["confirm"]
        elif self.state == "navigate": frame["right_stick"] = self._pid_stick(obs)
        elif self.state == "attack" and self.state_steps < self.config["delays"]["attack_burst"]:
            frame["buttons"], frame["right_trigger"] = self.config["buttons"]["fire"], 255
        return frame

    def step(self, observation: Mapping[str, Any]) -> FSMResult:
        """Consume one fresh observation and send exactly one policy frame when safe."""
        if self.status != "running":
            return FSMResult(self.state, None, self.status, self.reason, self.outcome)
        obs, error = self._observe(observation)
        if error:
            return self._fail(obs, error)
        assert obs is not None
        terminal = obs["domains"]["terminal"]
        if terminal["availability"] != "available" or terminal["provenance"].get("status") != "qualified":
            return self._fail(obs, "terminal_unavailable")
        terminal_state = terminal["value"].get("state")
        if terminal_state == _TERMINAL_SUCCESS:
            return self._succeed()
        if terminal_state in _TERMINAL_FAILURES or self._milestone(obs, "mission_failed", "retry", "divergence"):
            return self._fail(obs, "terminal_negative")
        if terminal_state != _TERMINAL_ACTIVE:
            return self._fail(obs, "invalid_terminal_state")
        if self._milestone(obs, "target_lost") or (self.state in {"acquire", "attack", "track_objective"} and obs["domains"]["target"]["availability"] != "available"):
            return self._fail(obs, "target_lost")
        if not self._available(obs):
            return self._fail(obs, "required_domain_unavailable")
        self._transition(obs)
        if self.state == "terminal":
            return self._succeed()
        budget = self.config["state_budgets"][self.state]
        if self.state_steps >= budget or self.sequence >= self.config["max_steps"] - 1:
            return self._fail(obs, "timeout_or_budget")
        frame = self._frame(obs)
        action, ok = self._emit(frame)
        if not ok:
            if frame == _neutral():
                return self._fail(obs, "backend_error", already_neutral=True)
            return self._fail(obs, "backend_error")
        self.state_steps += 1
        return FSMResult(self.state, action, self.status)
