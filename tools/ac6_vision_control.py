#!/usr/bin/env python3
"""Deterministic vision/control policy engine for the AC6 Xenia oracle.

The process is deliberately emulator-agnostic. A bridge pauses Xenia at a
completed guest frame, publishes one observation as JSONL, reads one action,
applies it for the requested number of guest frames, and repeats. This module
never substitutes wall-clock sleeps for guest-frame stepping.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import random
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Iterable, TextIO


OBSERVATION_SCHEMA = "ac6-agent-observation/v1"
ACTION_SCHEMA = "ac6-agent-action/v1"
ARCHIVE_SCHEMA = "ac6-agent-archive/v1"
TRACE_SCHEMA = "ac6-agent-discovery-trace/v1"
BEHAVIOR_SCHEMA = "ac6-agent-behavior/v1"
MOVIE_SCHEMA = "ac6-agent-controller-movie/v1"
VISION_RULES_SCHEMA = "ac6-agent-vision-rules/v1"
BRIDGE_CAPABILITIES_SCHEMA = "ac6-xenia-agent-bridge-capabilities/v1"
TARGET_XEX_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
SITUATIONS = {
    "boot",
    "splash",
    "title",
    "attract_movie",
    "menu",
    "loading",
    "gameplay",
    "pause",
    "mission_result",
    "retry",
    "unknown",
}
BUTTONS = {
    "A",
    "B",
    "X",
    "Y",
    "START",
    "BACK",
    "DPAD_UP",
    "DPAD_DOWN",
    "DPAD_LEFT",
    "DPAD_RIGHT",
    "LB",
    "RB",
    "LS",
    "RS",
}
MAX_HOLD_FRAMES = 240


class ContractError(RuntimeError):
    pass


def validate_bridge_capabilities(path: Path) -> dict[str, Any]:
    doc = json.loads(path.read_text())
    if doc.get("schema") != BRIDGE_CAPABILITIES_SCHEMA:
        raise ContractError("unsupported bridge-capabilities schema")
    required = {
        "pause_after_completed_present": True,
        "exact_guest_frame_step": True,
        "guest_controller_injection": True,
        "framebuffer_capture_while_paused": True,
        "checkpoint_restore": True,
    }
    if doc.get("capabilities") != required:
        raise ContractError("bridge lacks required deterministic capabilities")
    if doc.get("frame_boundary") != "completed_xe_swap":
        raise ContractError("bridge frame boundary is not qualified")
    if doc.get("controller_boundary") != "guest_xam_poll":
        raise ContractError("bridge controller boundary is not qualified")
    implementation = doc.get("implementation", {})
    if not implementation.get("name") or not is_hex(implementation.get("commit"), 40):
        raise ContractError("bridge implementation identity is incomplete")
    return doc


def canonical(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def is_hex(value: object, length: int) -> bool:
    return (
        isinstance(value, str) and len(value) == length and all(character in "0123456789abcdef" for character in value)
    )


def require_keys(value: dict[str, Any], required: set[str], context: str) -> None:
    missing = sorted(required - value.keys())
    if missing:
        raise ContractError(f"{context} missing keys: {', '.join(missing)}")


@dataclass(frozen=True)
class Controller:
    buttons: tuple[str, ...] = ()
    lx: int = 0
    ly: int = 0
    rx: int = 0
    ry: int = 0
    lt: int = 0
    rt: int = 0
    connected: bool = True

    def validate(self) -> None:
        if not set(self.buttons) <= BUTTONS:
            raise ContractError("unknown controller button")
        if len(set(self.buttons)) != len(self.buttons):
            raise ContractError("duplicate controller button")
        for name in ("lx", "ly", "rx", "ry"):
            if not -32768 <= getattr(self, name) <= 32767:
                raise ContractError(f"{name} outside signed 16-bit range")
        for name in ("lt", "rt"):
            if not 0 <= getattr(self, name) <= 255:
                raise ContractError(f"{name} outside byte range")

    def document(self) -> dict[str, Any]:
        self.validate()
        result = asdict(self)
        result["buttons"] = list(self.buttons)
        return result


@dataclass(frozen=True)
class Option:
    name: str
    controller: Controller
    hold_frames: int
    situations: frozenset[str] = frozenset(SITUATIONS)
    risk: float = 0.0

    def validate(self) -> None:
        self.controller.validate()
        if not 1 <= self.hold_frames <= MAX_HOLD_FRAMES:
            raise ContractError(f"invalid hold_frames for {self.name}")
        if not self.situations <= SITUATIONS:
            raise ContractError(f"invalid situations for {self.name}")
        if not 0.0 <= self.risk <= 1.0:
            raise ContractError(f"invalid risk for {self.name}")


def default_options() -> dict[str, Option]:
    menu = frozenset({"title", "menu", "pause", "retry", "mission_result", "unknown"})
    flight = frozenset({"gameplay", "unknown"})
    rows = (
        Option("neutral_1", Controller(), 1),
        Option("wait_15", Controller(), 15),
        Option("wait_30", Controller(), 30),
        Option("press_start", Controller(("START",)), 1, menu, 0.1),
        Option("confirm", Controller(("A",)), 1, menu, 0.1),
        Option("cancel", Controller(("B",)), 1, menu, 0.1),
        Option("menu_up", Controller(("DPAD_UP",)), 1, menu, 0.1),
        Option("menu_down", Controller(("DPAD_DOWN",)), 1, menu, 0.1),
        Option("menu_left", Controller(("DPAD_LEFT",)), 1, menu, 0.1),
        Option("menu_right", Controller(("DPAD_RIGHT",)), 1, menu, 0.1),
        Option("roll_left", Controller(lx=-24576), 3, flight, 0.25),
        Option("roll_right", Controller(lx=24576), 3, flight, 0.25),
        Option("pitch_up", Controller(ly=24576), 3, flight, 0.25),
        Option("pitch_down", Controller(ly=-24576), 3, flight, 0.25),
        Option("accelerate", Controller(rt=255), 4, flight, 0.2),
        Option("brake", Controller(lt=255), 4, flight, 0.3),
        Option("fire_gun", Controller(("A",)), 2, flight, 0.35),
        Option("fire_missile", Controller(("B",)), 1, flight, 0.4),
        Option("cycle_target", Controller(("Y",)), 1, flight, 0.2),
        Option("disconnect_1", Controller(connected=False), 1, frozenset({"menu", "gameplay", "unknown"}), 0.8),
    )
    for row in rows:
        row.validate()
    return {row.name: row for row in rows}


@dataclass
class Behavior:
    name: str = "balanced"
    prompt: str = ""
    mode: str = "balanced"
    novelty_weight: float = 1.0
    exploration_weight: float = 0.8
    risk_weight: float = 0.5
    max_decisions: int = 1000
    max_guest_frames: int = 36000
    restore_interval: int = 0
    allowed_options: list[str] = field(default_factory=list)
    blocked_options: list[str] = field(default_factory=list)
    forced_options: list[str] = field(default_factory=list)
    preferred_situations: list[str] = field(default_factory=list)

    def validate(self, options: dict[str, Option]) -> None:
        if self.mode not in {"exploit", "balanced", "explore", "forced"}:
            raise ContractError("invalid behavior mode")
        if not 1 <= self.max_decisions <= 1_000_000:
            raise ContractError("invalid decision budget")
        if not 1 <= self.max_guest_frames <= 100_000_000:
            raise ContractError("invalid guest-frame budget")
        if not 0 <= self.restore_interval <= self.max_decisions:
            raise ContractError("invalid checkpoint restore interval")
        for name in ("novelty_weight", "exploration_weight", "risk_weight"):
            value = getattr(self, name)
            if not math.isfinite(value) or value < 0.0:
                raise ContractError(f"invalid behavior weight: {name}")
        named = set(self.allowed_options + self.blocked_options + self.forced_options)
        if not named <= options.keys():
            raise ContractError("behavior references unknown option")
        if not set(self.preferred_situations) <= SITUATIONS:
            raise ContractError("behavior references unknown situation")
        if set(self.forced_options) & set(self.blocked_options):
            raise ContractError("an option cannot be forced and blocked")
        if self.allowed_options and not set(self.forced_options) <= set(self.allowed_options):
            raise ContractError("forced option is outside the allowed set")

    def document(self) -> dict[str, Any]:
        result = asdict(self)
        result["schema"] = BEHAVIOR_SCHEMA
        return result


def compile_prompt(prompt: str, options: dict[str, Option]) -> Behavior:
    """Compile a bounded natural-language intent into explicit policy knobs."""
    text = prompt.casefold()
    behavior = Behavior(name="prompted", prompt=prompt)
    if any(word in text for word in ("explore", "nouveau", "inédit", "tordu", "spécial")):
        behavior.mode = "explore"
        behavior.novelty_weight = 2.0
        behavior.exploration_weight = 1.5
        behavior.restore_interval = 25
    if any(word in text for word in ("prudent", "sûr", "safe", "sans risque")):
        behavior.risk_weight = 2.5
        behavior.blocked_options.append("disconnect_1")
    if any(word in text for word in ("menu", "sous-menu")):
        behavior.preferred_situations.extend(["title", "menu"])
        behavior.allowed_options = [
            "neutral_1",
            "wait_15",
            "press_start",
            "confirm",
            "cancel",
            "menu_up",
            "menu_down",
            "menu_left",
            "menu_right",
        ]
    if any(word in text for word in ("perdre", "échec", "fail volontaire")):
        behavior.mode = "forced"
        behavior.preferred_situations.append("retry")
        behavior.forced_options = ["brake", "pitch_down", "neutral_1"]
    no_fire = any(word in text for word in ("sans tirer", "ne pas tirer", "no fire"))
    if no_fire:
        behavior.blocked_options.extend(["fire_gun", "fire_missile"])
    if not no_fire and any(word in text for word in ("tirer", "agressif", "combat")):
        behavior.forced_options.extend(["cycle_target", "fire_missile", "fire_gun"])
    if any(word in text for word in ("déconnexion", "reconnexion", "disconnect")):
        behavior.mode = "forced"
        behavior.forced_options = ["disconnect_1", "neutral_1"]
    behavior.allowed_options = list(dict.fromkeys(behavior.allowed_options))
    behavior.blocked_options = list(dict.fromkeys(behavior.blocked_options))
    behavior.forced_options = list(dict.fromkeys(behavior.forced_options))
    behavior.preferred_situations = list(dict.fromkeys(behavior.preferred_situations))
    behavior.validate(options)
    return behavior


@dataclass(frozen=True)
class Observation:
    run_id: str
    guest_frame: int
    guest_tick: int
    image_path: str
    image_sha256: str
    visual_signature: str
    state_signature: str
    situation: str
    confidence: float
    checkpoint: str | None
    ocr_text: str
    terminal: bool
    raw: dict[str, Any]

    @classmethod
    def parse(cls, row: dict[str, Any]) -> "Observation":
        require_keys(
            row, {"schema", "identity", "run_id", "guest_frame", "guest_tick", "image", "state"}, "observation"
        )
        if row["schema"] != OBSERVATION_SCHEMA:
            raise ContractError("unsupported observation schema")
        identity = row["identity"]
        if identity.get("xex_sha256") != TARGET_XEX_SHA256:
            raise ContractError("observation XEX identity mismatch")
        image = row["image"]
        state = row["state"]
        situation = state.get("situation", "unknown")
        if situation not in SITUATIONS:
            raise ContractError("unknown situation")
        confidence = float(state.get("confidence", 0.0))
        if not 0.0 <= confidence <= 1.0:
            raise ContractError("invalid situation confidence")
        image_sha = image.get("sha256", "")
        if not is_hex(image_sha, 64):
            raise ContractError("invalid image SHA-256")
        if not str(row["run_id"]):
            raise ContractError("empty run ID")
        if int(row["guest_frame"]) < 0 or int(row["guest_tick"]) < 0:
            raise ContractError("negative guest clock")
        visual = state.get("visual_signature") or image_sha
        semantic_state = {
            "situation": situation,
            "ocr": state.get("ocr_text", ""),
            "guest_pc": state.get("guest_pc"),
            "mission_phase": state.get("mission_phase"),
            "hud": state.get("hud"),
        }
        return cls(
            run_id=str(row["run_id"]),
            guest_frame=int(row["guest_frame"]),
            guest_tick=int(row["guest_tick"]),
            image_path=str(image.get("path", "")),
            image_sha256=image_sha,
            visual_signature=str(visual),
            state_signature=state.get("signature") or digest(semantic_state),
            situation=situation,
            confidence=confidence,
            checkpoint=row.get("checkpoint"),
            ocr_text=str(state.get("ocr_text", "")),
            terminal=bool(state.get("terminal", False)),
            raw=row,
        )

    @property
    def cell_id(self) -> str:
        return digest({"visual": self.visual_signature, "state": self.state_signature})


class VisionClassifier:
    """Strict exact-signature and OCR-rule situation classifier."""

    def __init__(self, rules: list[dict[str, Any]]) -> None:
        self.rules = rules
        for rule in rules:
            if rule.get("situation") not in SITUATIONS - {"unknown"}:
                raise ContractError("vision rule has invalid situation")
            if not any(key in rule for key in ("image_sha256", "visual_signature", "ocr_contains")):
                raise ContractError("vision rule has no evidence selector")
            confidence = float(rule.get("confidence", 1.0))
            if not 0.0 <= confidence <= 1.0:
                raise ContractError("vision rule has invalid confidence")

    @classmethod
    def load(cls, path: Path | None) -> "VisionClassifier":
        if path is None:
            return cls([])
        doc = json.loads(path.read_text())
        if doc.get("schema") != VISION_RULES_SCHEMA:
            raise ContractError("unsupported vision-rules schema")
        return cls(list(doc.get("rules", [])))

    def apply(self, row: dict[str, Any]) -> dict[str, Any]:
        state = row.setdefault("state", {})
        if state.get("situation") in SITUATIONS - {"unknown"} and float(state.get("confidence", 0.0)) >= 0.5:
            return row
        image = row.get("image", {})
        ocr = str(state.get("ocr_text", "")).casefold()
        matches: list[tuple[float, str]] = []
        for rule in self.rules:
            selected = True
            if "image_sha256" in rule:
                selected &= image.get("sha256") == rule["image_sha256"]
            if "visual_signature" in rule:
                selected &= state.get("visual_signature") == rule["visual_signature"]
            if "ocr_contains" in rule:
                tokens = rule["ocr_contains"]
                if isinstance(tokens, str):
                    tokens = [tokens]
                selected &= all(str(token).casefold() in ocr for token in tokens)
            if selected:
                matches.append((float(rule.get("confidence", 1.0)), rule["situation"]))
        if matches:
            confidence, situation = max(matches, key=lambda value: (value[0], value[1]))
            state["situation"] = situation
            state["confidence"] = confidence
            state["classifier"] = "exact-rules-v1"
        else:
            state["situation"] = "unknown"
            state["confidence"] = 0.0
            state["classifier"] = "no-match"
        return row


@dataclass
class ActionStats:
    visits: int = 0
    total_reward: float = 0.0
    outcomes: set[str] = field(default_factory=set)

    @property
    def mean(self) -> float:
        return self.total_reward / self.visits if self.visits else 0.0


@dataclass
class Cell:
    cell_id: str
    situation: str
    visual_signature: str
    state_signature: str
    checkpoint: str | None
    first_frame: int
    visits: int = 0
    best_path: list[str] = field(default_factory=list)
    actions: dict[str, ActionStats] = field(default_factory=dict)


class Archive:
    def __init__(self) -> None:
        self.cells: dict[str, Cell] = {}

    def observe(self, observation: Observation, path: list[str]) -> tuple[Cell, bool]:
        created = observation.cell_id not in self.cells
        if created:
            self.cells[observation.cell_id] = Cell(
                cell_id=observation.cell_id,
                situation=observation.situation,
                visual_signature=observation.visual_signature,
                state_signature=observation.state_signature,
                checkpoint=observation.checkpoint,
                first_frame=observation.guest_frame,
                best_path=list(path),
            )
        cell = self.cells[observation.cell_id]
        cell.visits += 1
        if observation.checkpoint and not cell.checkpoint:
            cell.checkpoint = observation.checkpoint
        if not cell.best_path or len(path) < len(cell.best_path):
            cell.best_path = list(path)
        return cell, created

    def update(self, cell_id: str, option: str, reward: float, outcome: str) -> None:
        stats = self.cells[cell_id].actions.setdefault(option, ActionStats())
        stats.visits += 1
        stats.total_reward += reward
        stats.outcomes.add(outcome)

    def document(self) -> dict[str, Any]:
        cells = []
        for cell in sorted(self.cells.values(), key=lambda value: value.cell_id):
            row = asdict(cell)
            row["actions"] = {
                name: {"visits": stats.visits, "total_reward": stats.total_reward, "outcomes": sorted(stats.outcomes)}
                for name, stats in sorted(cell.actions.items())
            }
            cells.append(row)
        return {"schema": ARCHIVE_SCHEMA, "identity": {"xex_sha256": TARGET_XEX_SHA256}, "cells": cells}

    @classmethod
    def load(cls, path: Path) -> "Archive":
        archive = cls()
        if not path.exists():
            return archive
        doc = json.loads(path.read_text())
        if doc.get("schema") != ARCHIVE_SCHEMA:
            raise ContractError("unsupported archive schema")
        if doc.get("identity", {}).get("xex_sha256") != TARGET_XEX_SHA256:
            raise ContractError("archive XEX identity mismatch")
        for row in doc.get("cells", []):
            actions = {
                name: ActionStats(int(stats["visits"]), float(stats["total_reward"]), set(stats.get("outcomes", [])))
                for name, stats in row.pop("actions", {}).items()
            }
            cell = Cell(**row, actions=actions)
            archive.cells[cell.cell_id] = cell
        return archive


class Policy:
    def __init__(self, behavior: Behavior, options: dict[str, Option], seed: int) -> None:
        behavior.validate(options)
        self.behavior = behavior
        self.options = options
        self.random = random.Random(seed)
        self.forced_index = 0

    def candidates(self, situation: str) -> list[Option]:
        allowed = set(self.behavior.allowed_options or self.options)
        allowed -= set(self.behavior.blocked_options)
        result = [option for name, option in self.options.items() if name in allowed and situation in option.situations]
        if not result:
            result = [self.options["neutral_1"]]
        return sorted(result, key=lambda value: value.name)

    def choose(self, cell: Cell, archive: Archive) -> tuple[Option, dict[str, float]]:
        forced = self.behavior.forced_options
        if forced:
            name = forced[self.forced_index % len(forced)]
            self.forced_index += 1
            option = self.options[name]
            if cell.situation in option.situations:
                return option, {"forced": 1.0}
        candidates = self.candidates(cell.situation)
        total = 1 + sum(cell.actions.get(row.name, ActionStats()).visits for row in candidates)
        scored: list[tuple[float, str, Option, dict[str, float]]] = []
        for option in candidates:
            stats = cell.actions.get(option.name, ActionStats())
            exploit = stats.mean
            explore = math.sqrt(math.log(total + 1) / (stats.visits + 1))
            novelty = 1.0 / (1.0 + len(stats.outcomes))
            if self.behavior.mode == "exploit":
                explore *= 0.1
                novelty *= 0.1
            elif self.behavior.mode == "explore":
                explore *= 1.5
                novelty *= 2.0
            jitter = self.random.random() * 1e-9
            score = (
                exploit
                + self.behavior.exploration_weight * explore
                + self.behavior.novelty_weight * novelty
                - self.behavior.risk_weight * option.risk
                + jitter
            )
            parts = {"exploit": exploit, "explore": explore, "novelty": novelty, "risk": option.risk, "total": score}
            scored.append((score, option.name, option, parts))
        _, _, option, parts = max(scored, key=lambda row: (row[0], row[1]))
        return option, parts


class TraceWriter:
    def __init__(self, stream: TextIO) -> None:
        self.stream = stream
        self.sequence = 0
        self.chain = "0" * 64

    def append(self, kind: str, payload: dict[str, Any]) -> dict[str, Any]:
        body = {
            "schema": TRACE_SCHEMA,
            "sequence": self.sequence,
            "kind": kind,
            "previous_sha256": self.chain,
            "payload": payload,
        }
        self.chain = digest(body)
        body["record_sha256"] = self.chain
        self.stream.write(canonical(body).decode())
        self.stream.flush()
        self.sequence += 1
        return body


class Engine:
    def __init__(
        self, behavior: Behavior, archive: Archive, seed: int, trace: TraceWriter, strict_receipts: bool = False
    ) -> None:
        self.options = default_options()
        self.behavior = behavior
        self.policy = Policy(behavior, self.options, seed)
        self.archive = archive
        self.trace = trace
        self.path: list[str] = []
        self.pending: dict[str, Any] | None = None
        self.decisions = 0
        self.frames = 0
        self.strict_receipts = strict_receipts

    def validate_receipt(self, observation: Observation) -> None:
        if self.pending is None:
            return
        receipt = observation.raw.get("previous_action")
        if receipt is None:
            if self.strict_receipts:
                raise ContractError("observation lacks previous-action receipt")
            return
        if receipt.get("action_id") != self.pending["action_id"]:
            raise ContractError("previous-action receipt ID mismatch")
        advanced = int(receipt.get("advanced_frames", -1))
        if self.pending["operation"] == "restore":
            if advanced != 0 or receipt.get("checkpoint") != self.pending["checkpoint"]:
                raise ContractError("checkpoint restore receipt mismatch")
        else:
            hold = self.pending["hold_frames"]
            if not 1 <= advanced <= hold:
                raise ContractError("guest-frame advance receipt outside action bounds")
            if advanced != hold and not self.pending["stop_on_visual_change"]:
                raise ContractError("short guest-frame advance without visual stop")

    def restore_action(self, observation: Observation, current: Cell) -> dict[str, Any] | None:
        interval = self.behavior.restore_interval
        if not interval or not self.decisions or self.decisions % interval:
            return None
        candidates = [
            cell for cell in self.archive.cells.values() if cell.checkpoint and cell.cell_id != current.cell_id
        ]
        if not candidates:
            return None
        target = min(candidates, key=lambda cell: (cell.visits, len(cell.best_path), cell.cell_id))
        action = {
            "schema": ACTION_SCHEMA,
            "identity": {"xex_sha256": TARGET_XEX_SHA256},
            "action_id": digest({"run": observation.run_id, "decision": self.decisions, "restore": target.cell_id}),
            "observation_frame": observation.guest_frame,
            "observation_sha256": digest(observation.raw),
            "cell_id": current.cell_id,
            "situation": observation.situation,
            "operation": "restore",
            "checkpoint": target.checkpoint,
            "target_cell_id": target.cell_id,
            "hold_frames": 0,
            "controller": Controller().document(),
            "stop_on_visual_change": False,
            "score": {"go_explore_restore": 1.0},
            "behavior": self.behavior.name,
        }
        self.trace.append("decision", action)
        self.pending = action
        self.path = list(target.best_path)
        self.decisions += 1
        return action

    def reward(self, observation: Observation, created: bool) -> float:
        reward = 1.0 if created else -0.02
        if observation.situation in self.behavior.preferred_situations:
            reward += 1.0
        if observation.situation == "mission_result":
            reward += 10.0
        if observation.terminal:
            reward += 2.0
        return reward

    def step(self, observation: Observation) -> dict[str, Any]:
        if self.decisions >= self.behavior.max_decisions:
            raise ContractError("decision budget exhausted")
        if self.frames >= self.behavior.max_guest_frames:
            raise ContractError("guest-frame budget exhausted")
        self.validate_receipt(observation)
        cell, created = self.archive.observe(observation, self.path)
        if self.pending is not None:
            previous_cell = self.pending["cell_id"]
            previous_option = self.pending.get("option")
            previous_visual = self.pending.get("visual_signature")
            changed = previous_visual != observation.visual_signature
            reward = self.reward(observation, created) + (0.25 if changed else -0.1)
            if previous_option is not None:
                self.archive.update(previous_cell, previous_option, reward, cell.cell_id)
                self.trace.append(
                    "feedback",
                    {
                        "cell_id": previous_cell,
                        "option": previous_option,
                        "outcome": cell.cell_id,
                        "reward": reward,
                        "visual_changed": changed,
                    },
                )
        restore = self.restore_action(observation, cell)
        if restore is not None:
            return restore
        option, score = self.policy.choose(cell, self.archive)
        if self.frames + option.hold_frames > self.behavior.max_guest_frames:
            option = self.options["neutral_1"]
            score = {"budget_guard": 1.0}
        action_id = digest(
            {
                "run": observation.run_id,
                "frame": observation.guest_frame,
                "cell": cell.cell_id,
                "option": option.name,
                "decision": self.decisions,
            }
        )
        action = {
            "schema": ACTION_SCHEMA,
            "identity": {"xex_sha256": TARGET_XEX_SHA256},
            "action_id": action_id,
            "observation_frame": observation.guest_frame,
            "observation_sha256": digest(observation.raw),
            "cell_id": cell.cell_id,
            "situation": observation.situation,
            "operation": "input",
            "option": option.name,
            "hold_frames": option.hold_frames,
            "controller": option.controller.document(),
            "stop_on_visual_change": option.hold_frames > 1,
            "score": score,
            "behavior": self.behavior.name,
        }
        self.trace.append("decision", action)
        self.pending = dict(action, visual_signature=observation.visual_signature)
        self.path.append(option.name)
        self.decisions += 1
        self.frames += option.hold_frames
        return action


def read_behavior(path: Path | None, prompt: str, options: dict[str, Option]) -> Behavior:
    if path is not None:
        row = json.loads(path.read_text())
        if row.pop("schema", None) != BEHAVIOR_SCHEMA:
            raise ContractError("unsupported behavior schema")
        behavior = Behavior(**row)
    else:
        behavior = compile_prompt(prompt, options) if prompt else Behavior()
    behavior.validate(options)
    return behavior


def atomic_write(path: Path, document: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".tmp.{os.getpid()}")
    temporary.write_bytes(canonical(document))
    os.replace(temporary, path)


def serve(arguments: argparse.Namespace) -> int:
    options = default_options()
    behavior = read_behavior(arguments.behavior, arguments.prompt, options)
    archive = Archive.load(arguments.archive)
    classifier = VisionClassifier.load(arguments.vision_rules)
    capabilities = validate_bridge_capabilities(arguments.capabilities)
    trace_path: Path = arguments.trace
    if trace_path.exists() and trace_path.stat().st_size:
        raise ContractError("refusing discovery trace collision")
    trace_path.parent.mkdir(parents=True, exist_ok=True)
    with trace_path.open("a", encoding="utf-8") as trace_stream:
        trace = TraceWriter(trace_stream)
        trace.append(
            "session",
            {
                "identity": {"xex_sha256": TARGET_XEX_SHA256},
                "seed": arguments.seed,
                "behavior": behavior.document(),
                "transport": "jsonl-frame-boundary",
                "bridge_capabilities_sha256": digest(capabilities),
            },
        )
        engine = Engine(behavior, archive, arguments.seed, trace, strict_receipts=True)
        for line in sys.stdin:
            if not line.strip():
                continue
            observation = Observation.parse(classifier.apply(json.loads(line)))
            action = engine.step(observation)
            sys.stdout.write(canonical(action).decode())
            sys.stdout.flush()
            atomic_write(arguments.archive, archive.document())
            if observation.terminal:
                break
    return 0


def validate_trace(path: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    previous = "0" * 64
    for sequence, line in enumerate(path.read_text().splitlines()):
        row = json.loads(line)
        if row.get("schema") != TRACE_SCHEMA or row.get("sequence") != sequence:
            raise ContractError("trace sequence/schema mismatch")
        if row.get("previous_sha256") != previous:
            raise ContractError("trace chain mismatch")
        claimed = row.pop("record_sha256", None)
        actual = digest(row)
        row["record_sha256"] = claimed
        if claimed != actual:
            raise ContractError("trace record hash mismatch")
        previous = claimed
        records.append(row)
    return records


def replay(arguments: argparse.Namespace) -> int:
    records = validate_trace(arguments.trace)
    frames = []
    cursor = 0
    for row in records:
        if row["kind"] != "decision":
            continue
        action = row["payload"]
        if action.get("operation", "input") == "restore":
            frames.append(
                {
                    "start_frame": cursor,
                    "hold_frames": 0,
                    "operation": "restore",
                    "checkpoint": action["checkpoint"],
                    "target_cell_id": action["target_cell_id"],
                    "action_id": action["action_id"],
                }
            )
            continue
        frames.append(
            {
                "start_frame": cursor,
                "hold_frames": action["hold_frames"],
                "operation": "input",
                "option": action["option"],
                "controller": action["controller"],
                "action_id": action["action_id"],
            }
        )
        cursor += action["hold_frames"]
    movie = {
        "schema": MOVIE_SCHEMA,
        "identity": {"xex_sha256": TARGET_XEX_SHA256},
        "source_trace_sha256": hashlib.sha256(arguments.trace.read_bytes()).hexdigest(),
        "total_frames": cursor,
        "frames": frames,
    }
    atomic_write(arguments.output, movie)
    print(f"AC6_AGENT_REPLAY_PASS actions={len(frames)} frames={cursor} sha256={digest(movie)}")
    return 0


def compile_behavior_command(arguments: argparse.Namespace) -> int:
    behavior = compile_prompt(arguments.prompt, default_options())
    atomic_write(arguments.output, behavior.document())
    print(f"AC6_AGENT_BEHAVIOR_PASS output={arguments.output} sha256={digest(behavior.document())}")
    return 0


def validate_capabilities_command(arguments: argparse.Namespace) -> int:
    document = validate_bridge_capabilities(arguments.path)
    print(f"AC6_AGENT_BRIDGE_PASS sha256={digest(document)}")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subparsers = result.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("serve", help="synchronous JSONL observation/action loop")
    run.add_argument("--archive", type=Path, required=True)
    run.add_argument("--trace", type=Path, required=True)
    run.add_argument("--behavior", type=Path)
    run.add_argument("--prompt", default="")
    run.add_argument("--vision-rules", type=Path)
    run.add_argument("--capabilities", type=Path, required=True)
    run.add_argument("--seed", type=int, default=0xAC6)
    run.set_defaults(function=serve)
    convert = subparsers.add_parser("replay", help="compile a discovery trace to a movie")
    convert.add_argument("trace", type=Path)
    convert.add_argument("--output", type=Path, required=True)
    convert.set_defaults(function=replay)
    behavior = subparsers.add_parser("compile-behavior")
    behavior.add_argument("prompt")
    behavior.add_argument("--output", type=Path, required=True)
    behavior.set_defaults(function=compile_behavior_command)
    capabilities = subparsers.add_parser("validate-capabilities")
    capabilities.add_argument("path", type=Path)
    capabilities.set_defaults(function=validate_capabilities_command)
    return result


def main() -> int:
    try:
        arguments = parser().parse_args()
        return int(arguments.function(arguments))
    except (ContractError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"AC6 agent error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
