# Cycle 1350 — three functions, three calls each

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus and the image were
  read.
- No product C++ changed, no contract changed.

## The nine call sites are three, three times

`sub_820A7070`'s nine callers are **three functions**, each calling it three
times, in the same shape:

| caller | `r5` at call 1 | call 2 | call 3 |
|---|---|---|---|
| `sub_82097560` | `r27` | `r24` | `r28 + 0x124030` |
| `sub_8219BDD8` | `r27` | `r24` | `r28 + 0x123C40` |
| `sub_8219F8C0` | `r27` | `r24` | `r28 + 0x130710` |

Identical register choices across three unrelated functions — `r27`, then `r24`,
then a computed offset — which is a compiler emitting the same source pattern
three times, not a coincidence.

## The table lives inside a very large arena

The third call in each triple builds its argument as `addis r5,r28,N` then
`addi r5,r5,M`, and `sub_82097560` stores the result back at `r28+0x2E8`.

`r28` is not an object. Nearby, the same function does
`lwzx r4,r28,r11` with `r11 = 0x12544F0` — an offset of **over nineteen
megabytes**. Whatever `r28` points at is an arena, and the child tables sit at
fixed displacements inside it: `0x123C40`, `0x124030`, `0x130710` — three tables
within 0xD000 of each other, and a fourth region 0xC000 further on.

## Which is a good place to stop this descent

The question this thread has been chasing since cycle 1341 is what class the
children are. Eight cycles have moved it:

```
child slots +0xC0/+0xC4/+0xC8         cycle 1341
child locator at +0x60                cycle 1340
-> derived from galib::CGaObj          cycle 1346
-> not in the class map                cycle 1346
-> 306 vtables carry no RTTI           cycle 1348
-> one candidate, 0x820078D0           cycle 1348
child array = caller's 5th argument    cycle 1349
-> = arena + 0x124030 and two more     this cycle
```

Every step was measured and several corrected a predecessor. But the target has
receded each time, and it is now inside a nineteen-megabyte arena whose base is a
register in a dispatcher with five jump tables.

**The candidate from cycle 1348 is still the only one that fits**, and confirming
it by tracing this chain further is not obviously cheaper than confirming it
directly — the harness can execute `0x822A1668` on a built service with a child
whose vtable is `0x820078D0` and see whether the three slot calls land. That is
one capsule against an arena walk of unknown depth.

## Not established

- What the arena is, and what its tables hold.
- Whether the children carry `0x820078D0`.
- The count's source, still path-dependent from cycle 1349.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Build the capsule instead of walking further. `0x822A1668` needs a unit with
`[+0x60] & 0x4000`, a one-element child array, and a child whose vtable is
`0x820078D0` — all synthetic, all under the harness's control. If the three slot
calls fire, the candidate is confirmed against the code that consumes it rather
than against the code that fills it, and this thread stops descending.
