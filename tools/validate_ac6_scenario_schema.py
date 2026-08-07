#!/usr/bin/env python3
"""Validate a derived AC6 scenario schema against real retail bytes.

The schema in ``analysis/scenario-schema/`` is read out of the decompiled
``*Bin`` parsers, not guessed from payload shape.  This validator closes the
loop: it walks the payload with exactly the container rules the parsers
implement and asserts the invariants the code guarantees.  A schema that
disagrees with the bytes fails here rather than surviving as prose.

It never writes, and it needs no emulator, bridge or native run.

Usage
-----
    python3 tools/validate_ac6_scenario_schema.py \\
        analysis/scenario-schema/ObjBin.json \\
        reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

# Every *Bin container stores offsets relative to their own base. A node is
# {u32 data_off; u32 table_off}; a table is {s32 count; u32 child_off[count]}.
NODE_DATA_OFF = 0x00
NODE_TABLE_OFF = 0x04

# Upper bound on a plausible child count. The real tables are far smaller; this
# only stops a misparse from allocating against a garbage length.
MAX_CHILDREN = 8192


class Payload:
    """A decoded retail node graph addressed by absolute payload offsets."""

    def __init__(self, data: bytes) -> None:
        self.data = data

    def u32(self, offset: int) -> int:
        if offset < 0 or offset + 4 > len(self.data):
            raise ValueError(f"u32 read out of range at {offset:#x}")
        return struct.unpack_from(">I", self.data, offset)[0]

    def s32(self, offset: int) -> int:
        if offset < 0 or offset + 4 > len(self.data):
            raise ValueError(f"s32 read out of range at {offset:#x}")
        return struct.unpack_from(">i", self.data, offset)[0]

    def u8(self, offset: int) -> int:
        if offset < 0 or offset >= len(self.data):
            raise ValueError(f"u8 read out of range at {offset:#x}")
        return self.data[offset]

    def node_data(self, node: int) -> int | None:
        """node.data_off resolved, or None when the parser treats it as absent."""
        offset = self.u32(node + NODE_DATA_OFF)
        if offset == 0:
            return None
        resolved = node + offset
        return resolved if 0 <= resolved < len(self.data) else None

    def children(self, node: int) -> list[int]:
        offset = self.u32(node + NODE_TABLE_OFF)
        if offset == 0:
            return []
        table = node + offset
        if table + 4 > len(self.data):
            return []
        count = self.s32(table)
        if not 0 <= count <= MAX_CHILDREN:
            return []
        if table + 4 + 4 * count > len(self.data):
            return []
        return [table + self.u32(table + 4 + 4 * index) for index in range(count)]

    def present(self, node: int | None) -> bool:
        """The exact emptiness test every *Bin reader performs before a slot."""
        if node is None or node + 8 > len(self.data) or node < 0:
            return False
        return self.u32(node) != 0 or self.u32(node + 4) != 0


def walk_obj_records(payload: Payload) -> tuple[list[int], int, list[str]]:
    """Reach every ObjBin node from the scenario root along the decompiled path.

    root slot 0 -> per-entry node -> 0x8232F380 array -> 0x8232F198 -> ObjBin.
    """
    problems: list[str] = []
    root_children = payload.children(0)
    if not root_children:
        return [], 0, ["scenario root has no child table"]
    slot0 = root_children[0]
    entries = payload.children(slot0)

    records: list[int] = []
    for index, entry in enumerate(entries):
        level1 = payload.children(entry)
        if len(level1) != 1:
            problems.append(f"slot0[{index}] has {len(level1)} children, expected 1")
            continue
        dispatch = level1[0]
        slots = payload.children(dispatch)
        if len(slots) < 2:
            problems.append(f"slot0[{index}] dispatch node has {len(slots)} slots, expected 2")
            continue
        array_node = slots[1]
        data = payload.node_data(array_node)
        if data is None:
            problems.append(f"slot0[{index}] array node has no data word")
            continue
        declared = payload.u8(data)
        available = payload.children(array_node)
        if declared > len(available):
            problems.append(
                f"slot0[{index}] declares {declared} entries but the table holds {len(available)}")
        for element in available[:declared]:
            if not payload.present(element):
                continue
            inner = payload.children(element)
            if not inner or not payload.present(inner[0]):
                continue
            records.append(inner[0])
    return records, len(entries), problems


def validate_obj_bin(schema: dict, payload: Payload) -> dict:
    records, slot0_entries, problems = walk_obj_records(payload)

    max_children = len(schema["record"]["fields"]) - 1  # data word is not a child
    children_hist: Counter[int] = Counter()
    tag_hist: Counter[int] = Counter()
    weapon_hist: Counter[int] = Counter()
    maneuver_hist: Counter[int] = Counter()

    for record in records:
        children = payload.children(record)
        children_hist[len(children)] += 1
        if len(children) > max_children:
            problems.append(
                f"Obj node {record:#x} has {len(children)} children, schema allows {max_children}")

        if children and payload.present(children[0]):
            data = payload.node_data(children[0])
            if data is not None:
                tag = payload.u8(data)
                tag_hist[tag] += 1
                if tag > 2:
                    problems.append(f"Obj node {record:#x} param tag {tag} outside 0..2")

        weapons = sum(1 for slot in (3, 4, 5)
                      if len(children) > slot and payload.present(children[slot]))
        weapon_hist[weapons] += 1

        if len(children) > 1 and payload.present(children[1]):
            block = payload.children(children[1])
            filled = sum(1 for entry in block if payload.present(entry))
            maneuver_hist[filled] += 1
            if filled > 8:
                problems.append(
                    f"Obj node {record:#x} has {filled} maneuvers, schema allows 8")

    return {
        "slot0_entries": slot0_entries,
        "obj_records_reached": len(records),
        "inconsistencies": len(problems),
        "children_per_obj_node": dict(sorted(children_hist.items())),
        "param_tag_distribution": dict(sorted(tag_hist.items())),
        "weapon_slots_filled": dict(sorted(weapon_hist.items())),
        "maneuvers_per_obj": dict(sorted(maneuver_hist.items())),
        "problems": problems[:20],
    }


def walk_set_nodes(payload: Payload) -> tuple[list[int], list[str]]:
    """Collect every SetBin node reachable from the scenario root.

    Two readers hand a node to SetBin::read: 0x8232CCA0 at child[0] of each
    level-0 entry, and 0x8232F198 at child[1] of each Obj element. Both are
    walked so no Order is missed.
    """
    problems: list[str] = []
    sets: list[int] = []
    root_children = payload.children(0)
    if not root_children:
        return [], ["scenario root has no child table"]

    for index, entry in enumerate(payload.children(root_children[0])):
        level1 = payload.children(entry)
        if len(level1) != 1:
            problems.append(f"slot0[{index}] has {len(level1)} children, expected 1")
            continue
        slots = payload.children(level1[0])
        if len(slots) < 2:
            problems.append(f"slot0[{index}] dispatch node has {len(slots)} slots, expected 2")
            continue
        if payload.present(slots[0]):
            sets.append(slots[0])

        array_node = slots[1]
        data = payload.node_data(array_node)
        if data is None:
            continue
        declared = payload.u8(data)
        available = payload.children(array_node)
        for element in available[:declared]:
            if not payload.present(element):
                continue
            inner = payload.children(element)
            if len(inner) > 1 and payload.present(inner[1]):
                sets.append(inner[1])
    return sets, problems


def walk_order_records(payload: Payload) -> tuple[list[int], dict, list[str]]:
    """SetBin -> ActBin -> OrderBin, using each container's own u8 count."""
    sets, problems = walk_set_nodes(payload)
    orders: list[int] = []
    counters = {"set_nodes": len(sets), "act_nodes": 0}

    for set_node in sets:
        data = payload.node_data(set_node)
        if data is None:
            continue
        act_count = payload.u8(data)
        act_children = payload.children(set_node)
        if act_count > len(act_children):
            problems.append(
                f"SetBin {set_node:#x} declares {act_count} acts, table holds {len(act_children)}")
        for act_node in act_children[:act_count]:
            if not payload.present(act_node):
                continue
            counters["act_nodes"] += 1
            act_data = payload.node_data(act_node)
            if act_data is None:
                continue
            order_count = payload.u8(act_data)
            order_children = payload.children(act_node)
            if order_count > len(order_children):
                problems.append(
                    f"ActBin {act_node:#x} declares {order_count} orders, "
                    f"table holds {len(order_children)}")
            for order_node in order_children[:order_count]:
                if payload.present(order_node):
                    orders.append(order_node)
    return orders, counters, problems


def _tag2_descent(payload: Payload, children: list[int]) -> str:
    """Classify how far 0x82331AD0's descent gets on a tag-2 order.

    The reader needs child[0], then that node's own data word, then its table
    with at least one present child. Anything short of that makes it stop
    without writing the nested list.
    """
    if not children or not payload.present(children[0]):
        return "no_child0"
    if payload.node_data(children[0]) is None:
        return "child0_without_data"
    grandchildren = payload.children(children[0])
    if not grandchildren:
        return "child0_without_table"
    if not payload.present(grandchildren[0]):
        return "grandchild0_absent"
    return "descends"


def validate_order_bin(schema: dict, payload: Payload) -> dict:
    orders, counters, problems = walk_order_records(payload)

    tag_hist: Counter[int] = Counter()
    children_hist: Counter[int] = Counter()
    tag2_nested: Counter[str] = Counter()

    for order in orders:
        data = payload.node_data(order)
        if data is None:
            problems.append(f"Order node {order:#x} has no data word")
            continue
        tag = payload.u8(data)
        tag_hist[tag] += 1
        if tag > 9:
            problems.append(f"Order node {order:#x} tag {tag} outside the 0..9 the reader handles")

        children = payload.children(order)
        children_hist[len(children)] += 1

        if tag == 2:
            # 0x82331AD0 receives child[0] and only descends when that node has
            # a table with a present child of its own. Check the precondition
            # the reader enforces; do not invent a meaning for what is below it.
            tag2_nested[_tag2_descent(payload, children)] += 1

    return {
        "set_nodes": counters["set_nodes"],
        "act_nodes": counters["act_nodes"],
        "order_records_reached": len(orders),
        "inconsistencies": len(problems),
        "tag_distribution": dict(sorted(tag_hist.items())),
        "children_per_order_node": dict(sorted(children_hist.items())),
        "tag2_descent": dict(sorted(tag2_nested.items())),
        "problems": problems[:20],
    }


def validate_act_bin(schema: dict, payload: Payload) -> dict:
    """Check the two list headers, SetBin and ActBin, against their own rules.

    Both are pure list headers of 0x08: a resolved data word whose first byte is
    the element count, and a pointer to the element array. The interesting
    invariant is the relation between that declared count and the child table,
    because overrunning it is exactly what the `act empty! : %d` and
    `order empty! : %d` paths exist to catch.
    """
    sets, problems = walk_set_nodes(payload)

    set_counts: Counter[int] = Counter()
    act_counts: Counter[int] = Counter()
    set_overruns = 0
    act_overruns = 0
    empty_acts = 0
    act_nodes = 0

    for set_node in sets:
        data = payload.node_data(set_node)
        if data is None:
            problems.append(f"SetBin {set_node:#x} has no data word")
            continue
        declared = payload.u8(data)
        set_counts[declared] += 1
        children = payload.children(set_node)
        if declared > len(children):
            set_overruns += 1
            problems.append(
                f"SetBin {set_node:#x} declares {declared} acts, table holds {len(children)}")

        for act_node in children[:declared]:
            if not payload.present(act_node):
                continue
            act_nodes += 1
            act_data = payload.node_data(act_node)
            if act_data is None:
                problems.append(f"ActBin {act_node:#x} has no data word")
                continue
            order_count = payload.u8(act_data)
            act_counts[order_count] += 1
            if order_count == 0:
                # read() returns before touching the child table.
                empty_acts += 1
                continue
            order_children = payload.children(act_node)
            if order_count > len(order_children):
                act_overruns += 1
                problems.append(
                    f"ActBin {act_node:#x} declares {order_count} orders, "
                    f"table holds {len(order_children)}")

    return {
        "set_nodes": len(sets),
        "act_nodes": act_nodes,
        "acts_declaring_zero_orders": empty_acts,
        "set_count_overruns": set_overruns,
        "act_count_overruns": act_overruns,
        "inconsistencies": len(problems),
        "acts_per_set": dict(sorted(set_counts.items())),
        "orders_per_act": dict(sorted(act_counts.items())),
        "problems": problems[:20],
    }


def validate_maneuver_bin(schema: dict, payload: Payload) -> dict:
    """Walk ObjBin -> maneuver block -> ManeuverBin -> ComTblBin.

    ManeuverBin is the only class in the family whose count is an s32 rather
    than a u8, and its reader takes two hops per element before reaching the
    ComTblBin node. Both are checked here against the fail-closed paths the
    reader implements.
    """
    records, _, problems = walk_obj_records(payload)

    maneuver_nodes = 0
    elements = 0
    count_hist: Counter[int] = Counter()
    comtbl_count_hist: Counter[int] = Counter()
    descent_hist: Counter[str] = Counter()
    negative_counts = 0
    table_overruns = 0

    for record in records:
        children = payload.children(record)
        if len(children) < 2 or not payload.present(children[1]):
            continue
        for slot in payload.children(children[1]):
            if not payload.present(slot):
                continue
            maneuver_nodes += 1
            data = payload.node_data(slot)
            if data is None:
                problems.append(f"ManeuverBin {slot:#x} has no data word")
                continue
            count = payload.s32(data)
            count_hist[count] += 1
            if count < 0:
                negative_counts += 1
                problems.append(f"ManeuverBin {slot:#x} has negative count {count}")
                continue
            table = payload.children(slot)
            if count > len(table):
                table_overruns += 1
                problems.append(
                    f"ManeuverBin {slot:#x} declares {count} elements, "
                    f"table holds {len(table)}")

            for element in table[:count]:
                elements += 1
                if not payload.present(element):
                    descent_hist["element_absent"] += 1
                    continue
                if payload.node_data(element) is None:
                    descent_hist["comtblm_p_data_absent"] += 1
                    continue
                inner = payload.children(element)
                if not inner:
                    descent_hist["comtblm_child_absent"] += 1
                    continue
                comtbl = inner[0]
                if not payload.present(comtbl):
                    descent_hist["comtbl_node_absent"] += 1
                    continue
                descent_hist["reaches_comtbl"] += 1

                comtbl_data = payload.node_data(comtbl)
                if comtbl_data is None:
                    problems.append(f"ComTblBin {comtbl:#x} has no data word")
                    continue
                com_count = payload.u8(comtbl_data)
                comtbl_count_hist[com_count] += 1
                if com_count and com_count > len(payload.children(comtbl)):
                    problems.append(
                        f"ComTblBin {comtbl:#x} declares {com_count} coms, "
                        f"table holds {len(payload.children(comtbl))}")

    return {
        "obj_records": len(records),
        "maneuver_nodes": maneuver_nodes,
        "maneuver_elements": elements,
        "negative_counts": negative_counts,
        "maneuver_table_overruns": table_overruns,
        "inconsistencies": len(problems),
        "elements_per_maneuver": dict(sorted(count_hist.items())),
        "element_descent": dict(sorted(descent_hist.items())),
        "coms_per_comtbl": dict(sorted(comtbl_count_hist.items())),
        "problems": problems[:20],
    }


COUNT_READERS = {"u8": "u8", "u16": "u16", "s32": "s32"}


def validate_list_header(schema: dict, payload: Payload) -> dict:
    """Generic walk for any class the schema declares as a list header.

    Several classes differ only in where they sit and how wide their count is:
    SubMisTblBin uses a u8, RadioTblBin a u16, ManeuverBin an s32. Rather than
    writing the same walk again per class, the schema carries a `reach` path and
    a `list_header` block, and this function executes them. A class whose schema
    lacks either falls back to a dedicated validator.
    """
    problems: list[str] = []
    header = schema["list_header"]
    width = header["count_type"]
    if width not in COUNT_READERS:
        raise ValueError(f"unsupported count type {width!r}")
    stride = header["element_stride"]

    node = 0
    for step in schema.get("reach", []):
        if step["op"] == "root":
            node = 0
            continue
        if step["op"] == "child":
            children = payload.children(node)
            index = step["index"]
            if index >= len(children):
                return {"inconsistencies": 1, "problems":
                        [f"reach step child[{index}] out of range ({len(children)} children)"]}
            node = children[index]
            continue
        raise ValueError(f"unsupported reach op {step['op']!r}")

    data = payload.node_data(node)
    if data is None:
        return {"inconsistencies": 1, "problems": [f"list header {node:#x} has no data word"]}

    if width == "u8":
        count = payload.u8(data)
    elif width == "u16":
        count = struct.unpack_from(">H", payload.data, data)[0]
    else:
        count = payload.s32(data)

    children = payload.children(node)
    overrun = count > len(children)
    if overrun:
        problems.append(
            f"list header {node:#x} declares {count} elements, table holds {len(children)}")
    if count < 0:
        problems.append(f"list header {node:#x} declares a negative count {count}")

    present = 0
    absent = 0
    with_children: Counter[int] = Counter()
    for element in children[:max(count, 0)]:
        if payload.present(element):
            present += 1
            with_children[len(payload.children(element))] += 1
        else:
            # The reader raises its per-element fail-closed string here.
            absent += 1

    return {
        "header_node": f"{node:#x}",
        "count_type": width,
        "declared_count": count,
        "table_children": len(children),
        "table_overrun": bool(overrun),
        "elements_present": present,
        "elements_absent": absent,
        "element_stride": stride,
        "children_per_element": dict(sorted(with_children.items())),
        "inconsistencies": len(problems),
        "problems": problems[:20],
    }


VALIDATORS = {
    "ObjBin": validate_obj_bin,
    "OrderBin": validate_order_bin,
    "ActBin": validate_act_bin,
    "ManeuverBin": validate_maneuver_bin,
}


def resolve_validator(schema: dict):
    """Dedicated validator when one exists, otherwise the schema-driven walk."""
    dedicated = VALIDATORS.get(schema.get("class"))
    if dedicated is not None:
        return dedicated
    if "list_header" in schema and "reach" in schema:
        return validate_list_header
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("schema", type=Path)
    parser.add_argument("payload", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args(argv)

    schema = json.loads(arguments.schema.read_text())
    if schema.get("schema") != "ac6.scenario-schema.v1":
        print(f"unsupported schema {schema.get('schema')!r}", file=sys.stderr)
        return 2
    transitive = schema.get("validation", {}).get("validated_transitively_by")
    if transitive:
        # Not a gap: this class is exercised by another schema's walk, and
        # saying so is more useful than a generic "unsupported" error.
        print(f"{schema['class']} is validated transitively by {transitive}; "
              f"run analysis/scenario-schema/{transitive}.json instead")
        return 3

    validator = resolve_validator(schema)
    if validator is None:
        print(f"no validator for class {schema.get('class')!r}; the schema declares "
              "neither a dedicated walk nor a list_header with a reach path",
              file=sys.stderr)
        return 2

    raw = arguments.payload.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    expected = schema.get("validation", {}).get("payload_sha256")
    if expected and expected != digest:
        print(f"payload SHA-256 mismatch: expected {expected}, got {digest}", file=sys.stderr)
        return 2

    result = validator(schema, Payload(raw))
    result["payload_sha256"] = digest
    result["class"] = schema["class"]

    # Fail closed against the counters the schema itself records.
    recorded = schema.get("validation", {})
    scalar_keys = ("slot0_entries", "obj_records_reached", "inconsistencies",
                   "declared_count", "table_children", "elements_present",
                   "elements_absent",
                   "set_nodes", "act_nodes", "order_records_reached",
                   "acts_declaring_zero_orders", "set_count_overruns",
                   "act_count_overruns", "obj_records", "maneuver_nodes",
                   "maneuver_elements", "negative_counts",
                   "maneuver_table_overruns")
    mismatches = [
        key for key in scalar_keys
        if key in recorded and recorded[key] != result[key]
    ]
    for key in ("children_per_obj_node", "param_tag_distribution",
                "weapon_slots_filled", "maneuvers_per_obj",
                "tag_distribution", "children_per_order_node", "tag2_descent",
                "acts_per_set", "orders_per_act", "elements_per_maneuver",
                "element_descent", "coms_per_comtbl", "children_per_element"):
        if key not in recorded:
            continue
        if {str(k): v for k, v in result[key].items()} != recorded[key]:
            mismatches.append(key)
    result["recorded_mismatches"] = mismatches

    text = json.dumps(result, indent=2)
    if arguments.output:
        arguments.output.write_text(text + "\n")
    print(text)

    if result["inconsistencies"] or mismatches:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
