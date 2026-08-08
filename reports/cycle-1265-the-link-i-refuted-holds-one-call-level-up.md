# Cycle 1265 — the link I refuted holds, one call level up

## Qualification

`ghidra-projects-xenon/ac6-xenon`; `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## What cycle 1263 concluded, and why it was right to

Cycle 1263 drafted `0x82345098`'s `3 × 6 × 8` clearing loop as structural
corroboration of the vertex stride rule, then tested the adjacency the reading
rested on and refused it:

> `bl 0x82345098` — one caller, `0x8233B318`. `bl 0x82345100` — one caller,
> `0x8233E6E4`. The two functions are four instructions apart in the image and
> have entirely separate single callers. Nothing ties the cleared tables to the
> stride path except that the code sits next to it.

That refutation was correct. **Adjacency in the image is not evidence**, and the
cycle closed with the right next question: *what is `0x8233B318`?*

## Established — the answer, and it reinstates the link on other grounds

`0x8233B318` sits inside the function beginning at `0x8233B2E8`:

```
8233b2fc  or   r30,r3,r3
8233b300  li   r31,0x0
8233b304  addi r3,r30,0x28
8233b308  stw  r31,0x4(r30)
8233b30c  stw  r31,0x24(r30)
8233b310  bl   0x82346f88        ; on this+0x28
8233b314  addi r3,r30,0x170
8233b318  bl   0x82345098        ; on this+0x170
8233b31c  addi r11,r30,0xa70
```

Set beside `0x8233E6B0`, the container initialiser cycle 1262 read:

```
8233e6c4  addi r29,r30,0x8
8233e6cc  bl   0x823d6a7c        ; on this+0x08
8233e6d4  addi r3,r30,0x28
8233e6d8  bl   0x82346fc0        ; on this+0x28
8233e6e4  bl   0x82345100        ; on this+0x170
```

**The same object, the same two members, two sibling functions:**

| member | built by | cleared by |
|---|---|---|
| `this+0x28` | `0x82346FC0` | `0x82346F88` |
| `this+0x170` | `0x82345100` | `0x82345098` |

So `0x82345098` clears precisely what `0x82345100` builds. The connection cycle
1263 could not find between two functions **is one call level up**, in the pair
of callers that operate on the same fields of the same class — and it is a
structural relation, not a positional one.

## What that does to the `3 × 6 × 8` reading

Cycle 1263 ended with the shape as "suggestive and uncontrolled": a
`3 × 6 × 8` nest of 8-byte slots, twice, matching the arity of the stride
formula's index spaces — `T8` with 8 entries, `T18` with 18 laid out as 3 by 6 —
and nothing tying it to the stride path.

It is now tied. The tables cleared at `this+0x170` are the same member the
stride function is constructed on, so a cache dimensioned by exactly the two
index spaces the stride formula reads is no longer a coincidence of loop bounds.

**This is corroboration of the stride rule and it is still not a proof of it.**
What is established is that the object carries a `[3][6][8]` table at the member
the stride builder owns. What is *not* established is that the table is a cache
of computed strides — nothing here reads a slot, and only the first word of each
8-byte slot is ever zeroed.

## The shape of this sequence, which is the reason to write it down

The claim was drafted, refuted by its author on the evidence it actually had,
and then re-established two cycles later on evidence of a different kind. The
refutation was not wasted: had cycle 1263 published the adjacency argument, this
cycle would have found the caller and *agreed with it for the wrong reason*, and
nobody would have learned that image order proves nothing.

The general form: **when a plausible link fails its test, the question to carry
forward is not "is the link real" but "what would a real link look like".** Here
it looked like a shared member offset in a shared class, which is checkable, and
which the first reading had no way to reach because it never left the two
functions.

## Not established

- **What an 8-byte slot holds**, and whether the two `0x480` tables are one
  cache or two. Unchanged from cycle 1263.
- **What the class at `r30` is.** Both take it as `this`, and both are reached
  through tables rather than by call: `0x8233E6B0` has **zero** direct `bl`
  callers and appears once as aligned data at `0x82085888`; `0x8233B2E8` has two
  callers (`0x8233B4B8`, `0x823D370C`) and also appears once as data, at
  `0x82085618`. The two table slots are `0x270` apart in the same region.
  **Walking back from each slot for an RTTI locator recovered nothing**, so the
  type is unread and "the same object" still rests on the shared offsets, not on
  a name. That is weaker than it would be with a type, and it is what there is.

  The `+0x28` pair is confirmed exclusive, which the offsets alone did not give:
  `0x82346F88` has exactly one caller, `0x8233B310`, and `0x82346FC0` exactly
  one, `0x8233E6D8` — each build/clear routine is called from one place, and
  those two places are the sibling functions.
- Whether `0x82346F88` / `0x82346FC0` do to `+0x28` what the other pair does to
  `+0x170`. Their call sites are now exclusive and symmetric; neither function
  was read.
