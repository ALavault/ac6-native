# `CSwgCallback+9`'s three reachable "set to 1" candidates are all ruled out by class identity

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: `.build/Default.xex.base.bin`
(RTTI walks), `codegen/generated/ppc_recomp.*.cpp` (control-flow evidence
only). Reachability control: the same valid 12000-tick, START-at-3000 atlas
used in `39dc4038` (`indirect_edges` block used for the one indirectly-
dispatched candidate).

## What this closes

`39dc4038` corrected `bdb437e6`'s unchecked reachability claim to "checked
and inconclusive": of ~25 static `stb rN,9(rM)` sites in the codebase,
three reached ones store an immediate `1` (`sub_8216C900`, `sub_8219CBF8`,
`sub_8227ABD8`), and a body-only read suggested — without proof — that two
of them belong to unrelated, larger objects. This report traces all three
to a definitive answer instead of a reading.

## The three, resolved

**`sub_8227ABD8`** is a constructor: it installs vtable `0x8200A8CC` at
`[this+0]` (the same instruction shape as `CSwgCallback`'s own constructor,
`sub_820CD990`) before setting `[this+9]=1`. RTTI-walked directly:

```
0x8200A8CC - 4 -> locator -> typedesc -> ".?AVCAce6Session@ACE6@@"
```

**`CAce6Session`** — session/connection management, nothing to do with
`swg`. A different class installing a different vtable into a different
object; it cannot write `CSwgCallback`'s byte regardless of reachability.

**`sub_8216C900`** is called exactly once in the whole 12000-tick run (the
atlas's own `indirect_edges`, `count: 1`, `lr=0x82190BCC`), and only
indirectly — through a vtable slot, not a direct `bl`, which is why the
first, direct-caller-only grep found nothing calling it. The call site
(`sub_82190B18`, `ppc_recomp.16.cpp:12654-12660`) loads an object pointer
into `r3`, then dispatches through **that object's own vtable slot `+0x20`**
(`lwz r11,0(r3); lwz r11,32(r11); mtctr r11; bctrl`). `CSwgCallback`'s own
vtable (`0x820061FC`, dumped in full in `bdb437e6`) has the confirmed
no-op stub (`0x820AC748`) at slot `+0x20` — not `sub_8216C900`. Whatever
object this call site targets, it is not (and cannot be) a `CSwgCallback`
instance: the same virtual slot number resolves to a provably different
function on this class.

**`sub_8219CBF8`** is called once, directly, from `sub_821929A8`, with
`r3 = caller's r31 + 84` — i.e. it operates on a sub-object embedded at a
fixed `+84` offset from whatever object `sub_821929A8` was given. Even in
the most generous reading (the caller's object *is* the same "world"
object `CSwgCallback` lives inside, at `[CSwgManager+4]`), `sub_8219CBF8`'s
own `[this+9]` would land at `world+84+9 = world+93` — a different byte
than `CSwgCallback`'s own field at `world+236+9 = world+245`. The offset
alone rules it out, independent of what class `sub_821929A8` actually is.

## Conclusion

All three reachable, literal-`1` candidates are definitively not arming
paths for this instance: one is a different class's constructor entirely,
one is unreachable through this class's own vtable by slot-number proof,
and one writes a structurally different offset even under the most
favorable object-identity assumption. Combined with `bdb437e6`'s finding
that the class's own constructor unconditionally zeroes the flag and none
of its six real (non-stub) vtable slots write it directly, **no reachable
mechanism to set `CSwgCallback+9` nonzero was found**, matching the same
shape (and now closed to the same rigor) as `[0x827AD2F0]`
(`03179c5b`/`8fba5b45`).

## Not established

- The four other reached-but-unread candidates from `39dc4038`
  (`sub_8219B5E8`, `sub_8219BE90`, `sub_8216C9A8`, `sub_821B1810`,
  `sub_820CDCD8`) store values other than a literal `1` (per `39dc4038`'s
  table: unresolved, `0`, or `2`) and were not re-examined here — ruled out
  by value, not by identity, so this list is not as rigorously closed as
  the three above.
- Whether the object `sub_82190B18`/`sub_821929A8` actually operate on has
  any bearing on `CSwgCallback` at all — neither was named.
- This remains a **static and semi-dynamic** result (constructor body +
  reachability atlas + one live watched run), not a full-run guarantee:
  the live watcher in `bdb437e6` only covered one 3600-tick run.

## Gates

No source changed; report-only commit.
