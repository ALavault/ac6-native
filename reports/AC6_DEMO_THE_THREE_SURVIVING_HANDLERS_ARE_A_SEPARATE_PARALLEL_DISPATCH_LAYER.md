# The three surviving cursor writers are a separate, parallel dispatch
# layer — not evidence the enqueue mechanism partially survives

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static reads, in full: `sub_823239F0`
(`ppc_recomp.43.cpp:16019-16088`), `sub_82326608`
(`ppc_recomp.44.cpp:4211-4278`), `sub_823266F8`
(`ppc_recomp.44.cpp:4351-4469+`, read through its main dispatch loop). No
probe run, no source change.

## What this checks

`f99ec99f` corrected the exact writer counts and left "what
`sub_823239F0`/`sub_82326608`/`sub_823266F8` actually do" as the direct
next static read — per `advisor`'s explicit diagnosis that three straight
corrections came from inferring semantics from write-log patterns rather
than reading code.

## `sub_823239F0` and `sub_82326608`: identical-shaped, nested type
## dispatchers — a different mechanism from the outer loop entirely

Both functions (called with the owner as `r3`) do the same thing: read
the shared cursor `[owner+248]`, use it to index a **secondary** array
(`[[owner+32] dereferenced]+56, indexed by [cursor+8]*8`) to get a type
value, multiply by 4, index into their **own** jump table (`0x8264CE54`
for `sub_823239F0`, `0x8264D07C` for `sub_82326608` — both computed
directly from the `lis`/`addi` immediates, both *different* from
`sub_82323BB8`'s own outer table at `0x8264CE6C`), call the resolved
function (`r5`/`r4` = cursor, `r6`/`r5` = `0`), then unconditionally
advance the cursor by **20 bytes** (`cursor+20`, not `+8` the way
`sub_82322A80` advances it) and return.

**These are not "the same enqueue mechanism, still partially working."**
They are structurally distinct handler types — each processes a
different-shaped element (20-byte stride, own secondary type lookup, own
jump table) from the shared per-frame element stream `sub_82323BB8`'s
outer loop walks. Being called at all simply means the outer loop's
per-tick dispatch still selects *these* element types; it never selected
`sub_82322A80`'s slot either before or after the press in this specific
sampled window's remaining activity — `sub_82322A80` dropping out is
still real and specific to its own slot, not part of a general collapse.

## `sub_823266F8`: a second, independent outer-loop-shaped dispatcher

This function is shaped completely differently — it is **its own
complete copy of the outer-loop pattern**, not a leaf handler:

```
if [owner+215] == 0: return immediately (flag gate)
if [owner+220] < 0: return
size = ([owner+44] - [owner+40]) >> 3          // identical formula to
                                                    sub_82323BB8's own
if [owner+220] >= size: return
entry = [owner+40] + [owner+220]*8              // same table, same index
loop_count = [entry+4]                            // same "+4 = count" shape
cursor2 = [entry+0] + [[owner+32]]                  // its OWN cursor,
                                                        stored to the SAME
                                                        [owner+248] field
if loop_count > 0:
  loop N times: type = [[cursor2]+0]; table[0x8264D094][type*4](owner); ...
then: two more direct calls through [owner+408] and [owner+412]'s own
  vtable-shaped indirect slots, if non-null.
```

**This reads the identical `[owner+40]`/`[owner+44]`/`[owner+220]` frame
table `sub_82323BB8`'s outer loop uses, independently, gated by its own
flag byte `[owner+215]`, dispatching through yet a third jump table
(`0x8264D094`).** It is a parallel consumer of the same per-frame data,
not a fallback or continuation of the enqueue path. It keeps firing after
the press (confirmed live, `f99ec99f`) because its own flag
(`[owner+215]`) stays set and the shared table swap (`7513c9dc`) affects
it the same structural way it affects the outer loop — its own loop count
also becomes `1` after the swap (same `entry+4` field, same table) — but
whatever its own table (`0x8264D094`) resolves the post-swap type to is
unread here.

## Reading

**There is no single "the enqueue mechanism," but at least four parallel
consumers of one shared per-frame element stream** — the outer loop
(table `0x8264CE6C`, includes `sub_82322A80`), and three
sibling/nested dispatchers (`0x8264CE54`, `0x8264D07C`,
`0x8264D094`) each reading the same table with their own type-resolution
and stride. The frame-table swap (`7513c9dc`) is a genuine, single event
affecting all of them structurally at once (fewer elements, shorter
table) — `sub_82322A80` specifically dropping out means the NEW frame's
element set no longer includes anything of the outer loop's
`sub_82322A80` type, while apparently still including at least one
element each of the types the other three dispatchers care about (or,
for `sub_823266F8` specifically, its own independent loop count from the
same swapped table also still resolves to something it can call). This
is consistent with `7513c9dc`'s reading (a genuine content change on
press, not a mechanism failure) and gives no evidence either way about
whether the new frame is a dead end or a real next step.

## Not established

- What `0x8264D094`, `0x8264D07C`, and `0x8264CE54`'s own slot contents
  are, or what functions they name — not read.
- Whether `sub_823266F8`'s own post-press dispatch (its inner loop, its
  own cursor) actually calls anything meaningful, or silently resolves to
  a null/no-op slot — not traced.
- `[owner+408]`/`[owner+412]`'s own indirect calls (unconditional, gated
  only on non-null) — not identified; these run every time
  `sub_823266F8` runs, independent of the frame-table content, and were
  not examined.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change — pure static read.
