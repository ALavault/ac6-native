"""Seeded AC6 controller variants and metadata-only episode comparisons."""

from __future__ import annotations

import copy
import random
from typing import Any, Mapping

from ..protocol import hash_timeline, load_request, normalize_request, sha256_json
from ..runner import run_safe


def _controller_event(index: int, ly: int, *, duration: int = 1) -> dict[str, Any]:
    return {
        "at": {"clock": "xam_input_poll", "index": index},
        "controller": 0,
        "state": {"buttons": [], "lx": 0, "ly": ly, "rx": 0, "ry": 0, "lt": 0, "rt": 0},
        "duration": {"clock": "xam_input_poll", "count": duration},
    }


def edge_timeline_variants(request: Mapping[str, Any], *, seed: int = 6,
                           budget: int = 8) -> list[dict[str, Any]]:
    """Return deterministic, bounded pitch controls around the same start."""
    if budget < 1 or budget > 16:
        raise ValueError("budget must be in range 1..16")
    canonical = load_request(request)
    base = copy.deepcopy(canonical)
    variants: list[tuple[str, int, int]] = [
        ("neutral", 0, 0),
        ("positive_one_poll", 1, 0),
        ("negative_one_poll", -1, 0),
        ("positive_deadzone_edge", 4096, 0),
        ("negative_deadzone_edge", -4096, 0),
        ("positive_saturation", 32767, 0),
        ("negative_saturation", -32768, 0),
        ("positive_shifted_poll", 1, 1),
        ("negative_shifted_poll", -1, 1),
        ("contradictory_axes", 1, 0),
        ("held_positive", 16384, 0),
        ("held_negative", -16384, 0),
        ("t_minus_one_positive", 1, 0),
        ("t_plus_one_negative", -1, 1),
        ("neutral_hold", 0, 0),
        ("positive_short_hold", 2, 0),
    ]
    rng = random.Random(seed)
    order = list(range(len(variants)))
    rng.shuffle(order)
    # Keep the four canonical controls first, then seed the remaining order.
    preferred = [0, 1, 2, 7]
    selected = preferred + [idx for idx in order if idx not in preferred]
    result: list[dict[str, Any]] = []
    for idx in selected[:budget]:
        label, ly, shift = variants[idx]
        item = copy.deepcopy(base)
        item["request_id"] = f"{canonical['request_id']}-{label}"
        item["seed"] = seed
        item["timeline"] = [_controller_event(shift, ly)]
        item["options"] = dict(item.get("options", {}))
        item["options"]["edge_label"] = label
        result.append(normalize_request(item))
    return result


def explore_request(request: Mapping[str, Any], *, seed: int = 6, budget: int = 8,
                    backend: Any = None) -> dict[str, Any]:
    canonical = load_request(request)
    cases = []
    for variant in edge_timeline_variants(canonical, seed=seed, budget=budget):
        result = run_safe(variant, backend=backend)
        cases.append({
            "request_id": variant["request_id"],
            "label": variant.get("options", {}).get("edge_label"),
            "timeline_sha256": hash_timeline(variant["timeline"]),
            "result": result,
        })
    unique = len({case["result"].get("semantic_hash") for case in cases})
    return {
        "schema": "ac6-emu-agent-exploration/v1",
        "execution": "xenia-diagnostic" if canonical.get("backend") == "xenia" else "simulated",
        "seed": seed,
        "budget": budget,
        "cases": cases,
        "deduplication": {"input_cases": len(cases), "unique_cases": unique},
        "timeline_corpus_sha256": sha256_json([case["timeline_sha256"] for case in cases]),
        "proof": {"guest_acceptance": False, "needs_dynamic_evidence": True},
    }


def compare_protocol_results(left: Mapping[str, Any], right: Mapping[str, Any]) -> dict[str, Any]:
    fields = ("target", "qualified", "qualification_failures", "stop_reason", "guest_progress", "observer_liveness", "timeline_sha256", "semantic_hash")
    difference = None
    for field in fields:
        if left.get(field) != right.get(field):
            difference = {"field": field, "left": left.get(field), "right": right.get(field)}
            break
    if difference is None:
        for index, (a, b) in enumerate(zip(left.get("frames", []), right.get("frames", []))):
            if a != b:
                difference = {"field": "frames", "index": index, "left_state_sha256": a.get("state_sha256"), "right_state_sha256": b.get("state_sha256")}
                break
    if difference is None and len(left.get("frames", [])) != len(right.get("frames", [])):
        difference = {"field": "frames.length", "left": len(left.get("frames", [])), "right": len(right.get("frames", []))}
    return {
        "schema": "emu-agent-result-compare/v1",
        "equivalent": difference is None,
        "first_divergence": difference,
        "left_episode_id": left.get("request_id"),
        "right_episode_id": right.get("request_id"),
        "proof": {"guest_aligned": False, "needs_dynamic_evidence": True},
    }


def branch_request(request: Mapping[str, Any], *, timeline: Any = None,
                   seed: int | None = None) -> dict[str, Any]:
    """Run a bounded branch of a full protocol request.

    The branch is a new request envelope; it never mutates the parent and
    keeps the backend/safety policy unchanged.
    """
    parent = load_request(request)
    child = dict(parent)
    child["request_id"] = f"{parent['request_id']}-branch"
    child["timeline"] = list(parent["timeline"] if timeline is None else timeline)
    if seed is not None:
        child["seed"] = seed
    result = run_safe(normalize_request(child))
    result["parent_episode_id"] = parent["request_id"]
    result["branch_timeline_sha256"] = hash_timeline(child["timeline"])
    return result


def minimize_request(request: Mapping[str, Any], *, max_attempts: int = 64) -> dict[str, Any]:
    """Delta-minimize timeline records while preserving a bounded receipt.

    With no live Xenia observer, preservation is structural only and is
    explicitly labelled diagnostic; this routine never claims a retail
    divergence has been minimized.
    """
    parent = load_request(request)
    if not isinstance(max_attempts, int) or isinstance(max_attempts, bool) or not 1 <= max_attempts <= 512:
        raise ValueError("max_attempts must be in range 1..512")
    timeline = list(parent["timeline"])
    attempts = 0
    candidate = timeline
    while len(candidate) > 1 and attempts < max_attempts:
        trial = candidate[:-1]
        attempts += 1
        # The transport is intentionally not invoked for an unqualified
        # minimization; canonical validation is the only safe predicate.
        normalize_request({**parent, "timeline": trial})
        candidate = trial
    child = dict(parent)
    child["request_id"] = f"{parent['request_id']}-minimized"
    child["timeline"] = candidate
    result = run_safe(child)
    return {
        "schema": "ac6-emu-agent-minimize/v1",
        "execution": "diagnostic",
        "original_episode_id": parent["request_id"],
        "original_timeline_sha256": hash_timeline(timeline),
        "minimized_timeline_sha256": hash_timeline(candidate),
        "original_count": len(timeline),
        "minimized_count": len(candidate),
        "attempts": attempts,
        "preserved": False,
        "predicate": "canonical-validation-only; guest divergence unknown",
        "result": result,
        "proof": {"guest_divergence_minimized": False, "needs_dynamic_evidence": True},
    }


__all__ = ["branch_request", "compare_protocol_results", "edge_timeline_variants", "explore_request", "minimize_request"]
