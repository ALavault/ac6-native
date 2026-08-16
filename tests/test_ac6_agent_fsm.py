import copy
import json
from pathlib import Path

import pytest

import tools.emu_agent.protocol.v2 as protocol
from tools.emu_agent.controller.ac6_fsm import AC6AgentFSM, BUDGET_STATES, STATES, _clamp_int16


class Driver:
    def __init__(self, outcomes=()):
        self.actions, self.outcomes = [], list(outcomes)

    def __call__(self, action):
        self.actions.append(copy.deepcopy(action))
        status = self.outcomes.pop(0) if self.outcomes else "ok"
        return {"status": status, "session_id": action["session_id"]}


def observation(seq, milestones=(), *, terminal="active", session="s", unavailable=(), position=(1000000, 1000000, 0)):
    provenance = {"status": "qualified", "reason": "test", "target_id": protocol.TARGET_ID,
                  "xex_sha256": protocol.XEX_SHA256, "module": "Default.xex",
                  "source_kind": "demo-native", "artifact_sha256": "a" * 64}
    values = {
        "player": {"position": list(position), "health": 100, "state": "alive"},
        "camera": {"position": [0, 0, 0]}, "flight": {"speed": 1, "altitude": 1, "heading": 0},
        "target": {"id": "t", "distance": 1, "locked": True},
        "objective": {"id": "o", "state": "active", "progress": 0},
        "terminal": {"state": terminal, "code": "ok"},
        "readback": {"sha256": "b" * 64, "width": 1, "height": 1, "format": "rgba8"},
    }
    domains = {name: {"availability": "available", "provenance": provenance, "value": value} for name, value in values.items()}
    for name in unavailable:
        domains[name] = {"availability": "unavailable", "provenance": {"status": "unavailable", "reason": "test"}, "value": None}
    return {"schema": protocol.OBSERVATION_SCHEMA, "observation_id": f"o-{seq}", "session_id": session,
            "sequence": seq, "tick": seq, "present": None, "milestones": list(milestones), "domains": domains,
            "availability": "available", "provenance": provenance}


@pytest.fixture(autouse=True)
def qualified(monkeypatch):
    monkeypatch.setattr(protocol, "QUALIFIED_PAL_ARTIFACT_SHA256S", frozenset({"a" * 64}))


def config(**changes):
    item = json.loads((Path(__file__).resolve().parents[1] / "config/ac6-agent-fsm-v1.json").read_text())
    item.update(changes)
    return item


def advance(fsm, milestones):
    return fsm.step(observation(fsm.sequence, milestones))


def to_attack(fsm):
    for milestones in (("title",), (), (), ("frontend",), (), (), ("loading",), ("flight",), ("stable",), ("acquire",), ("locked",)):
        advance(fsm, milestones)
    assert fsm.state == "attack"


def test_all_states_transitions_delays_and_positive_terminal():
    driver = Driver()
    fsm = AC6AgentFSM(driver, config())
    seen = []
    for milestones in (("title",), (), (), ("frontend",), (), (), ("loading",), ("flight",), ("stable",), ("acquire",), ("locked",), ("track",), ("complete",)):
        result = advance(fsm, milestones); seen.append(result.state)
    assert len(STATES) == 10 and set(BUDGET_STATES) == set(config()["state_budgets"])
    assert set(STATES).issubset(set(seen) | {"wait_boot"})
    assert result.state == "terminal" and result.status == "succeeded" and result.outcome == "succeeded"
    assert result.action["xinput"] == {**result.action["xinput"], "buttons": 0, "right_trigger": 0}
    assert [a["sequence"] for a in driver.actions] == list(range(len(driver.actions)))
    assert driver.actions[0]["xinput"]["buttons"] == 0
    assert driver.actions[2]["xinput"]["buttons"] == 256
    assert driver.actions[3]["xinput"]["buttons"] == 0
    assert driver.actions[5]["xinput"]["buttons"] == 4096
    assert driver.actions[10]["xinput"]["buttons"] == 16


def test_terminal_negative_is_checked_before_fire_and_target_loss_releases_neutral():
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    to_attack(fsm)
    result = fsm.step(observation(fsm.sequence, terminal="failed"))
    assert result.status == "failed" and result.reason == "terminal_negative"
    assert driver.actions[-1]["xinput"]["buttons"] == 0
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    to_attack(fsm)
    result = fsm.step(observation(fsm.sequence, unavailable=("target",)))
    assert result.reason == "target_lost" and driver.actions[-1]["xinput"]["buttons"] == 0


@pytest.mark.parametrize("state,domain", [("stabilize_flight", "player"), ("navigate", "objective"), ("acquire", "camera"), ("attack", "flight"), ("track_objective", "target")])
def test_required_domains_are_fail_closed(state, domain):
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    fsm.state = state  # State-specific contract test; the observation remains protocol-qualified.
    result = fsm.step(observation(0, unavailable=(domain,)))
    assert result.status == "failed" and result.reason in {"required_domain_unavailable", "target_lost"}
    assert driver.actions[-1]["xinput"]["buttons"] == 0


@pytest.mark.parametrize("terminal", ["bogus", "retry", "aborted", "divergence"])
def test_terminal_enum_is_strict(terminal):
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    result = fsm.step(observation(0, terminal=terminal))
    assert result.status == "failed" and result.action["xinput"]["buttons"] == 0


def test_terminal_domain_unavailable_releases_neutral():
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    result = fsm.step(observation(0, unavailable=("terminal",)))
    assert result.reason == "terminal_unavailable" and driver.actions[-1]["xinput"]["buttons"] == 0


def test_session_sequence_tick_and_backend_failures_are_not_ignored():
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    advance(fsm, ("title",))
    assert fsm.step(observation(1, session="other")).reason == "session_mismatch"
    driver = Driver(); fsm = AC6AgentFSM(driver, config())
    advance(fsm, ())
    assert fsm.step(observation(0)).reason == "stale_observation"
    # A backend failure after a non-neutral Start frame requires a neutral release.
    cfg = config(); cfg["delays"]["press_start"] = 0
    driver = Driver(["backend_unavailable"]); fsm = AC6AgentFSM(driver, cfg)
    result = advance(fsm, ("title",))
    assert result.reason == "backend_error" and [a["xinput"]["buttons"] for a in driver.actions] == [256, 0]


@pytest.mark.parametrize("active", ["start", "fire", "stick"])
def test_malformed_observation_after_active_frame_releases_with_last_safe_context(active):
    cfg = config(); cfg["delays"]["press_start"] = 0
    driver = Driver(); fsm = AC6AgentFSM(driver, cfg)
    if active == "start":
        advance(fsm, ("title",))
    elif active == "fire":
        to_attack(fsm)
    else:
        for milestones in (("title",), ("frontend",), ("loading",), ("flight",), ("stable",)):
            advance(fsm, milestones)
        assert driver.actions[-1]["xinput"]["right_stick"] != {"x": 0, "y": 0}
    previous = driver.actions[-1]
    result = fsm.step({"malformed": True})
    release = driver.actions[-1]
    assert previous["xinput"] != _neutral_frame() and result.reason == "invalid_observation"
    assert release["session_id"] == previous["session_id"] and release["tick"] == previous["tick"]
    assert release["sequence"] == previous["sequence"] + 1 and release["xinput"] == _neutral_frame()


def _neutral_frame():
    return {"buttons": 0, "left_trigger": 0, "right_trigger": 0,
            "left_stick": {"x": 0, "y": 0}, "right_stick": {"x": 0, "y": 0}, "connected": True}


def test_budgets_max_steps_neutral_final_and_saturation():
    cfg = config(); cfg["max_steps"] = 1
    driver = Driver(); fsm = AC6AgentFSM(driver, cfg)
    result = advance(fsm, ("title",))
    assert result.reason == "timeout_or_budget" and [a["sequence"] for a in driver.actions] == [0]
    cfg = config(); cfg["pid"]["navigate"]["output"] = 32767; cfg["waypoint"] = [-1000000, 1000000, 0]
    driver = Driver(); fsm = AC6AgentFSM(driver, cfg)
    for milestones in (("title",), (), (), ("frontend",), (), (), ("loading",), ("flight",), ("stable",)):
        advance(fsm, milestones)
    sticks = driver.actions[-1]["xinput"]["right_stick"]
    assert sticks == {"x": -32767, "y": 0}
    assert _clamp_int16(-1e9) == -32768 and _clamp_int16(1e9) == 32767
    cfg["pid"]["navigate"]["output"] = 32768
    with pytest.raises(ValueError, match="invalid P gain"):
        AC6AgentFSM(Driver(), cfg)


def test_determinism_hash_and_config_mutation():
    cfg = config()
    runs = []
    for _ in range(2):
        driver = Driver(); fsm = AC6AgentFSM(driver, cfg)
        for milestones in (("title",), (), (), ("frontend",), (), (), ("loading",), ("flight",), ("stable",)):
            advance(fsm, milestones)
        runs.append(driver.actions)
    assert runs[0] == runs[1]
    assert AC6AgentFSM(Driver(), cfg).config_sha256 == protocol.digest(cfg)
    mutated = copy.deepcopy(cfg); mutated["waypoint"][0] = 1
    assert AC6AgentFSM(Driver(), mutated).config_sha256 != protocol.digest(cfg)


@pytest.mark.parametrize("path,value", [
    (("pid", "navigate", "kp"), float("nan")), (("pid", "navigate", "kp"), float("inf")),
    (("max_steps",), 0.5), (("state_budgets", "wait_boot"), float("inf")),
    (("delays", "press_start"), -1), (("waypoint", 0), 1_000_001),
])
def test_every_numeric_config_input_is_bounded_and_finite(path, value):
    cfg = config(); target = cfg
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = value
    with pytest.raises(ValueError):
        AC6AgentFSM(Driver(), cfg)


def test_declared_pid_surface_is_only_consumed_navigate_p_gain():
    cfg = config()
    assert set(cfg["pid"]) == {"navigate"} and set(cfg["pid"]["navigate"]) == {"kp", "output"}
    cfg["pid"]["attack"] = {"kp": 0.1, "output": 1}
    with pytest.raises(ValueError, match="deterministic P"):
        AC6AgentFSM(Driver(), cfg)


def test_terminal_budget_is_not_required_or_accepted():
    cfg = config()
    assert "terminal" not in cfg["state_budgets"] and set(cfg["state_budgets"]) == set(BUDGET_STATES)
    cfg["state_budgets"]["terminal"] = 1
    with pytest.raises(ValueError, match="exactly the active states"):
        AC6AgentFSM(Driver(), cfg)
