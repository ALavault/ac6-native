# AC6 retail NTSC-U/J — r77 named the wrong wait; Thread 1 is in the ring-space backoff, not the fence spin (r78)

Date: 2026-08-31.

## Correction to r77

`reports/ac6-retail-native-codegen-gate2-r77-fence-frontier-20260831.md`
disassembled all three functions in the reported chain and characterized
`sub_821E64A8`'s terminal, unbounded spin on `object+0x2AF8 == 0` as "the
actual stall the probe hits." That inference was never checked against a
concrete captured backtrace of the actually-stalled thread — it assumed the
*last* wait in the function is the one active. It wasn't.

The existing artifact
`artifacts/retail-us-native-build-gate2-r51-thread/entry-gdb-all/gdb.log:162-165`
(already captured for r51, not re-run) shows, for **Thread 1** specifically
(the guest's own thread, LWP 3276777 — distinct from the 16 host workers
enumerated earlier in the same file, which are a different, unrelated set of
threads blocked on critical sections/events in unrelated subsystems):

```
Thread 1 (Thread 0x7ffff7e1f640 (LWP 3276777) "ac6recomp"):
#0  0x00005555557327fe in __imp__sub_821E6AC8 ()
#1  0x000055555573165c in __imp__sub_821E61A8 ()
#2  0x0000555555731b2e in __imp__sub_821E64A8 ()
```

Frame #2's return address maps to `0x821e6500`, immediately after
`sub_821E64A8`'s conditional call `bl 0x821e61a8` at `0x821e64fc` (taken
because `object+0x2a9c != 0`) — **not** the fallthrough into the `+0x2AF8`
spin, which sits at the same address but is only reached on the branch this
call preempts. Frame #1's return address maps to right after
`sub_821E61A8`'s own `bl 0x821e6ac8` at `0x821e6244`, inside its bounded
backoff loop. Frame #0 is live inside `sub_821E6AC8`.

**The real blocking condition is `sub_821E61A8`'s ring-space wait**:
```
0x821e61c0: lwz r10, 0x2a90(r31)   ; load pointer field
0x821e61c4: lwz r11, 0x2a9c(r31)   ; load limit/write value
0x821e61c8: subf  r9, r30, r11     ; r30 = requested units (4, from sub_821E64A8's arg)
0x821e61cc: lwz r10, 0x0(r10)      ; DEREFERENCE the pointer -> cursor
0x821e61d0: subf r11, r10, r11
0x821e61d4: cmplw cr6, r9, r11
0x821e61d8: bge  cr6, 0x821e6274   ; enough space -> return
```
never satisfies `bge`, so the loop keeps calling `sub_821E6AC8` for backoff
instead of exiting.

## Established

- `object+0x2a90` is **not** a cursor itself; it is a pointer to a
  separately-allocated 0x60 (96)-byte block. The only static store to
  `object+0x2a90` in the whole image is `sub_821E65B0` (0x821e6740:
  `stw r3,0x2a90(r31)`), immediately after `bl 0x821d74a8` with `r3=0x60`
  and a tag constant in `r4` — an allocator call. `sub_821E61A8` dereferences
  offset 0 of that allocation (`lwz r10,0x0(r10)`) to get the compared value.
- `sub_821E65B0` also resets `object+0x30`/`0x34` (the write cursor pair
  `sub_821E64A8` uses) and `object+0x39e4`/`0x39e8`/`0x2a94` to freshly
  allocated or zeroed state, and conditionally calls `sub_821E64A8` itself
  (0x821e65dc) — consistent with it being a "create/reset this queue
  object" routine, not a per-item consumer.
- r77's `object+0x2AF8` characterization (counting fence, incremented only,
  no static decrement site) stands as read — it just is not what Thread 1 is
  currently blocked on. Kept as background; not re-litigated here.

## Not established

- What is supposed to write offset 0 of the 96-byte block `object+0x2a90`
  points to (the actual "space consumed" cursor `sub_821E61A8` compares).
  No search has been run yet for stores to that pointee (it requires
  tracking the allocation's aliasing, not a plain displacement scan, since
  the write target is `*(ptr)+0`, and `0(reg)` is too generic to search
  directly).
- Whether this ring is graphics-related at all. Given `sub_821E64A8`'s 8
  callers span a very wide address range (`0x82172184` down through
  `0x821f36xx`), this may be a generic engine job/message queue reused by
  several subsystems, not the Vd/PM4 ring specifically. Not yet checked
  against what the two-dword packet payload (`0x5c8`, `0x00020000`) means
  outside a PM4 interpretation, which does not fit (PM4 type-3 headers have
  their top two bits set; `0x5c8` does not).

## Decision

Still no code change. The concrete next static step: find who writes offset
0 of the allocation reachable via `object+0x2a90`. Since the write target is
computed indirectly, the productive search is not another plain-displacement
scan; it likely needs allocator-return tracking (start from `sub_821E65B0`'s
`bl 0x821d74a8` at `0x821e6738` and check what other function receives the
same pointer, e.g. via a store of `object+0x2a90`'s value elsewhere as an
argument) or an indirect-dispatch scan if the drain happens through a
callback. `NEXT.md` updated accordingly; `reports/ac6-retail-native-codegen-gate2-r77-fence-frontier-20260831.md`
is superseded on the "which wait is active" question only, not withdrawn on
its own findings about `object+0x2AF8`.
