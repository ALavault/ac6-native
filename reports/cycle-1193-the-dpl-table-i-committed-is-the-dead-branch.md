# Cycle 1193 — the DPL member table I committed an hour ago is the dead branch

## The correction

Cycle 1192 ended by naming the DPL member walk as what a port should walk
instead of FHM:

> `0x821CC4D0` reads a `{first member, member count}` u16 index at `*0x8293BA38`
> into `0x44`-byte member records at `*0x8293BA3C`, names at `+0x04`.

**That code never runs in this build.** It is one side of a format switch, and
the build takes the other side.

The switch is a byte at `0x8293BA18`, and `0x821D5EF8` writes it immediately
before mounting the table:

```
821d61f4  li  r11,0x2
821d61f8  stb r11,-0x45e8(r10)   ; 0x8293BA18 = 2
821d61fc  bl  0x821cc250          ; then load "sim:DATA.TBL"
```

Three functions test that byte, and all three route away from `BA38`/`BA3C`:

| function | test | with the flag at 2 |
|---|---|---|
| `0x821CC250` loader, `821cc2bc` | `cmplwi r11,0x0; bne` | takes `0x821cc300`, **not** the branch writing `BA38`/`BA3C` |
| `0x821CC4D0`, `821cc504` | `cmplwi r11,0x1; bne` | jumps to `0x821cc7c8`, past the whole `0x44` walk |
| `0x821CBFD0`, `821cbfec` | `cmplwi r11,0x0; bne` | takes `0x821cc1d0`, past the `BA38` reads at `821cc03c`/`821cc0c0` |

So `0x8293BA38` and `0x8293BA3C` are written by dead code and read by dead code.
The four addresses in cycle 1192's closing section, and the paragraph I added to
`MISSION01_LADDER.md`, describe a format this executable can parse and this disc
does not use.

I published a structure without checking which branch reached it. The scan that
found the globals returned both branches in one list — `821cc2e4` and `821cc300`
are twenty instructions apart — and I read the first and stopped.

## What actually runs

`0x821CC250` reads `sim:DATA.TBL` (string at `0x82067DC0`, verified byte by byte)
into a heap buffer and, on the flag-2 path, publishes three globals:

```
821cc300  lwz r11,0x0(r31)      ; [file+0x00]
821cc304  stw r11,-0x45d0(r10)  ;   -> 0x8293BA30
821cc30c  addi r11,r31,0x8
821cc310  stw r11,-0x45d4(r10)  ;   -> 0x8293BA2C = file + 8, the record base
821cc318  lwz r11,0x4(r31)      ; [file+0x04]
821cc31c  stw r11,-0x45cc(r10)  ;   -> 0x8293BA34
```

`0x821CBFD0` then indexes that base:

```
821cc1e8  lwz    r7,-0x45d4(r11)       ; base = *0x8293BA2C
821cc1ec  lhz    r11,0x0(r9)           ; a u16 record index; r9 = object+0x128, stride 2
821cc1f0  rlwinm r11,r11,0x4,0x0,0x1f  ; index * 0x10
821cc1f4  add    r11,r11,r7            ; record = base + index * 0x10
821cc1f8  lwz    r10,0xc(r11)          ; record +0x0C, a u32
821cc1fc  lbz    r11,0x1(r11)          ; record +0x01, a u8, compared against 0 and 1
821cc200  addi   r10,r10,0x7ff
821cc208  rlwinm r11,r10,0x0,0x0,0x14  ; (v + 0x7FF) & ~0x7FF
821cc20c  add    r8,r11,r8             ; accumulated
```

The index list is at `object+0x128` as u16s and its length is the byte at
`object+0x139` (`821cbff0`). The function returns a sum, so `+0x0C` is a size
rounded up to a 2048-byte boundary — the DVD sector — not an offset.

**Records are sixteen bytes, and the file agrees independently.**
`game-files/DATA.TBL` is 14,824 bytes; `[+0x00]` is 926 and `[+0x04]` is 2, and
`8 + 926 * 0x10 = 14824` exactly. The header is two dwords, the records are 926
of sixteen, and nothing is left over. That closes the gap cycle 1192 left open —
"a build step sits between them and was not found" — by removing it: no build
step sits between, I was comparing the file against the wrong branch's shape.

The `2` at `[+0x04]` is also the number of PACs `0x821D5EF8` mounts at
`0x821cc338` immediately afterwards, `game:\DATA00.PAC` and `game:\DATA01.PAC`.

## Not established, stated plainly

- Whether `0x8293BA30` (926) is read as a bound anywhere. Four functions load
  `-0x45d0` off some base; I did not confirm any of them resolves to this global.
- The record's `+0x00`, `+0x04` and `+0x08`. `0x821CBFD0` does not read them, and
  the byte values I can see in the file are not a derivation.
- What `+0x01` selects. It is compared against 0 and against 1, and the disc has
  two PACs, which is a fit and not a reading.
- Whether the flag can be anything but 2 at runtime. `0x821D61F4` is the only
  writer found, and it is a literal.

## Decided rather than asked

Cycle 1192's FHM findings stand — the zero-occurrence result and the correction
to cycle 1181's twelve bytes were checked against controls and are unaffected by
this. Only its closing section, on what to walk instead, was wrong. I am
correcting it in place in the ladder rather than reverting the commit, because
the FHM half of that cycle is the part JV depends on.

Nothing is written into the product. Three field meanings out of a sixteen-byte
record is not a reader, and cycle 1158 already recorded what happens when a
structure that fits neatly gets a resolver written against the fit.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
8 + 926 * 0x10                                       ->  14824 = sizeof(DATA.TBL)
```
