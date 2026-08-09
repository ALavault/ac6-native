#!/usr/bin/env python3
"""Given a constructor address, print the object's subobject map.

WHY THIS EXISTS. Cycle 1370 reached the flight position integrator's owner by
hand: trace `this` through a constructor, watch every `stw` of a materialised
constant, read the `RTTICompleteObjectLocator` at each installed vtable minus
four, and note every `bl` whose `r3` is `this + k`. That is four separate
mistakes waiting to happen, and one of them was made on the first pass -- the
throwaway version rejected every type descriptor in the image because it guarded
`descriptor < 0x82400000` while AC6's descriptors live at `0x8268F...`. The
campaign's own `whose_vtable.py` had the bound right (`BASE .. BASE + len`), so
the answer was "the instrument was correct and my copy of it was not".

WHAT IT DOES. It reads the recompiled corpus (comment lines carry the original
PowerPC mnemonics), seeds `r3 = this + 0`, and propagates two disjoint lattices:

  - CONSTANTS, built by `lis` / `addi` / `mr`. A `lis rD,SIMM` is `SIMM << 16`
    and `addi` chains onto it. This is the only way to see a materialised
    address: `find_materialised_address.py` exists because a `lis`+`addi` pair
    is invisible to a substring scan of the image.
  - THIS-RELATIVE POINTERS, built by `addi rD,rThis,k` / `mr`. A register is in
    exactly one lattice; assigning from a constant clears the relative binding
    and the reverse, because a value cannot be both.

A `stw rSrc,off(rBase)` where `rSrc` is a constant and `rBase` is this-relative
is a **pointer install at `this + rel(base) + off`**, and if that constant has a
COL at minus four it is a vtable and the class has a name. A `bl` taken while
`r3` is this-relative is a **subobject constructor call at that offset**.

WHAT IT REFUSES TO DO. It does not follow branches, so a constructor with real
control flow is reported for the fall-through path only; the `--warn-branches`
count says how much was skipped rather than leaving that silent. It does not
claim a size -- a constructor does not carry one, the allocation site does.

AND ONE FALSE FRIEND IT DOES NOT FILTER. A displacement and a string address are
both integers. `sub_821F5AD8` contains `addi r10,r11,2224` and that 2224 is the
low half of `0x820008B0`, a string, not the `this + 2224` this tool reports for
the same number in `sub_8222BEC8`. The difference is which lattice the base
register is in, which is exactly why the two are kept apart here.

usage: map_object_layout.py CORPUS_DIR IMAGE CTOR_ADDR [CTOR_ADDR...] [--tsv]
exit 0 always; this is a lookup, not a gate.
"""

from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

BASE = 0x82000000

_IMPL = re.compile(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-Fa-f]{8})\) \{")
_INSN = re.compile(r"^\t// (\S+)(?:\s+(.*))?$")
_LIS = re.compile(r"^r(\d+),(-?\d+)$")
_ADDI = re.compile(r"^r(\d+),r(\d+),(-?\d+)$")
_MR = re.compile(r"^r(\d+),r(\d+)$")
_STW = re.compile(r"^r(\d+),(-?\d+)\((r\d+)\)$")
_BRANCH = {"b", "beq", "bne", "bge", "blt", "bgt", "ble", "bdnz", "bctr", "bctrl"}


def load_corpus(directory):
    """address -> [(address, mnemonic, operands)], from the generated comments."""
    functions = {}
    for path in sorted(Path(directory).glob("*.cpp")):
        text = path.read_text(errors="replace")
        for match in _IMPL.finditer(text):
            start = int(match.group(1), 16)
            end = text.find("\n}\n", match.end())
            if end < 0:
                end = len(text)
            body = []
            for line in text[match.end():end].splitlines():
                parsed = _INSN.match(line)
                if parsed:
                    operands = (parsed.group(2) or "").replace(" ", "")
                    body.append((start + 4 * len(body), parsed.group(1), operands))
            functions[start] = body
    return functions


def image_word(data, address):
    offset = address - BASE
    if offset < 0 or offset + 4 > len(data):
        return None
    return struct.unpack_from(">I", data, offset)[0]


def rtti_name(data, vtable):
    """The class name at `vtable - 4`, or None.

    The bound is the whole image, not an assumed end of `.rdata`: AC6 keeps its
    type descriptors around 0x8268F000, far past every section this campaign
    normally reads, and a tighter guard silently renames every class to None.
    """
    locator = image_word(data, vtable - 4)
    if locator is None or not (BASE <= locator < BASE + len(data)):
        return None
    descriptor = image_word(data, locator + 0x0C)
    if descriptor is None or not (BASE <= descriptor < BASE + len(data)):
        return None
    offset = descriptor + 8 - BASE
    if offset + 4 > len(data) or data[offset:offset + 4] != b".?AV":
        return None
    end = data.find(b"\0", offset)
    return None if end < 0 else data[offset:end].decode("ascii", "replace")


def map_layout(functions, data, address):
    """Return (installs, calls, branches_skipped) for one constructor."""
    body = functions.get(address)
    if body is None:
        return None
    constant = {}
    relative = {"r3": 0}
    installs = []
    calls = []
    branches = 0

    def bind(destination, const_value, rel_value):
        constant.pop(destination, None)
        relative.pop(destination, None)
        if const_value is not None:
            constant[destination] = const_value
        elif rel_value is not None:
            relative[destination] = rel_value

    for at, mnemonic, operands in body:
        if mnemonic == "lis":
            parsed = _LIS.match(operands)
            if parsed:
                bind("r" + parsed.group(1),
                     (int(parsed.group(2)) & 0xFFFF) << 16, None)
        elif mnemonic == "addi":
            parsed = _ADDI.match(operands)
            if parsed:
                destination = "r" + parsed.group(1)
                source = "r" + parsed.group(2)
                immediate = int(parsed.group(3))
                if source in constant:
                    bind(destination, (constant[source] + immediate) & 0xFFFFFFFF, None)
                elif source in relative:
                    bind(destination, None, relative[source] + immediate)
                else:
                    bind(destination, None, None)
        elif mnemonic == "mr":
            parsed = _MR.match(operands)
            if parsed:
                destination = "r" + parsed.group(1)
                source = "r" + parsed.group(2)
                bind(destination, constant.get(source), relative.get(source))
        elif mnemonic == "stw":
            parsed = _STW.match(operands)
            if parsed:
                source = "r" + parsed.group(1)
                offset = int(parsed.group(2))
                base = parsed.group(3)
                if source in constant and base in relative:
                    value = constant[source]
                    if BASE <= value < BASE + len(data):
                        installs.append((at, relative[base] + offset, value,
                                         rtti_name(data, value)))
        elif mnemonic == "bl":
            if operands.startswith("0x") and "r3" in relative:
                calls.append((at, relative["r3"], int(operands, 16)))
        elif mnemonic in _BRANCH:
            branches += 1
    return installs, calls, branches


def main() -> int:
    arguments = sys.argv[1:]
    as_tsv = "--tsv" in arguments
    arguments = [item for item in arguments if item != "--tsv"]
    if len(arguments) < 3:
        print("usage: map_object_layout.py CORPUS_DIR IMAGE CTOR_ADDR [CTOR_ADDR...]"
              " [--tsv]")
        return 1
    functions = load_corpus(arguments[0])
    data = Path(arguments[1]).read_bytes()
    if as_tsv:
        print("ctor\tkind\tsite\tthis_offset\tvalue\tname")
    for argument in arguments[2:]:
        address = int(argument, 16)
        result = map_layout(functions, data, address)
        if result is None:
            print("  NOT IN CORPUS  %s" % argument)
            continue
        installs, calls, branches = result
        if not as_tsv:
            print("=== sub_%08X === (%d branch(es) not followed)"
                  % (address, branches))
        for at, offset, value, name in installs:
            if as_tsv:
                print("0x%08X\tinstall\t0x%08X\t%d\t0x%08X\t%s"
                      % (address, at, offset, value, name or ""))
            else:
                print("  install  0x%08X  this+%-6d <- 0x%08X  %s"
                      % (at, offset, value, name or "(no RTTI)"))
        for at, offset, target in calls:
            if as_tsv:
                print("0x%08X\tconstruct\t0x%08X\t%d\t0x%08X\t"
                      % (address, at, offset, target))
            else:
                print("  ctor     0x%08X  this+%-6d -> sub_%08X" % (at, offset, target))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
