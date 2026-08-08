# Cycle 1215 — correcting cycle 1214: the guard is a loop body

## What I got wrong, one cycle ago

Cycle 1214 read `0x820FB050`–`0x820FB078` as a **fixed-member existence test**
and spent a census establishing which member it names. It names none. It is the
body of a loop:

```
820fb034  or     r30,r26,r26        ; r30 = 0     (r26 is written once, li r26,0x0)
820fb048  or     r4,r30,r30         ; <- LOOP HEAD
820fb050  bl     0x82234dd0         ;    member r30's pointer
820fb064  bl     0x82234e08         ;    member r30's size
820fb070  beq    cr6,0x820fb120     ;    absent -> next member
820fb078  beq    cr6,0x820fb120
820fb094  bl     0x82337C68         ;    THE NDXR PATH
820fb0a8  bl     0x82338DC0
820fb0c0  bl     0x82234dd0         ;    the same member again
820fb0fc  bl     0x82335F18         ;    an NTXR pack mount
820fb110  addi   r30,r30,0x1
820fb118  cmpwi  cr6,r30,0x100
820fb11c  blt    cr6,0x820fb048     ; <- BACK EDGE
```

**`0x820FA9C0` iterates member indices 0 through 0xFF and feeds every member that
exists to the NDXR loader, then mounts it as an NTXR pack.** There is no chosen
member; there is a sweep.

This is a better result than the one I published. It means the NDXR path is not
conditional on some particular member being present — it runs **once per existing
member of the mission's container**, twenty-six times for Mission 01.

## How the error happened, and the rule it earns

I dumped 300 instructions from `0x820FA9C0`; the guard sits past the end. I read
it from a **second** dump starting at `0x820FAE90`, analysed the conditional
above it exactly as cycle 1213's rule requires — *walk up to the nearest
conditional and establish what selects it* — and stopped. The back-edge was
`0x8C` bytes **below** where I stopped reading.

**Cycle 1213's rule has a twin: walk down to the nearest back-edge.** A
conditional tells you what selects a block. It does not tell you how many times
the block runs, or over what. I applied half a rule and got a confident,
well-controlled, wrong answer — the census in cycle 1214 is all correct and all
beside the point.

It also explains a smell I noted and did not chase: cycle 1214 recorded that
"`r26` is the index at the guard, and the cached probes use literals while the
guard uses a register." **A register where the neighbours use literals is a loop
counter.** I wrote the observation down in the "not established" list instead of
following it.

## What survives from cycle 1214

- The accessor semantics: a five-word directory, count at `+0x00`, base at
  `+0x04`, offset table at `+0x0C` (`0x82234DD0` → `base + offsets[i]`), size
  table at `+0x10` (`0x82234E08` → `sizes[i]`).
- The literal-index probes at `820faaf4`–`820fabb4` — members 4, 5, 6, 8, 7 cached
  into `this+0x0C…` with sizes at `this+0x40…`. Those are **before** the loop and
  are a different mechanism.
- The member census of Mission 01's entry: 26 members, 4–10 carrying two `.ntxr`
  each, 13/17/22/23 carrying none, and no bare `.ndxr` anywhere — the geometry
  living in `001_MDLP.mdlp` and the two large bundles.

What changes is the census's role. It no longer discriminates a guard; it
describes what the sweep will find.

## And a correction to the load-path report

Cycle 1213 placed the six `mode = 0` NTXR mounts "on DPL container members `0x7`,
`0x8`, `0x9`". At least one of them — `820fb0fc`, which I have now read — is
**inside this 0…0xFF sweep** and is therefore on no fixed member at all. I have
not read the other five and do not extend the correction to them.

## Not established, stated plainly

- Who supplies the container. Unchanged from cycle 1214, and still the link
  between "the sweep runs over a container" and "that container is Mission 01's".
- What the pre-loop literal probes of members 4–8 are for, given that the sweep
  visits every member anyway.
- `0x82338DC0`, called between the NDXR path and the mount.
- Whether the loop's `0x100` bound is ever reached, or whether the directory's
  own count cuts it short first — `0x82234DD0` returns null past `[dir+0x00]`, so
  a short directory simply skips, but that is a property of the accessor rather
  than something read at the loop.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
back edge 820fb11c blt cr6,0x820fb048, bound cmpwi r30,0x100 — read here
```

No product code changed.
