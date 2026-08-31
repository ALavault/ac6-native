from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
IDENTITY = ROOT / "analysis/oracle/ac6-recomp-ab90b-us/content-identity.json"


def test_us_content_identity_is_metadata_only_and_complete() -> None:
    document = json.loads(IDENTITY.read_text(encoding="utf-8"))
    assert document["schema"] == "ac6.retail-content-identity.v1"
    assert document["status"] == "qualified-metadata-only"
    assert document["target"] == "ntsc-uj"
    assert document["source_media"]["retail_bytes_committed"] is False
    files = document["files"]
    assert set(files) == {
        "default.xex",
        "DATA.TBL",
        "DATA00.PAC",
        "DATA01.PAC",
        "bgmpack.bin",
        "demopack_eng.bin",
        "demopack_jpn.bin",
        "moviepack.bin",
        "voicepack_eng.bin",
        "voicepack_jpn.bin",
    }
    assert files["DATA.TBL"]["sha256"] == (
        "bad3a157eb75c839d9d6187f69ac061dee6d075d9cd61e0aa73b533473863b2f"
    )
    assert files["DATA00.PAC"]["size"] == 2266267648
    assert files["moviepack.bin"]["size"] == 698417152
    for name, record in files.items():
        assert record["size"] > 0, name
        assert len(record["sha256"]) == 64, name
        assert record["iso_offset"] >= 0, name

