# Cycle 1181 — the resource registry is runtime-populated, and the API is located

## Task 2d's named next step, taken

Cycle 1175 left the FHM reader blocked and named where to resume: *"read the
registry at `0x82675B80` and what `0x82343010` does with a type it has not
seen."* The first half is done and it is a negative.

**The registry is all zeros in the image.** Dumped `0x82675B80`–`0x82675C00`:
every word is zero. It is BSS, populated at runtime — the same shape as the tree
root `0x829E6218` the model-container trace hit, and the same reason
`Ac6Xrefs` returns nothing for it in a `-noanalysis` project.

So no static read of the registry will say which types are registered or what
reader each carries. That route is closed and should not be tried again.

## What is located instead

The registry's API is a cluster of small functions, each forming the address the
same way:

```
0x82337BD8   0x82337C5C   0x82337C84   0x82337CA8   0x82337D08
0x82337D1C   0x82337D3C            addi rX, r11, 0x5b80
```

`0x82337BD8` is the shape of the family:

```
82337bf8  addi r3,r10,0x900     ; a scratch buffer at 0x82686900
82337bfc  li   r5,0x40          ; 0x40 bytes
82337c04  bl   0x82344050       ; copy/format the caller's r3 into it
82337c10  addi r30,r11,0x5b80   ; the registry
82337c18  bl   0x82342d70       ; look r4 up in it
```

A 0x40-byte name buffer and a keyed lookup. That is a resource-by-name API, and
`0x82337C68` — the sole caller of `0x82343010`, the function cycle 1175 pointed
at — is one member of it.

## What this changes for the task

The next read is `0x82342D70`, the lookup, and whichever member of the cluster
**registers** rather than resolves. One of the seven writes into the registry;
`0x821CBE24` (`stw r30,0x5b80(r31)`) is a different structure at the same
displacement and is probably not it, which is worth checking before it is
assumed either way.

This does not get closer to the FHM *layout*, and nothing here contradicts cycle
1175: the layout is still measured, the walker still stays out of the product.
What moved is the search, from "somewhere in the resource system" to seven named
functions over one named table.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed.

## Addendum — the registry is an array of 12-byte entries

`0x82342D70(registry, index)`, read straight after the above:

```
82342d84  addi r3,r30,0x80    ; a sub-object at registry+0x80
82342d88  bl   0x8234ad60     ;   -> r29, a base
82342d94  addi r3,r30,0x4     ; a sub-object at registry+0x04
82342d98  bl   0x8234cc80     ;   -> r3, an offset
82342d9c  mulli r10,r31,0xc   ; index * 12
82342da4  add  r3,r11,r10     ; return base + offset + index*12
```

So it is not a name lookup after all — the second argument is an **index**, and
entries are **twelve bytes** each. The 0x40-byte buffer `0x82337BD8` fills before
calling this is something else the caller wants, not the key.

Twelve bytes is enough for a type code and two pointers, which is the shape a
reader table would have. It is also enough for three of anything else, and the
entries are zero in the image, so that is a note and not a claim.

**The stopping point is here, and deliberately.** What remains for this thread is
whichever of the seven API functions writes an entry — that is what would name
the type codes and their readers. Reading it wants a fresh pass: three of this
session's errors came from taking one more window on a spent context, and the
next step is a register-tracking read of exactly the kind that produced them.
