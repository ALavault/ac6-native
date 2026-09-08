#!/usr/bin/env python3
"""Apply the r414 fix for a regression in r366's INVENTED
RtlLeaveCriticalSection fix for sub_821FA9E0.

Background (see reports/ac6-retail-native-codegen-gate2-r413-missing-
commit-explained-r366-invented-fix-clobbers-its-own-return-value-
20260908.md and r424_return_chain.gdb/.log under
artifacts/retail-us-native-r404-bucket17-writer/):

r366 (tools/apply_sub_821fa9e0_leave_fix.py) closed a real, well-
documented critical-section leak in sub_821FA9E0 by inserting a
conditional RtlLeaveCriticalSection call immediately before the
function's single return, reusing ctx.r3 to carry the critical
section's "this" pointer -- exactly as RtlEnterCriticalSection does
earlier in the same function.

What r366 missed: at the point that call is inserted, ctx.r3 already
holds this function's own return value, reloaded two lines above via
`lwz r3,356(r31)` (the pointer sub_821FA9E0 -- a realloc() -- is about
to hand back to its caller). Writing the critical-section pointer into
ctx.r3 to pass it to RtlLeaveCriticalSection clobbers that return
value, and it is never restored afterward -- the function always
returns 0 (or whatever garbage happens to be loaded from
heap+1408/0x580) whenever the lock was actually held, i.e. on every
call that took the "grow" path. r413 captured this live end to end:
sub_821F9E10 returns the correct new pointer (0x100015a0 in the
captured run), but sub_821FA9E0 itself returns 0, and sub_823857E0/
sub_8237FA50 (the growable-array push_back driving r399-r412's whole
overflow chain) faithfully propagate that already-lost 0.

This script patches the ALREADY-r366-PATCHED block to save ctx.r3
before entering the critical section and restore it immediately after
leaving -- the RtlLeaveCriticalSection call itself is untouched, only
the return-value clobber around it is fixed. It is idempotent (checks
for its own marker) and requires r366's fix to already be present
(it patches r366's own inserted block, not the pristine unpatched
source) -- run tools/apply_sub_821fa9e0_leave_fix.py first if starting
from a clean regeneration.
"""

import argparse
import sys
from pathlib import Path

MARKER = "BUGFIX (r414)"

FUNC_SIGNATURE = "PPC_FUNC_IMPL(__imp__sub_821FA9E0)"

R366_MARKER = "BUGFIX (r366, INVENTED)"

OLD_BLOCK = """	if ((r23.u32 & 1u) != 0) {
		ctx.r3.u64 = PPC_LOAD_U32(r27.u32 + 1408);
		__imp__RtlLeaveCriticalSection(ctx, base);
		r23.u64 = r23.u64 ^ 1;
	}"""

NEW_BLOCK = """	if ((r23.u32 & 1u) != 0) {
		// %s: r366's fix (above) reuses ctx.r3 to pass the critical
		// section pointer to RtlLeaveCriticalSection without saving or
		// restoring this function's own return value, which was just
		// reloaded into ctx.r3 two lines above (`lwz r3,356(r31)`).
		// Confirmed live (r413/r424): sub_821FA9E0 returns 0 instead of
		// the real result on every call that held the lock -- exactly
		// the growth path that drives the r399-r412 overflow chain.
		// Save/restore ctx.r3 around the Leave call to fix this without
		// touching r366's actual leak fix.
		uint64_t r414_saved_r3 = ctx.r3.u64;
		ctx.r3.u64 = PPC_LOAD_U32(r27.u32 + 1408);
		__imp__RtlLeaveCriticalSection(ctx, base);
		ctx.r3.u64 = r414_saved_r3;
		r23.u64 = r23.u64 ^ 1;
	}""" % MARKER


def patch_file(path: Path) -> str:
    text = path.read_text()
    if FUNC_SIGNATURE not in text:
        raise SystemExit(f"error: {FUNC_SIGNATURE} not found in {path}")
    if MARKER in text:
        return "already-applied"
    if R366_MARKER not in text:
        raise SystemExit(
            "error: r366's fix (BUGFIX (r366, INVENTED)) is not present -- "
            "run tools/apply_sub_821fa9e0_leave_fix.py first, this script "
            "patches r366's own inserted block, not the pristine source"
        )
    count = text.count(OLD_BLOCK)
    if count == 0:
        raise SystemExit(
            "error: expected r366-patched if-block not found -- the "
            "generated source may have changed since this script was "
            "written; re-derive the exact block before patching"
        )
    if count != 1:
        raise SystemExit(
            f"error: expected exactly one occurrence, found {count} -- "
            "re-check before patching all of them blindly"
        )
    patched = text.replace(OLD_BLOCK, NEW_BLOCK)
    path.write_text(patched)
    return f"applied ({count} occurrence(s) patched)"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "generated_file",
        type=Path,
        help="path to the generated ppc_recomp.*.cpp file containing sub_821FA9E0 "
        "(e.g. build/ntsc-uj/native/codegen-<tag>/generated/ppc_recomp.27.cpp)",
    )
    args = parser.parse_args()

    if not args.generated_file.is_file():
        raise SystemExit(f"error: {args.generated_file} does not exist")

    result = patch_file(args.generated_file)
    print(f"{args.generated_file}: {result}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
