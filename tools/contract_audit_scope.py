#!/usr/bin/env python3
"""Select live contracts while retaining checkable supersession metadata."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


class ContractScopeError(ValueError):
    """A contract cannot be placed safely in the current audit scope."""


@dataclass(frozen=True)
class ScopedContract:
    path: Path
    document: dict


@dataclass(frozen=True)
class SupersededContract:
    path: Path
    replacement: Path


def _load(path: Path) -> dict:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ContractScopeError(f"unreadable contract {path}: {exc}") from exc
    if not isinstance(document, dict):
        raise ContractScopeError(f"contract root is not an object: {path}")
    return document


def _replacement_path(contract: Path, declared: str) -> Path:
    candidates = (Path(declared), contract.parent / declared)
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise ContractScopeError(
        f"superseded contract {contract} names missing replacement {declared}"
    )


def current_contracts(
    paths: Iterable[str | Path],
) -> tuple[list[ScopedContract], list[SupersededContract]]:
    """Partition contracts into current and superseded records.

    A ``superseded_by`` marker excludes historical evidence from current
    audits, but it is not a free-form escape hatch: the marker must name a
    readable contract and may not point back to the historical file itself.
    """
    active: list[ScopedContract] = []
    superseded: list[SupersededContract] = []
    for raw_path in paths:
        path = Path(raw_path)
        document = _load(path)
        declared = document.get("superseded_by")
        if declared is None:
            active.append(ScopedContract(path, document))
            continue
        if not isinstance(declared, str) or not declared.strip():
            raise ContractScopeError(
                f"superseded_by must be a non-empty path in {path}"
            )
        replacement = _replacement_path(path, declared)
        if replacement.resolve() == path.resolve():
            raise ContractScopeError(f"contract supersedes itself: {path}")
        _load(replacement)
        superseded.append(SupersededContract(path, replacement))
    return active, superseded


def print_superseded(records: Iterable[SupersededContract]) -> None:
    for record in records:
        print(
            "contract_scope=historical "
            f"contract={record.path} superseded_by={record.replacement}"
        )
