#!/usr/bin/env python3
"""Sweep every class deriving from a given RTTI base class and report
whether a specific vtable slot is a named shared stub or something else.

Built for AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS.md:
answers "does any class in the image implement CSwgListener's +0x20
interface for real, or is it dead by design everywhere" — a question a
single live probe run cannot answer, since it can only see whichever
classes happen to be constructed at the moment it samples.

Method (MSVC x86/PPC RTTI, single/non-virtual inheritance only):
  1. Find BASE_CLASS's own TypeDescriptor by scanning every class's own
     locator->ClassHierarchyDescriptor->BaseClassArray for a self-entry
     whose mangled name matches.
  2. Find every BaseClassDescriptor (24-byte struct: typedesc, numContained,
     mdisp, pdisp, vdisp, attributes) referencing that TypeDescriptor -
     these may be COMDAT-folded and shared by many classes at the same
     mdisp, so group by (typedesc, mdisp) and find every baseArray slot
     that references each group's BCD address, not just the BCD itself.
  3. For each referencing slot, walk back to its owning class's own vtable:
     try each plausible base-class-array index, verify the CHD, then among
     locators sharing that CHD pick the one whose own subobject-offset
     field equals the group's mdisp (a class can have multiple vtables in
     a multiple-inheritance layout; the offset field disambiguates them -
     conflating a secondary vtable with the wrong one produced 41 false
     "non-stub" reads before this check was added).
  4. Read --slot-offset of the resolved vtable and compare against
     --stub-address.

This only reaches classes whose base class array happens to have the base
class within the searched index range (0..14) - a class with a deeper
hierarchy is silently skipped, reported in the unresolved count.
"""
import argparse
import struct
import sys
from collections import defaultdict


def build_index(data, base):
    index = defaultdict(list)
    for off in range(0, len(data) - 3, 4):
        val = struct.unpack_from(">I", data, off)[0]
        index[val].append(base + off)
    return index


class Image:
    def __init__(self, path, base):
        with open(path, "rb") as f:
            self.data = f.read()
        self.base = base
        self.size = len(self.data)

    def u32(self, addr):
        off = addr - self.base
        if off < 0 or off + 4 > self.size:
            return None
        return struct.unpack_from(">I", self.data, off)[0]

    def s32(self, addr):
        off = addr - self.base
        if off < 0 or off + 4 > self.size:
            return None
        return struct.unpack_from(">i", self.data, off)[0]

    def cstr(self, addr, maxlen=96):
        if addr is None:
            return None
        off = addr - self.base
        if off < 0 or off >= self.size:
            return None
        end = self.data.find(b"\x00", off, off + maxlen)
        if end == -1:
            end = off + maxlen
        return self.data[off:end].decode("latin1", errors="replace")


def find_base_typedesc(img, index, mangled_name):
    """Find BASE_CLASS's own TypeDescriptor by locating a self-referential
    BaseClassDescriptor (numContainedBases==0, attributes==64, mdisp==0)
    whose type name matches, reached via any class's own CHD walk."""
    needle = mangled_name.encode("latin1")
    for off in range(0, img.size - len(needle)):
        if img.data[off:off + len(needle)] != needle:
            continue
        name_addr = img.base + off
        typedesc_addr = name_addr - 8
        if img.u32(typedesc_addr + 8) is None:
            continue
        if img.cstr(typedesc_addr + 8) != mangled_name:
            continue
        return typedesc_addr
    return None


def resolve_owning_vtable(img, index, slot, expect_mdisp, max_index=14):
    for i in range(0, max_index + 1):
        base_array_start = slot - i * 4
        chd = base_array_start - 0x10
        sig = img.u32(chd)
        num_base = img.u32(chd + 8)
        base_array_ptr = img.u32(chd + 12)
        if sig != 0 or base_array_ptr != base_array_start:
            continue
        if num_base is None or num_base <= i or num_base > max_index:
            continue
        for addr_holding_chd in index.get(chd, []):
            locator = addr_holding_chd - 0x10
            offset_field = img.s32(locator + 4)
            if offset_field != expect_mdisp:
                continue
            typedesc = img.u32(locator + 0x0C)
            cls_name = img.cstr(typedesc + 8) if typedesc else None
            if not cls_name or not cls_name.startswith(".?AV"):
                continue
            for addr_holding_locator in index.get(locator, []):
                return cls_name, addr_holding_locator + 4
    return None, None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", help="flat memory image, e.g. .build/Default.xex.base.bin")
    ap.add_argument("--base-address", default="0x82000000",
                     help="load address of image[0] (default 0x82000000)")
    ap.add_argument("--base-class", required=True,
                     help='mangled RTTI name to sweep derivers of, e.g. ".?AVCSwgListener@@"')
    ap.add_argument("--slot-offset", type=lambda s: int(s, 0), default=0x20,
                     help="vtable byte offset to inspect (default 0x20)")
    ap.add_argument("--stub-address", type=lambda s: int(s, 0), required=True,
                     help="function address considered a no-op stub")
    ap.add_argument("--max-base-index", type=int, default=14,
                     help="max base-class-array index to try when resolving (default 14)")
    args = ap.parse_args()

    base = int(args.base_address, 0)
    img = Image(args.image, base)

    print("building value index...", file=sys.stderr)
    index = build_index(img.data, base)
    print(f"index built: {len(index)} distinct 32-bit values", file=sys.stderr)

    typedesc = find_base_typedesc(img, index, args.base_class)
    if typedesc is None:
        print(f"error: could not locate TypeDescriptor for {args.base_class}", file=sys.stderr)
        return 1
    print(f"{args.base_class} typedesc = 0x{typedesc:08X}")

    bcd_candidates = index.get(typedesc, [])
    groups = {}
    for bcd in bcd_candidates:
        num_contained = img.u32(bcd + 4)
        mdisp = img.s32(bcd + 8)
        attr = img.u32(bcd + 20)
        if num_contained != 0 or attr != 64:
            continue
        referers = index.get(bcd, [])
        if referers:
            groups.setdefault(mdisp, []).extend(referers)

    total = 0
    resolved = 0
    nonstub = []
    for mdisp, slots in sorted(groups.items()):
        print(f"\n=== mdisp={mdisp}: {len(slots)} referencing slot(s) ===")
        for slot in slots:
            total += 1
            cls, vtable = resolve_owning_vtable(img, index, slot, mdisp, args.max_base_index)
            if cls is None:
                print(f"  slot@0x{slot:08X}: UNRESOLVED")
                continue
            resolved += 1
            value = img.u32(vtable + args.slot_offset)
            tag = "STUB" if value == args.stub_address else "NON-STUB"
            print(f"  {cls}  vtable=0x{vtable:08X}  +0x{args.slot_offset:X}=0x{value:08X}  {tag}")
            if value != args.stub_address:
                nonstub.append((cls, vtable, value))

    print(f"\n=== summary: {total} slots, {resolved} resolved, {total - resolved} unresolved, "
          f"{len(nonstub)} non-stub implementor(s) ===")
    for cls, vtable, value in nonstub:
        print(f"  {cls}  vtable=0x{vtable:08X}  handler=0x{value:08X}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
