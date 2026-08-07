#!/usr/bin/env python3
"""Emit the Mission 01 retail manifests the native product consumes.

Every value in the generated files is either read from the retail scenario
payload or is a declared placeholder. Nothing is inferred from a name, a
cardinality coincidence or a plausible reading. The provenance of each column
is stated in the file header the generator writes, and again here:

waves.tsv - one row per parsed unit record, in the order the retail consumer
0x820A7070 iterates them:

    mission_id        1, the mission this payload belongs to
    spawn_tick        1 for every row. Retail constructs every record in one
                      pass at mission load, so there is no per-record tick to
                      read. The native director rejects tick 0, hence 1.
    unit_id           4097 + element index. Retail stores the element index
                      at constructed-object +0xD0; the base is the native
                      entity base already used elsewhere in the product, and is
                      needed because the native registry rejects a unit whose
                      id equals its owner id.
    owner             faction index + 1, read from record data +0x0D
    asset             the object class 0x820A7F48 selects from data +0x08
    faction           same as owner, which the native runtime requires
    x, y, z           the first Obj sub-record's three floats
    health/max_health 1.0. THE PARSED RECORD CARRIES NO DURABILITY FIELD.
    collision_radius  1.0, same reason

objectives.tsv - one row per parsed sub-mission, four columns, so the
condition defaults to Manual: retail advances a sub-mission from its own
script, never from a completion predicate the native runtime could evaluate.

ai.tsv is deliberately NOT emitted. The native format is a periodic firing
rule (first_tick, period_ticks, entity, target, weapon). The retail payload
carries no such thing: unit behaviour is a Set -> Act -> Order program with no
entity-to-entity reference and no period. Writing one would be invention.

Usage:
    python3 tools/emit_mission01_retail_manifests.py PAYLOAD --output DIR
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from emit_ac6_native_snapshot import Payload  # noqa: E402

MISSION_ID = 1
#: The native entity base; retail element index 0 becomes entity 4097.
ENTITY_BASE = 4097

#: data +0x08 -> the class 0x820A7F48 allocates, before the faction refinement
#: that only applies in game modes 2 and 3. Read off the switch at 0x820A72E0.
CLASS_TO_OBJECT_CATEGORY = {0: 1, 1: 4, 2: 4, 3: 4, 4: 3}


class Scenario:
    """The parsed Mission 01 scenario tree, addressed the way retail does."""

    def __init__(self, payload: Payload) -> None:
        self.p = payload
        self.root_slots = payload.children(0)

    def slot(self, index: int) -> int:
        return self.root_slots[index]

    def unit_records(self) -> list[dict]:
        """Slot 0, 'Obj & Unit' - what 0x820A7070 iterates."""
        records = []
        for index, wrapper in enumerate(self.p.children(self.slot(0))):
            node = self.p.children(wrapper)[0]
            data = self.p.resolve(node, 0)
            children = self.p.children(node)
            objs = []
            if len(children) > 1 and self.p.present(children[1]):
                for obj in self.p.children(children[1]):
                    obj_data = self.p.resolve(obj, 0)
                    objs.append(struct.unpack_from(">3f", self.p.data, obj_data))
            records.append({
                "index": index,
                "class_byte": self.p.u8(data + 0x08),
                "faction_index": self.p.u8(data + 0x0D),
                "object_category": CLASS_TO_OBJECT_CATEGORY[self.p.u8(data + 0x08)],
                "has_behaviour_set": len(children) > 0 and self.p.present(children[0]),
                "objects": objs,
            })
        return records

    def factions(self) -> list[dict]:
        """Slot 5 - the table data +0x0D indexes, sized into context+0x58."""
        table = self.slot(5)
        entries = []
        for index, node in enumerate(self.p.children(table)):
            data = node + self.p.u32(node)
            entries.append({
                "index": index,
                "side_code": self.p.u8(data + 0x2C),
                "word_0x28": self.p.u32(data + 0x28),
            })
        return entries

    def sub_missions(self) -> list[dict]:
        """Slot 2 - what context+0x10 indexes and context+0x2C timestamps."""
        result = []
        for index, node in enumerate(self.p.children(self.slot(2))):
            steps = []
            for child in self.p.children(node):
                for step in self.p.children(child):
                    data = self.p.resolve(step, 0)
                    if data is not None:
                        steps.append(self.p.u8(data))
            result.append({"index": index, "steps": steps})
        return result


def waves_rows(records: list[dict]) -> list[list[str]]:
    rows = []
    for record in records:
        x, y, z = record["objects"][0] if record["objects"] else (0.0, 0.0, 0.0)
        rows.append([
            str(MISSION_ID),
            "1",
            str(ENTITY_BASE + record["index"]),
            str(record["faction_index"] + 1),
            str(record["object_category"]),
            str(record["faction_index"] + 1),
            f"{x:.6f}", f"{y:.6f}", f"{z:.6f}",
            "1.000000", "1.000000", "1.000000",
        ])
    return rows


def objectives_rows(sub_missions: list[dict]) -> list[list[str]]:
    return [[
        str(MISSION_ID),
        str(entry["index"] + 1),
        f"mission01-submission-{entry['index']}",
        "1",
    ] for entry in sub_missions]


WAVES_HEADER = """\
# Mission 01 retail unit records, one row per element of the parsed
# 'Obj & Unit' slot, in the order 0x820A7070 iterates them.
# columns: mission_id spawn_tick unit_id owner asset faction x y z health max_health collision_radius
# spawn_tick is 1 for every row: retail builds every record in a single pass
#   at mission load and the payload carries no per-record tick.
# unit_id is 4097 + the element index (retail stores the index at object +0xD0;
#   the native registry rejects an id equal to the owner id, hence the base).
# owner and faction are both the record's faction byte (data +0x0D) + 1.
# asset is the object class 0x820A7F48 selects from the class byte (data +0x08).
# x y z are the first Obj sub-record's three floats.
# health, max_health and collision_radius are PLACEHOLDERS: the parsed record
#   carries no durability or collision field. No retail claim is made on them.
"""

OBJECTIVES_HEADER = """\
# Mission 01 retail sub-missions, one row per element of the parsed
# sub-mission table (root slot 2), which context+0x10 indexes and
# context+0x2C timestamps.
# columns: mission_id objective_id stable_id required
# Four columns, so the condition is Manual: retail advances a sub-mission from
# its own script (0x8226E158), not from a predicate the native runtime could
# evaluate. Nothing here completes an objective by itself.
"""


def write_tsv(path: Path, header: str, rows: list[list[str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        stream.write(header)
        for row in rows:
            stream.write("\t".join(row) + "\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args(argv)

    raw = arguments.payload.read_bytes()
    scenario = Scenario(Payload(raw))
    records = scenario.unit_records()
    factions = scenario.factions()
    sub_missions = scenario.sub_missions()

    write_tsv(arguments.output / "waves.tsv", WAVES_HEADER, waves_rows(records))
    write_tsv(arguments.output / "objectives.tsv", OBJECTIVES_HEADER,
              objectives_rows(sub_missions))

    census = {
        "schema": "ac6.mission01-retail-census.v1",
        "payload_sha256": hashlib.sha256(raw).hexdigest(),
        "unit_records": len(records),
        "class_byte_histogram": histogram(r["class_byte"] for r in records),
        "faction_index_histogram": histogram(r["faction_index"] for r in records),
        "object_category_histogram": histogram(r["object_category"] for r in records),
        "objects_per_record_histogram": histogram(len(r["objects"]) for r in records),
        "object_records": sum(len(r["objects"]) for r in records),
        "records_with_behaviour_set": sum(1 for r in records if r["has_behaviour_set"]),
        "factions": factions,
        "sub_missions": sub_missions,
    }
    (arguments.output / "census.json").write_text(
        json.dumps(census, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({key: census[key] for key in (
        "unit_records", "class_byte_histogram", "faction_index_histogram",
        "object_records")}, sort_keys=True))
    return 0


def histogram(values) -> dict[str, int]:
    counts: dict[str, int] = {}
    for value in values:
        counts[str(value)] = counts.get(str(value), 0) + 1
    return dict(sorted(counts.items(), key=lambda item: int(item[0])))


if __name__ == "__main__":
    raise SystemExit(main())
