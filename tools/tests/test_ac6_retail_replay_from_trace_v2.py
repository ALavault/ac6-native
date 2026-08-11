from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from build_ac6_execution_trace_v2 import DOMAINS  # noqa: E402
from build_ac6_retail_replay_from_trace_v2 import (  # noqa: E402
    ReplayBuildError,
    build_replay,
    read_cache_identity,
)


def make_cache(root: Path) -> tuple[Path, bytes]:
    cache = root / "cache"
    indices = cache / "indices"
    indices.mkdir(parents=True)
    index = b"qualified sealed index fixture"
    digest = hashlib.sha256(index).digest()
    (indices / f"{digest.hex()}.ac6idx").write_bytes(index)
    (cache / "current").write_bytes(b"AC6RCUR\0" + struct.pack(">II", 2, 48) + digest)
    return cache, digest


def payload(domain: str, tick: int) -> dict:
    if domain == "controller_input":
        return {
            "pitch": -tick,
            "roll": tick,
            "yaw": -32768 if tick == 1 else 32767,
            "throttle": 255,
            "buttons": 0x0300,
        }
    if domain == "output_hashes":
        return {"simulation": "a" * 64}
    return {"tick": tick}


def make_raw(path: Path, ticks: int = 2) -> None:
    events = []
    for tick in range(1, ticks + 1):
        for domain in DOMAINS:
            events.append({
                "sequence": len(events),
                "tick": tick,
                "domain": domain,
                "payload": payload(domain, tick),
            })
    path.write_text("".join(json.dumps(event) + "\n" for event in events),
                    encoding="utf-8")


def test_builds_exact_legacy_v2_replay(tmp_path: Path) -> None:
    cache, digest = make_cache(tmp_path)
    raw = tmp_path / "raw.jsonl"
    make_raw(raw)
    replay = build_replay(raw, cache, 1, 2)

    assert replay[:9] == b"AC6RTPLY\0"
    assert struct.unpack_from("<IIIIII", replay, 9) == (2, 1, 1, 1, 1, 1)
    assert replay[33:65] == digest
    assert struct.unpack_from("<I", replay, 65) == (4,)
    assert struct.unpack_from("<hhhBH", replay, 69) == (-1, 1, -32768, 255, 0x0300)
    assert struct.unpack_from("<hhhBH", replay, 78) == (-1, 1, -32768, 255, 0x0300)
    assert struct.unpack_from("<hhhBH", replay, 87) == (-2, 2, 32767, 255, 0x0300)
    assert struct.unpack_from("<hhhBH", replay, 96) == (-2, 2, 32767, 255, 0x0300)
    assert len(replay) == 69 + 4 * 9


def test_cache_identity_requires_big_endian_current_and_matching_index(tmp_path: Path) -> None:
    cache, digest = make_cache(tmp_path)
    assert read_cache_identity(cache) == digest
    (cache / "current").write_bytes(b"AC6RCUR\0" + struct.pack("<II", 2, 48) + digest)
    with pytest.raises(ReplayBuildError, match="current record identity"):
        read_cache_identity(cache)


def test_controller_ranges_fail_closed(tmp_path: Path) -> None:
    cache, _ = make_cache(tmp_path)
    raw = tmp_path / "raw.jsonl"
    make_raw(raw, 1)
    events = [json.loads(line) for line in raw.read_text(encoding="utf-8").splitlines()]
    events[0]["payload"]["throttle"] = 256
    raw.write_text("".join(json.dumps(event) + "\n" for event in events),
                   encoding="utf-8")
    with pytest.raises(ReplayBuildError, match="controller throttle range"):
        build_replay(raw, cache, 1, 1)
