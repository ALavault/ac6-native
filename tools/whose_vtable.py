#!/usr/bin/env python3
"""Given a function address, name the classes whose vtable holds it.

Two of this file's expensive shapes -- *the displacement collision* and *the
right search, run against a sibling* -- both reduce to one question asked before
the search rather than after it: **whose is this?** A displacement means nothing
until you know which class has a field there, and a search's population is
whatever family you are actually looking at.

Answering it by hand takes a data scan, a walk backwards for the vtable start,
a read of the `RTTICompleteObjectLocator` at `vtable-4`, and a read of the type
descriptor's name. Done four times in one session, each time slightly
differently, and once wrongly -- cycle 1265 read a `.pdata` row as a vtable slot
and concluded two functions were "reached through tables rather than by call".

WHAT IT DOES, for each address:

  - scans the flat image for aligned words equal to it, EXCLUDING `.pdata`
    (0x82079E00..0x82089FB0), where a function's own exception record lives and
    is not a reference;
  - for each remaining hit, walks backwards up to `--span` words looking for a
    plausible `RTTICompleteObjectLocator` pointer at `candidate-4`: it must
    point into the image, its `+0x0C` must point into the image, and the bytes
    at that target `+8` must begin `.?AV`;
  - prints the class name and the slot offset.

The `.?AV` prefix is the check that makes this safe. Any word can look like a
pointer; only a real type descriptor spells a mangled class name at `+8`, and
that is why a hit with no name is reported as a hit with no name rather than
silently dropped -- a vtable without RTTI is a real thing here, and several of
the camera behaviour classes are exactly that.

usage: whose_vtable.py IMAGE ADDR [ADDR...] [--span N]
exit 0 always; this is a lookup, not a gate.
"""

import struct
import sys

BASE = 0x82000000
PDATA_START = 0x82079E00
PDATA_END = 0x82089FB0
DEFAULT_SPAN = 64  # words to walk back looking for the vtable start
CLASS_MAP = "analysis/class-map.tsv"


def load_class_map(path=CLASS_MAP):
    """The campaign's own audited vtable->class table, 811 vtables under J2.

    Prefer it to re-deriving: it was swept once, its 1,619 refusals were
    enumerated in class-map-rejects.tsv, and it is gated. Its addresses are
    lowercase hex, which cost a grep before this was noticed.
    """
    table = {}
    try:
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("#"):
                    continue
                fields = line.rstrip("\n").split("\t")
                if len(fields) < 2 or not fields[0].startswith("0x"):
                    continue
                table[int(fields[0], 16)] = fields[1]
    except OSError:
        return {}
    return table


def image_word(data, address):
    offset = address - BASE
    if offset < 0 or offset + 4 > len(data):
        return None
    return struct.unpack_from(">I", data, offset)[0]


def rtti_name(data, vtable):
    """Read the class name from vtable-4, or None."""
    locator = image_word(data, vtable - 4)
    if locator is None or not (BASE <= locator < BASE + len(data)):
        return None
    descriptor = image_word(data, locator + 0x0C)
    if descriptor is None or not (BASE <= descriptor < BASE + len(data)):
        return None
    offset = descriptor + 8 - BASE
    if offset < 0 or offset + 4 > len(data):
        return None
    if data[offset:offset + 4] != b".?AV":
        return None
    end = data.find(b"\0", offset)
    if end == -1:
        return None
    return data[offset:end].decode("ascii", "replace")


def main() -> int:
    arguments = sys.argv[1:]
    span = DEFAULT_SPAN
    if "--span" in arguments:
        index = arguments.index("--span")
        span = int(arguments[index + 1])
        arguments = arguments[:index] + arguments[index + 2:]
    if len(arguments) < 2:
        print("usage: whose_vtable.py IMAGE ADDR [ADDR...] [--span N]")
        return 1

    try:
        with open(arguments[0], "rb") as handle:
            data = handle.read()
    except OSError as exc:
        print("  UNREADABLE  %s  %s" % (arguments[0], exc))
        return 1

    mapped = load_class_map()
    print("class map: %d vtables" % len(mapped) if mapped else
          "class map: NOT LOADED -- names come from the RTTI walk alone")

    for argument in arguments[1:]:
        target = int(argument, 16)
        hits = []
        for offset in range(0, len(data) - 3, 4):
            if struct.unpack_from(">I", data, offset)[0] != target:
                continue
            address = BASE + offset
            if PDATA_START <= address < PDATA_END:
                continue  # its own exception record, not a reference
            hits.append(address)

        resolved = []
        unresolved = []
        for hit in hits:
            named = None
            for back in range(0, span + 1):
                candidate = hit - 4 * back
                if candidate in mapped:
                    named = (candidate, back, mapped[candidate] + "  [class-map]")
                    break
                name = rtti_name(data, candidate)
                if name is not None:
                    named = (candidate, back, name + "  [RTTI]")
                    break
            if named is None:
                unresolved.append(hit)
            else:
                resolved.append((hit, named))

        # Named first: a reader should not have to grep the output of a tool
        # written to save them a search. 0x82266390 has 232 hits and 145 names.
        print("0x%08X appears in %d aligned word(s) outside .pdata: "
              "%d named, %d unnamed" %
              (target, len(hits), len(resolved), len(unresolved)))
        for hit, (vtable, back, name) in resolved:
            print("    at 0x%08X   vtable 0x%08X slot +0x%02X   %s" %
                  (hit, vtable, back * 4, name))
        if unresolved:
            print("    %d hit(s) with NO NAME within %d words -- neither the "
                  "class map nor an RTTI locator; several real vtables here have "
                  "none (0x820078D0 holds 0 at -4, 0x82009440 a function "
                  "address). First few: %s" %
                  (len(unresolved), span,
                   ", ".join("0x%08X" % h for h in unresolved[:4])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
