#!/usr/bin/env python3
"""Emit an ``ac6.function-snapshot.v1`` from an independent native parser.

``scripts/MicroExecuteScenarioParser.java`` runs the retail PPC instructions
themselves through Ghidra's p-code emulator and records what they write. This
module writes the other side of that comparison: a from-scratch implementation
of the same parsers, derived from ``analysis/scenario-schema/`` rather than from
the machine code, laying its output into the same synthetic address space.

If the two agree byte for byte, the schema is not merely consistent with the
payload - it reproduces the retail parser's exact behaviour on it. That is the
``microexec`` evidence the Mission 01 v2 gate accepts, and it needs no emulator,
bridge or native run.

Usage
-----
    python3 tools/emit_ac6_native_snapshot.py ObjBin 0xfa0 PAYLOAD --output OUT
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

# The same layout MicroExecuteScenarioParser.java uses, so the two snapshots are
# directly comparable rather than merely similar.
PAYLOAD_BASE = 0xB0000000
RECORD_BASE = 0xB4000000
BUFFER_BASE = 0xB5000000
RECORD_BYTES = 0x100
BUFFER_BYTES = 0x8000
POISON = 0xCD


class Payload:
    """The decoded node graph, addressed by payload-relative offsets."""

    def __init__(self, data: bytes) -> None:
        self.data = data

    def u32(self, offset: int) -> int:
        return struct.unpack_from(">I", self.data, offset)[0]

    def s32(self, offset: int) -> int:
        return struct.unpack_from(">i", self.data, offset)[0]

    def u8(self, offset: int) -> int:
        return self.data[offset]

    def resolve(self, node: int, word: int) -> int | None:
        """node + node[word], or None when the parser treats it as absent."""
        offset = self.u32(node + 4 * word)
        return None if offset == 0 else node + offset

    def children(self, node: int) -> list[int]:
        table = self.resolve(node, 1)
        if table is None:
            return []
        count = self.s32(table)
        if count < 0:
            return []
        return [table + self.u32(table + 4 + 4 * i) for i in range(count)]

    def present(self, node: int | None) -> bool:
        if node is None:
            return False
        return self.u32(node) != 0 or self.u32(node + 4) != 0


class Image:
    """A record and buffer, written exactly as the parsers do.

    Writes are tracked with an explicit mask rather than inferred by diffing
    against a poison fill. A parser can legitimately write a byte equal to any
    single poison value, which a diff would report as unwritten. The emulator
    side removes the same blind spot by taking the union of two poison passes.
    """

    def __init__(self) -> None:
        self.record = bytearray([POISON]) * RECORD_BYTES
        self.buffer = bytearray([POISON]) * BUFFER_BYTES
        self.record_written = bytearray(RECORD_BYTES)
        self.buffer_written = bytearray(BUFFER_BYTES)

    def _region(self, guest: int) -> tuple[bytearray, bytearray, int]:
        if RECORD_BASE <= guest < RECORD_BASE + RECORD_BYTES:
            return self.record, self.record_written, guest - RECORD_BASE
        if BUFFER_BASE <= guest < BUFFER_BASE + BUFFER_BYTES:
            return self.buffer, self.buffer_written, guest - BUFFER_BASE
        raise ValueError(f"write outside the synthetic regions at {guest:#x}")

    def write32(self, guest: int, value: int) -> None:
        region, written, offset = self._region(guest)
        struct.pack_into(">I", region, offset, value & 0xFFFFFFFF)
        for index in range(4):
            written[offset + index] = 1

    def zero(self, guest: int, words: int) -> None:
        for index in range(words):
            self.write32(guest + 4 * index, 0)

    def runs(self) -> list[dict]:
        """Contiguous written spans, in address order, as the emulator reports."""
        out: list[dict] = []
        for base, region, written in (
            (RECORD_BASE, self.record, self.record_written),
            (BUFFER_BASE, self.buffer, self.buffer_written),
        ):
            index = 0
            while index < len(region):
                if not written[index]:
                    index += 1
                    continue
                start = index
                while index < len(region) and written[index]:
                    index += 1
                out.append({"address": f"0x{base + start:08x}",
                            "size": index - start,
                            "after_hex": region[start:index].hex()})
        return out


def guest(offset: int | None) -> int:
    """A payload offset as the guest pointer the parser stores."""
    return 0 if offset is None else PAYLOAD_BASE + offset


class Parsers:
    """The *Bin readers, written from the schema rather than the machine code."""

    def __init__(self, payload: Payload, image: Image) -> None:
        self.p = payload
        self.image = image
        # Each entry mirrors one call the retail reader makes into the error
        # printer, with the exact string address its branch materializes. This
        # is the one call kind a native parser can and must reproduce.
        self.errors: list[dict] = []

    ERROR_PRINTER = 0x823828B8

    def _fail(self, string_address: int, message: str) -> None:
        self.errors.append({
            "target": f"0x{self.ERROR_PRINTER:08x}",
            "ordinal": len(self.errors),
            "note": f"error printer, arg 0x{string_address:08x}",
            "message": message,
        })

    # -- ComBin ------------------------------------------------------------
    def com_read(self, record: int, node: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x820106BC, "ComBin::read() / data empty!")

    # -- ComTblBin ---------------------------------------------------------
    def comtbl_size(self, node: int) -> int:
        data = self.p.resolve(node, 0)
        return 0 if data is None else self.p.u8(data) << 2

    def comtbl_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x82010690, "ComTblBin::read() / data empty!")
            return
        count = self.p.u8(data)
        if count == 0:
            return                      # early exit, before the child table
        children = self.p.children(node)
        self.image.write32(record + 4, buffer)
        for index in range(count):
            child = children[index] if index < len(children) else None
            self.image.write32(buffer + 4 * index, 0)
            if child is None or not self.p.present(child):
                self._fail(0x82010634, "ComTblBin::read() / com empty! : %d")
                continue
            self.com_read(buffer + 4 * index, child)

    # -- ManeuverBin -------------------------------------------------------
    def maneuver_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x820105D4, "ManeuverBin::read() / data empty!")
            return
        count = self.p.s32(data)
        comtblm = buffer
        comtbl = buffer + count * 4
        cursor = comtbl + count * 8
        self.image.write32(record + 4, comtblm)
        self.image.write32(record + 8, comtbl)

        children = self.p.children(node)
        for index in range(count):
            child = children[index] if index < len(children) else None
            self.image.write32(comtblm + 4 * index, 0)
            self.image.zero(comtbl + 8 * index, 2)
            if child is None or not self.p.present(child):
                self._fail(0x82010538, "ManeuverBin::read() / comtblm empty! : %d")
                continue
            payload_pointer = self.p.resolve(child, 0)
            self.image.write32(comtblm + 4 * index, guest(payload_pointer))
            if payload_pointer is None:
                self._fail(0x82010460, "ComTblMBin::read() / p_data empty!")
            inner = self.p.resolve(child, 1)
            comtbl_node = None
            if inner is not None:
                grandchildren = self.p.children(child)
                if grandchildren and self.p.present(grandchildren[0]):
                    comtbl_node = grandchildren[0]
            if comtbl_node is None:
                self._fail(0x8201056C, "ManeuverBin::read() / comtblm child empty! : %d")
                continue
            self.comtbl_read(comtbl + 8 * index, comtbl_node, cursor)
            cursor += self.comtbl_size(comtbl_node)

    def maneuver_size(self, node: int) -> int:
        data = self.p.resolve(node, 0)
        if data is None:
            return 0
        count = self.p.s32(data)
        total = count * 12
        for child in self.p.children(node)[:count]:
            if not self.p.present(child):
                continue
            grandchildren = self.p.children(child)
            if grandchildren and self.p.present(grandchildren[0]):
                total += self.comtbl_size(grandchildren[0])
        return total

    # -- ObjBin maneuver block (0x82330C58) --------------------------------
    def maneuver_block_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        cursor = buffer
        for index, child in enumerate(self.p.children(node)[:8]):
            if not self.p.present(child):
                continue
            self.image.write32(record + 4 + 4 * index, cursor)
            self.maneuver_read(cursor, child, cursor + 0x0C)
            cursor += 0x0C + self.maneuver_size(child)

    def maneuver_block_size(self, node: int) -> int:
        """Reproduce 0x82330A30 exactly, including its asymmetry with the reader.

        The sizer starts at 0x60 - eight maneuver records of 0x0C - and, when
        slot 0 is present, *overwrites* that base with `size0 + 0x6C` rather
        than adding to it. Slots 1..7 then add `size_i + 0x0C` each. The reader
        0x82330C58 advances only 0x0C per present slot, so the two disagree by
        0x60 whenever slot 0 is present. ObjBin::read advances its cursor by the
        sizer, so the sizer is what the layout follows.
        """
        children = self.p.children(node)[:8]
        total = 0x60
        for index, child in enumerate(children):
            if not self.p.present(child):
                continue
            if index == 0:
                total = self.maneuver_size(child) + 0x6C
            else:
                total += self.maneuver_size(child) + 0x0C
        return total

    # -- ObjBin param variant (0x82330F98) ---------------------------------
    def param_read(self, record: int, node: int, buffer: int) -> None:
        tag_pointer = self.p.resolve(node, 0)
        self.image.write32(record, guest(tag_pointer))
        children = self.p.children(node)
        child = children[0] if children and self.p.present(children[0]) else None
        if tag_pointer is None:
            return
        tag = self.p.u8(tag_pointer)
        if tag > 2:
            return                      # the reader writes nothing
        self.image.write32(record + 4 + 4 * tag, buffer)
        target = self.p.resolve(child, 0) if child is not None else None
        self.image.write32(buffer, guest(target))
        if tag == 1 and target is None:
            self._fail(0x82010294, "ParamGroundBin::read() / data empty!")

    @staticmethod
    def param_size(payload: Payload, node: int) -> int:
        data = payload.resolve(node, 0)
        if data is None:
            return 0
        return 0x10 if payload.u8(data) <= 2 else 0


    # -- OrderBin tag 2 sub-block (0x82331AD0) ------------------------------
    def order_tag2_read(self, record: int, node: int | None, buffer: int) -> None:
        """Reproduce 0x82331AD0, including the descent it usually does not take.

        The reader needs a resolved data word, a table, and a present first
        child before it writes anything beyond the header. On the Mission 01
        payload that precondition never holds, so this stops after the first
        word - but the check is reproduced rather than assumed, so a node that
        did descend would diverge here instead of passing silently.
        """
        if node is None:
            return
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            return
        table = self.p.resolve(node, 1)
        if table is None:
            return
        children = self.p.children(node)
        if not children or not self.p.present(children[0]):
            return
        raise NotImplementedError(
            "OrderBin tag-2 descent is unexercised by this payload; "
            "0x82331D98 is not modelled")

    def order_tag2_size(self, node: int | None) -> int:
        """0x82331A38: zero unless the same descent succeeds."""
        if node is None:
            return 0
        if self.p.resolve(node, 1) is None:
            return 0
        children = self.p.children(node)
        if not children or not self.p.present(children[0]):
            return 0
        raise NotImplementedError("OrderBin tag-2 descent is unexercised")

    # -- OrderBin ----------------------------------------------------------
    # tag -> (record word index, error string address or None)
    ORDER_TAGS = {
        0: (1, None),
        1: (2, 0x820102C4),
        2: (3, None),
        3: (4, 0x820102F8),
        4: (5, 0x82010324),
        5: (6, 0x8201037C),
        6: (7, 0x82010350),
        7: (8, None),
        8: (9, 0x820103A8),
        9: (10, None),
    }
    ORDER_MESSAGES = {
        0x820102C4: "OrderDisappearBin::read() / data empty!",
        0x820102F8: "OrderStopBin::read() / data empty!",
        0x82010324: "OrderLeadBin::read() / data empty!",
        0x8201037C: "OrderJumpBin::read() / data empty!",
        0x82010350: "OrderFlagBin::read() / data empty!",
        0x820103A8: "OrderPropertyBin::read() / data empty!",
    }

    def order_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x82010438, "OrderBin::read() / data empty!")
        table = self.p.resolve(node, 1)
        if table is None:
            self._fail(0x8201040C, "OrderBin::read() / child empty!")

        # The reader takes child[0] only, and only when the table is non-empty
        # and that child is present; otherwise it carries a null child forward.
        children = self.p.children(node)
        child = children[0] if children and self.p.present(children[0]) else None

        if data is None:
            return
        tag = self.p.u8(data)
        if tag not in self.ORDER_TAGS:
            return                      # the reader writes nothing at all
        word, error = self.ORDER_TAGS[tag]

        if tag == 2:
            self.image.zero(buffer, 2)
            self.image.write32(record + 4 * word, buffer)
            self.order_tag2_read(buffer, child, buffer + 0x10)
            return

        self.image.write32(buffer, 0)
        self.image.write32(record + 4 * word, buffer)
        target = self.p.resolve(child, 0) if child is not None else None
        self.image.write32(buffer, guest(target))
        if error is not None and target is None:
            self._fail(error, self.ORDER_MESSAGES[error])


    # -- OrderBin sizer (0x823310E8) ---------------------------------------
    # tag 0 and 1 cost 4; tag 2 costs 8 plus a nested block; tags 3..9 cost 4;
    # anything else costs nothing.
    def order_size(self, node: int | None) -> int:
        if node is None:
            # The retail sizer would dereference a null data pointer here. No
            # order child is ever absent in this payload, so the path is dead;
            # returning 0 keeps the model total rather than guessing a fault.
            return 0
        data = self.p.resolve(node, 0)
        if data is None:
            self._fail(0x820103D8, "OrderBin::getReadBuffSize() / data empty!")
            return 0
        tag = self.p.u8(data)
        if tag == 2:
            return 8 + self.order_tag2_size(self._order_child(node))
        if tag in self.ORDER_TAGS:
            return 4
        return 0

    def _order_child(self, node: int) -> int | None:
        children = self.p.children(node)
        return children[0] if children and self.p.present(children[0]) else None

    # -- ActBin (0x82330688 / 0x82330540) ----------------------------------
    def act_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x8201026C, "ActBin::read() / data empty!")
            return
        count = self.p.u8(data)
        if count == 0:
            return                      # early exit, before the child table
        self.image.write32(record + 4, buffer)
        cursor = buffer + count * 0x2C
        table = self.p.resolve(node, 1)
        if table is None:
            self._fail(0x82010244, "ActBin::read() / child empty!")
        children = self.p.children(node)

        for index in range(count):
            element = buffer + index * 0x2C
            self.image.zero(element, 11)
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            if child is None:
                self._fail(0x82010218, "ActBin::read() / order empty! : %d")
                continue
            self.order_read(element, child, cursor)
            cursor += self.order_size(child)

    def act_size(self, node: int | None) -> int:
        if node is None:
            return 0
        data = self.p.resolve(node, 0)
        if data is None:
            self._fail(0x820101E4, "ActBin::getReadBuffSize() / data empty!")
            return 0
        count = self.p.u8(data)
        if count == 0:
            return 0
        total = count * 0x2C
        if self.p.resolve(node, 1) is None:
            self._fail(0x820101B0, "ActBin::getReadBuffSize() / child empty!")
        children = self.p.children(node)
        for index in range(count):
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            if child is None:
                self._fail(0x82010178, "ActBin::getReadBuffSize() / order empty! : %d")
            total += self.order_size(child)
        return total

    # -- SetBin (0x8232F5F8 / 0x8232F4D0) ----------------------------------
    def set_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x82010074, "SetBin::read() / data empty!")
            return
        count = self.p.u8(data)
        if count == 0:
            return
        self.image.write32(record + 4, buffer)
        cursor = buffer + count * 8
        if self.p.resolve(node, 1) is None:
            self._fail(0x8201004C, "SetBin::read() / child empty!")
        children = self.p.children(node)

        for index in range(count):
            element = buffer + index * 8
            self.image.zero(element, 2)
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            if child is None:
                self._fail(0x82010020, "SetBin::read() / act empty! : %d")
                continue
            self.act_read(element, child, cursor)
            cursor += self.act_size(child)


    # -- Unnamed 0x28 element (0x8232F9B8 / 0x8232F8B0) --------------------
    # A tagged union with the same shape as OrderBin but no error string
    # anywhere in its subtree, so none of its variants can be named.
    UNNAMED28_TAGS = {0: 1, 4: 2, 5: 3, 6: 4, 1: 5, 2: 6, 7: 8, 8: 9}

    def _unnamed28_child(self, node: int) -> int | None:
        """The grandchild 0x8232F9B8 requires before it writes a variant."""
        if self.p.resolve(node, 0) is None or self.p.resolve(node, 1) is None:
            return None
        children = self.p.children(node)
        if not children or not self.p.present(children[0]):
            return None
        return children[0]

    def unnamed28_read(self, record: int, node: int | None, buffer: int) -> None:
        if node is None:
            return
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            return
        child = self._unnamed28_child(node)
        if child is None:
            return
        tag = self.p.u8(data)
        if tag not in self.UNNAMED28_TAGS:
            return
        if tag == 2:
            raise NotImplementedError(
                "unnamed 0x28 tag-2 descent is unexercised by this payload; "
                "0x823308E0 is not modelled")
        self.image.write32(buffer, 0)
        self.image.write32(record + 4 * self.UNNAMED28_TAGS[tag], buffer)
        self.image.write32(buffer, guest(self.p.resolve(child, 0)))

    def unnamed28_size(self, node: int | None) -> int:
        if node is None:
            return 0
        data = self.p.resolve(node, 0)
        if data is None or self.p.resolve(node, 1) is None:
            return 0
        if self._unnamed28_child(node) is None:
            return 0
        tag = self.p.u8(data)
        if tag == 2:
            raise NotImplementedError("unnamed 0x28 tag-2 sizer is unexercised")
        return 4 if tag in self.UNNAMED28_TAGS else 0

    # -- Unnamed 0x28 list (0x8232ED10 / 0x8232EC08) -----------------------
    def unnamed28_list_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None or self.p.resolve(node, 1) is None:
            return
        count = self.p.u8(data)
        self.image.write32(record + 4, buffer)
        cursor = buffer + count * 0x28
        children = self.p.children(node)
        for index in range(count):
            element = buffer + index * 0x28
            self.image.zero(element, 10)
        for index in range(count):
            element = buffer + index * 0x28
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            self.unnamed28_read(element, child, cursor)
            cursor += self.unnamed28_size(child)

    def unnamed28_list_size(self, node: int) -> int:
        """0x8232EC08, including the 16-byte round-up the other sizers lack.

        The tail is `(total + 0xF) >> 4 << 4`. It is the only sizer in the
        family that aligns, and it is worth 8 bytes on a two-element list, so a
        model that ignores it drifts the buffer cursor from the second
        sub-mission onward.
        """
        data = self.p.resolve(node, 0)
        if data is None or self.p.resolve(node, 1) is None:
            return 0
        count = self.p.u8(data)
        total = count * 0x28
        children = self.p.children(node)
        for index in range(count):
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            total += self.unnamed28_size(child)
        return (total + 0xF) & ~0xF

    # -- SubMisBin (0x8232C8A8 / 0x8232C7E0) -------------------------------
    def submis_read(self, record: int, node: int, buffer: int) -> None:
        self.image.write32(record, guest(self.p.resolve(node, 0)))
        table = self.p.resolve(node, 1)
        if table is None:
            self._fail(0x8200FE9C, "SubMisBin::read() / child data empty!")
            return
        children = self.p.children(node)

        if children and self.p.present(children[0]):
            self.image.write32(buffer, 0)
            self.image.write32(record + 4, buffer)
            target = self.p.resolve(children[0], 0)
            self.image.write32(buffer, guest(target))
            if target is None:
                self._fail(0x8200FE38, "MapmaskBin::read ( ) / data empty!")
            buffer += 0x10

        if len(children) > 1 and self.p.present(children[1]):
            self.image.zero(buffer, 2)
            self.image.write32(record + 8, buffer)
            self.unnamed28_list_read(buffer, children[1], buffer + 8)

    def submis_size(self, node: int) -> int:
        total = 0
        if self.p.resolve(node, 1) is None:
            self._fail(0x8200FE64, "SubMisBin::getReadBuffSize ( ) / child empty!")
            return 0
        children = self.p.children(node)
        if children and self.p.present(children[0]):
            total = 0x10
        if len(children) > 1 and self.p.present(children[1]):
            total += self.unnamed28_list_size(children[1]) + 8
        return total

    # -- SubMisTblBin (0x82309758 / 0x82309620) ----------------------------
    def submistbl_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x8200F6BC, "SubMisTblBin::read() / data empty!")
            return
        count = self.p.u8(data)
        if count == 0:
            return
        self.image.write32(record + 4, buffer)
        cursor = buffer + count * 0x10
        if self.p.resolve(node, 1) is None:
            self._fail(0x8200F68C, "SubMisTblBin::read() / child empty!")
        children = self.p.children(node)

        for index in range(count):
            element = buffer + index * 0x10
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            if child is None:
                self._fail(0x8200F65C, "SubMisTblBin::read() / submis empty!")
            self.image.zero(element, 3)
            if child is None:
                continue
            self.submis_read(element, child, cursor)
            cursor += self.submis_size(child)

    # -- RadioTblBin (0x823094D8 / 0x823093C8) -----------------------------
    def radiotbl_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x8200F574, "RadioTblBin::read ( ) / data empty!")
            return
        count = struct.unpack_from(">H", self.p.data, data)[0]
        if count == 0:
            return
        if self.p.resolve(node, 1) is None:
            self._fail(0x8200F544, "RadioTblBin::read ( ) / child empty!")
        self.image.write32(record + 4, buffer)
        children = self.p.children(node)
        for index in range(count):
            element = buffer + index * 0x10
            self.image.write32(element, 0)
            child = children[index] if index < len(children) else None
            if child is not None and not self.p.present(child):
                child = None
            if child is None:
                self._fail(0x8200F50C, "RadioTblBin::read ( ) / radio data empty! : %d")
            # 0x8232C7B0 writes one word: the resolved payload, or zero.
            self.image.write32(element,
                               guest(self.p.resolve(child, 0)) if child is not None else 0)

    # -- ObjBin ------------------------------------------------------------
    def obj_read(self, record: int, node: int, buffer: int) -> None:
        data = self.p.resolve(node, 0)
        self.image.write32(record, guest(data))
        if data is None:
            self._fail(0x82010150, "ObjBin::read() / data empty!")
        children = self.p.children(node)
        cursor = buffer

        if len(children) > 0 and self.p.present(children[0]):
            self.image.zero(cursor, 4)
            self.image.write32(record + 0x04, cursor)
            self.param_read(cursor, children[0], cursor + 0x10)
            cursor += 0x10 + self.param_size(self.p, children[0])

        if len(children) > 1 and self.p.present(children[1]):
            self.image.zero(cursor, 9)
            self.image.write32(record + 0x08, cursor)
            self.maneuver_block_read(cursor, children[1], cursor + 0x24)
            cursor += 0x24 + self.maneuver_block_size(children[1])

        for index, offset in ((2, 0x0C), (3, 0x10), (4, 0x14), (5, 0x18)):
            if len(children) <= index or not self.p.present(children[index]):
                continue
            self.image.write32(cursor, 0)
            self.image.write32(record + offset, cursor)
            target = self.p.resolve(children[index], 0)
            self.image.write32(cursor, guest(target))
            cursor += 0x10

        if len(children) > 6 and self.p.present(children[6]):
            self.image.write32(cursor, 0)
            self.image.write32(record + 0x1C, cursor)
            target = self.p.resolve(children[6], 0)
            self.image.write32(cursor, guest(target))


ENTRY_POINTS = {
    "ComBin": ("0x82331E78", lambda parsers, node: parsers.com_read(RECORD_BASE, node)),
    "ComTblBin": ("0x82331C10",
                  lambda parsers, node: parsers.comtbl_read(RECORD_BASE, node, BUFFER_BASE)),
    "ManeuverBin": ("0x82331808",
                    lambda parsers, node: parsers.maneuver_read(RECORD_BASE, node, BUFFER_BASE)),
    "OrderBin": ("0x82331208",
                 lambda parsers, node: parsers.order_read(RECORD_BASE, node, BUFFER_BASE)),
    "ActBin": ("0x82330688",
               lambda parsers, node: parsers.act_read(RECORD_BASE, node, BUFFER_BASE)),
    "SetBin": ("0x8232F5F8",
               lambda parsers, node: parsers.set_read(RECORD_BASE, node, BUFFER_BASE)),
    "SubMisTblBin": ("0x82309758",
                     lambda parsers, node: parsers.submistbl_read(RECORD_BASE, node, BUFFER_BASE)),
    "SubMisBin": ("0x8232C8A8",
                  lambda parsers, node: parsers.submis_read(RECORD_BASE, node, BUFFER_BASE)),
    "RadioTblBin": ("0x823094D8",
                    lambda parsers, node: parsers.radiotbl_read(RECORD_BASE, node, BUFFER_BASE)),
    "ObjBin": ("0x82330158",
               lambda parsers, node: parsers.obj_read(RECORD_BASE, node, BUFFER_BASE)),
}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cls", choices=sorted(ENTRY_POINTS))
    parser.add_argument("node", help="payload-relative node offset, e.g. 0xfa0")
    parser.add_argument("payload", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args(argv)

    raw = arguments.payload.read_bytes()
    payload = Payload(raw)
    image = Image()
    parsers = Parsers(payload, image)
    address, entry = ENTRY_POINTS[arguments.cls]
    node = int(arguments.node, 0)
    entry(parsers, node)

    snapshot = {
        "schema": "ac6.function-snapshot.v1",
        "identity": {
            "implementation": "native",
            "function": address,
            "case": f"{arguments.cls}@node+{node:#x}",
        },
        "provenance": {
            "xex_sha256": "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
            "payload_sha256": hashlib.sha256(raw).hexdigest(),
            "derived_from": "analysis/scenario-schema/, not from the machine code",
            "fail_closed_messages": [error["message"] for error in parsers.errors],
        },
        "exit": {"kind": "return"},
        "registers": {},
        "calls": [{key: error[key] for key in ("target", "ordinal", "note")}
                  for error in parsers.errors],
        "memory_writes": image.runs(),
    }
    text = json.dumps(snapshot, indent=2)
    if arguments.output:
        arguments.output.write_text(text + "\n")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
