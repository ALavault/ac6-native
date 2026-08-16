"""Deterministic, bounded episode exploration.

The public functions are deliberately pure with respect to the host: they do
not launch a process, read a path, or mutate emulator memory.  A future real
backend can be attached at the MCP boundary once it provides the qualified
observer and replay evidence required by the project.
"""

from .engine import (
    ExplorationError,
    MAX_ACTIONS,
    MAX_FRAMES,
    SAFE_CONTROLS,
    SCHEMA,
    compare_episodes,
    explore,
    explore_edge,
    inspect_episode,
    minimize_episode,
    replay,
    run_episode,
    branch_episode,
    canonical_digest,
    normalize_actions,
)
from .ac6 import branch_request, compare_protocol_results, edge_timeline_variants, explore_request, minimize_request

__all__ = [
    "ExplorationError",
    "MAX_ACTIONS",
    "MAX_FRAMES",
    "SAFE_CONTROLS",
    "SCHEMA",
    "SAFE_CONTROLS",
    "branch_episode",
    "canonical_digest",
    "compare_episodes",
    "explore",
    "explore_edge",
    "inspect_episode",
    "minimize_episode",
    "normalize_actions",
    "replay",
    "run_episode",
    "compare_protocol_results",
    "branch_request",
    "edge_timeline_variants",
    "explore_request",
    "minimize_request",
]
