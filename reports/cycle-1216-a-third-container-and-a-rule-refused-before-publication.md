# Cycle 1216 — a third container, and a composition rule refused before it was published

## The third container

`0x82338DC0`, the call cycle 1215 left unread between the NDXR load and the NTXR
mount, resolves against a **third** container:

```
82338dcc  lis  r11,-0x7d99
82338dd4  addi r3,r11,0x5b80   ; 0x82675B80
82338ddc  bl   0x82342db0
82338de4  lwz  r11,0x0(r3)
82338de8  lwz  r11,0x5c(r11)   ; vtable slot +0x5C on the result
82338df0  bctrl
```

`0x82342DB0` is **structurally identical to `0x8233EBB0`**, the texture-registry
lookup of cycles 1201–1202, on four points:

```
82342dc0  addi   r3,r3,0x80                  ; +0x80 to reach the manager
82342dc8  bl     0x823432a0                  ; the find
82342dd0  cntlzw r10,r11
82342dd4  rlwinm r3,r10,0x1b,0x1f,0x1f       ; the branchless null test
82342dd8  stw    r11,0x0(r31)                ; *out = result
```

So `0x82675B80` joins `0x828C8100` (textures) and `0x828CCB80` (shaders) as a
ResourceManager-family container, and like both of them it is **zero-filled in
the image** — BSS, populated at runtime.

## The key, read

Both calls in the sweep compose their key the same way:

```
820fb088  add    r10,r10,r28              ; r28 = 0x2808
820fb08c  rlwinm r11,r10,0x9,0x0,0x16     ; (v << 9) & ~0x1FF
820fb090  add    r4,r11,r30               ; | the loop index
820fb094  bl     0x82337c68               ; the NDXR load

820fb098  lwz    r11,0x80(r31)
820fb09c  add    r11,r11,r28
820fb0a0  rlwinm r11,r11,0x9,0x0,0x16
820fb0a4  add    r3,r11,r30
820fb0a8  bl     0x82338dc0               ; the third container
```

**The resource key is `(base << 9) | memberIndex`**, with the member index being
cycle 1215's sweep counter, and the base differing between the two calls —
a register for the loader, `[this+0x80]` for the container lookup.

## The rule I did not publish

The obvious next step was to claim this is how *all* resource ids are formed, and
the GIDX ids of cycles 1207–1210 were sitting right there to be reinterpreted as
`(pack << 9) | index`. The far-side rule from `INSTRUMENT_DISCIPLINE.md` says
query the other side first. Across the 663 distinct GIDX ids in the corpus:

| `id & 0x1FF` | count |
|---|---|
| in `0..0xFF` — consistent with a sweep index | 312 |
| **`>= 0x100`** — impossible for a 0…0xFF index | **351** |

**More than half fail.** The low nine bits of a GIDX id are not a member index,
so the composition read here is local to `0x820FA9C0`'s two calls and is not the
GIDX namespace.

This is the fourth time today the far-side check has changed an answer — cycles
1202, 1209, 1211 — and the **first time it ran before the claim was written down
rather than after.** The rule cost one query and saved a cycle report.

## What that leaves standing

- `0x82675B80` is a third ResourceManager-family container, identified by the
  four-point structural match rather than by a name.
- `0x820FA9C0` keys both its loader call and its container lookup as
  `(base << 9) | memberIndex`.
- Whatever namespace that key belongs to, **it is not GIDX's.**

## Not established, stated plainly

- What `0x82675B80` holds, and what vtable slot `+0x5C` does to it. The container
  is empty in the image and the call is virtual.
- The base term. `r10` at `820fb088` and `[this+0x80]` at `820fb098` are both
  unread, so `0x2808` is a bias on something unknown.
- `0x823432A0`, the find, and whether it is the same `0x10`-byte node walk as
  `0x8234CF68`.
- Whether the two calls' differing bases mean two namespaces or one.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
663 GIDX ids tested against the composition; 351 refute it
```

No product code changed.
