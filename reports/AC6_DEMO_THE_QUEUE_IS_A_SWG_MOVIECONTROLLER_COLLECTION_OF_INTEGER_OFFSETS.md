# The queue is a swg::MovieController collection of integer offsets, drained by index each tick

## Qualification

AC6 demo PAL, same XEX SHA-256. Entirely static, no probe run. Continues
`390cfe33`'s named next step: `sub_82323BB8`'s body, read from its start
through the loop `390cfe33` had already partially characterized
(`ppc_recomp.43.cpp:16291-16812`), plus a raw read of two static jump
tables (`0x8264CDC0`, `0x8264CDF8`) and the RTTI `TypeDescriptor`
adjacent to the second one, validated against 771 other occurrences of
the same shared `pvftable` in the image (all followed by a proper `.?AV`
name — the same check `tools/whose_vtable.py` already performs, done by
hand here since the object located isn't a class vtable itself).

## The class: `swg::MovieController`

`sub_82323BB8`'s first phase (`this+8`/`this+4`, a single "current" item,
not the loop `390cfe33` described) dispatches through two 6-entry static
jump tables sitting back-to-back in the image (`0x8264CDC0`: entries
`0..5` = `{0x82322E68, 0x82322E68, 0x823235D0, 0x823235E8, 0x82323600,
0x82323600}`; `0x8264CDF8`: `{0x82322E98, 0x82323618, 0x823237D8,
0x82323618, 0x82322EC8, 0x82323630}`; both zero-padded from index 6 to
13, matching the `0x38`-byte/14-word spacing between the two bases).
Immediately after the second table's padding sits an MSVC
`TypeDescriptor` (`pvftable=0x8203052C`, the same shared vtable 771 other
type descriptors in this image use) whose name reads directly:
**`.?AVMovieController@swg@@`** — `swg::MovieController`. The six
type-dispatched handlers this phase calls (all clustered in
`0x82322E6x`-`0x823237Dx`, the same translation unit as the interpreter
family `346255b2`/`1fcc88b3`/`b67e7f6f` already located) compute a 2D
affine transform (scale/rotation/translation, `lfs`/`fmuls`/`fmadds`
chains storing to `this+320..340+`) — this is the clip's own **animation
update**, unrelated to script dispatch; not read further here.

## The queue, precisely

Later in the same function, a **second, distinct** block (previously
partially read in `390cfe33`) is the actual script-event queue:

```
r28 = [[this+16]]+40](this+16)     // virtual call, slot 10 -- Count()
if r28 <= 0: skip the loop entirely
r29 = r24                          // starting index (r24's own source unread)
loop while r29 < r28:
    r30 = [this+16]                 // the collection object itself
    r22 = [[r30+0]+44](r30, r29)     // virtual call, slot 11 -- GetAt(index) -- returns an INTEGER OFFSET
    (r3=r30) = [[r30+0]+32](r30)     // virtual call, slot 8 -- purpose unconfirmed (advance/remove?)
    sub_82325288(r3=this+256, r4=r22, r5=this)   // the thunk `390cfe33` already traced
    r29 += 1
```

**`this+16` is a sized, indexable collection whose elements are plain
integer offsets**, not typed event objects — `GetAt(index)` (slot 11)
returns the value directly, and that value is exactly the `r4`
`390cfe33` showed becomes `table_base + r4` in `sub_823251E0`. **This
sharpens, and does not correct, `390cfe33`'s "two virtual methods per
item" description** — slot 11 for the offset and slot 8 for a second
step both stand; what's new is the `Count()` call establishing the loop
bound and `this+16`'s identity as a genuine collection object rather than
a linked list (the phase-1 dispatch's `this+8`/`this+4` pointer-chase is
a *different* item source, for the unrelated animation-update phase).

## What this means for "what selects each tick's entry"

The question `390cfe33` left open — "what's in the queue and who enqueues
it" — is now precisely: **who calls `Add`/`Insert`/`Push` on the
collection at `this+16`, with what offset, and under what condition.**
Neither the collection's own class (its vtable, reachable from
`[this+16]+0`) nor its insertion method has been read. This is the
concrete next step, and it is now well-defined rather than open-ended:
find the collection's insert method (same shape as this campaign's own
prior map/insert-tracing work, `25d092bc`) and enumerate its callers.

## Not established

- The collection's own class/vtable, its insert method, and every
  caller of that insert method — not read. This is where the answer to
  "could an offset landing inside EndMode's statement ever be enqueued"
  actually lives; nothing read so far bears on it directly.
- `r24`'s own source (the loop's starting index) — not traced; plausibly
  always `0`, not confirmed.
- The slot-8 virtual call's purpose (advance an internal cursor? remove
  the drained item? both are consistent with a queue-drain shape;
  neither is confirmed).
- Phase 1's own six handlers and their relationship, if any, to phase
  2's queue (read only as "unrelated animation math," not exhaustively
  ruled out as ever touching the queue).

## Gates

No source changed; report-only commit, entirely static reading of
generated code and the flat XEX image.
