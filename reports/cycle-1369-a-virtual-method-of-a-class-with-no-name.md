# Cycle 1369 — a virtual method of a class with no name

## Qualification

- Ghidra was used for two reference scans. **No oracle pass.**
- No product C++ changed, no contract changed.

## `f31` is the function's own float argument

`fmr f31,f1` in the prologue, and nothing rewrites it in 359 instructions. So the
scale applied to every rate before the position step is **passed in**, and the
question moves to the caller.

## There is no caller

`0x82303110` has **no direct callers at all**. Its only non-`.pdata` reference is a
single `.rdata` word at `0x8200F2EC` — slot **+0x7C**, index 31, of a **36-slot
vtable based at `0x8200F270`**.

That vtable holds **zero at `vtable−4`**: no Complete Object Locator, so it is one
of the **306 unnamed vtables** cycle 1348 counted. Its class has no RTTI and the
class map cannot name it.

## Which explains the whole thread

Sixteen cycles followed data from named entry points — the input tick, the
transform, the unit's vtable, the children, the arena, a placement branch. Every
one was bounded and answered and none reached flight.

**They could not have.** The integrator is a virtual method of an unnamed class,
invoked only through a vtable slot. There is no `bl` to follow, and no RTTI to
look up. A data-following search is structurally unable to arrive.

The signature search reached it in four cycles because it never needed a name or a
call edge — only a shape.

That is the lesson worth keeping from A3.2, and it generalises: **when following
call edges keeps terminating in bounded, correct, irrelevant answers, the target
may not be on any call edge you can see.** 27% of this binary's classes are
invisible to the map, and the flight model is in that 27%.

## Not established

- What `f31` is. The caller is a virtual dispatch and the object's class has no
  name, so the answer needs either a capsule or the call site inside whatever
  invokes slot 31.
- What class `0x8200F270` is.
- What the other two all-three-component candidates do.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
tools/tests                          Ran 72 tests, OK
```

## Next

Who invokes slot **+0x7C**. It is an enumerable property — `lwz rX,124(rY)`
followed by `mtctr`/`bctrl` — and it is the same kind of narrow, distinctive
search that found the integrator, rather than another walk down a call chain that
cannot reach it.
