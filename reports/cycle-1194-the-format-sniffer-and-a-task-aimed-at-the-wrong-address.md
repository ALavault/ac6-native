# Cycle 1194 — the format sniffer, derived; and task 2g was aimed at the wrong address

## The task premise was wrong

Task 2g said "derive the NDXR header parse at `0x82352B88`". `0x82352B88` does
not parse anything. It is a three-slot vtable sequence:

```
82352b9c  lwz r11,0x0(r31)   ; vtable
82352ba0  lwz r11,0x18(r11)  ; slot +0x18
82352ba8  bctrl
82352bb4  lwz r11,0x10(r11)  ; slot +0x10
82352bc8  lwz r11,0x20(r11)  ; slot +0x20
```

Its twin `0x82352BE8` runs `+0x24`, `+0x14`, `+0x1C` — the same three stages in
reverse roles — and `0x82352C48` is a reference-count release: an
`lwarx`/`stwcx.` decrement of `[this+0x04]` that calls `0x82352BE8` when the
count reaches zero. That is a resource lifecycle, and no byte of any file is
read anywhere in it. The address came from an earlier cycle's guess and I carried
it into a task description without checking it.

## What actually recognises a format

`0x8234CA28`, and it is short enough to give whole:

```
8234ca3c  lis  r10,0x4e55
8234ca40  ori  r10,r10,0x5033   ; 0x4E555033 = "NUP3"
8234ca44  lwz  r11,0x0(r31)     ; the file's first dword
8234ca48  cmplw cr6,r11,r10
8234ca4c  bne  cr6,0x8234ca58
8234ca50  lhz  r3,0x8(r31)      ; NUP3: the code is the u16 at +0x08
8234ca54  b    return

8234ca58  bl   0x8233ef48       ; is it NDXR?
8234ca64  beq  0x8234ca7c
8234ca68  lbz  r11,0x8(r31)     ; NDXR: the code is also at +0x08
8234ca6c  lbz  r10,0x9(r31)
8234ca70  rlwinm r11,r11,0x8,0x0,0x1f
8234ca74  add  r3,r11,r10       ;   (b[8] << 8) + b[9]
8234ca78  b    return

8234ca7c  bl   0x8233ef68       ; is it GIDX?
8234ca88  beq  0x8234caa0
8234ca8c  addi r31,r31,0x10     ;   skip 0x10 bytes
8234ca90  bl   0x8233ef48       ;   and re-test for NDXR
8234ca9c  bne  0x8234ca68

8234caa0  li   r3,0x0           ; unrecognised
```

**The type code is a `u16` at file `+0x08` in every recognised case**, and the
function's whole job is to produce it.

**A GIDX file is a `0x10`-byte header in front of an NDXR.** That is derived, not
fitted: the only thing the GIDX arm does is advance the pointer by `0x10` and ask
the NDXR predicate again. It is the wrapper size the texture work has been
carrying as an observation.

## The two predicates, named by their literals rather than by their shape

```
8233ef48  lis r10,0x4e44 / ori r10,r10,0x5852   ; 0x4E445852 = "NDXR"
8233ef68  lis r10,0x4749 / ori r10,r10,0x4458   ; 0x47494458 = "GIDX"
```

Both are the same branchless equality, which is worth writing down once because
it will recur and it does not look like a comparison:

```
lwz    r11,0x0(r3)
subf   r11,r11,r10
cntlzw r11,r11
rlwinm r3,r11,0x1b,0x1f,0x1f
```

`cntlzw` is 32 only when the difference is zero, and `rlwinm ...,27,31,31`
extracts the `0x20` bit of the count into bit 0. So the result is exactly
`magic == literal`, with no branch.

## One thing I nearly over-read

The NUP3 arm uses `lhz r3,0x8(r31)` and the NDXR arm assembles
`(b[8] << 8) + b[9]` by hand. It is tempting to call that an endianness
distinction — NUP3 stored one way, NDXR the other. **It is not.** This target is
big-endian, so `lhz` at `+0x08` and `(b[8] << 8) + b[9]` compute the identical
value. The two arms differ in instruction selection and in nothing else, and any
byte-order claim drawn from this function would be invented.

## Not established, stated plainly

- What the `u16` codes mean. The dispatcher `0x8234CB58` handles 1, 2 and
  `0x200`, and cycle 1192 established that set is closed, but which code a
  Mission 01 NDXR carries is a file measurement and was not taken here.
- The NDXR header past `+0x0A`. Nothing in this function reads it.
- Whether `0x8234CA28` is reached on the Mission 01 load path at all. It is the
  recogniser; that it is *the* recogniser used for model loading is a separate
  claim and I did not trace a caller.

## Decided rather than asked

Task 2g is rewritten to point at `0x8234CB58` and `0x82350AF8` rather than
`0x82352B88`, and to ask for the constructor's field reads rather than for "the
header parse", which presumed a shape.

Nothing is written into the product.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```
