# Cycle 1156 — the surface rule derived, the decoder at 668 of 692, and a third scope error

## First, the error

Cycle 1155 reported the asset inventory as `1 .mate, 0 .mdlp, 0 .ndxr` and
concluded the material→texture binding was **input-blocked**. That was produced
with `find . -maxdepth 3`. The extracted assets are four to six levels deep.

```
.mate      1
.mdlp      3   (1 distinct, 29 MB, in Mission 01's own bundle)
.ndxr    537   (179 distinct by content)
.ntxr   1053
```

Only the `.mate` count was right. The binding is **not** input-blocked: 179
distinct NDXR models are extracted, and Mission 01's bundle `idx_0009` holds
`001_MDLP.mdlp` — 29 MB declaring 94 entries — beside the very scenario
container the session already reads and the PLAD.

**This is the third scope error of this campaign, and they are all the same
mistake.** Cycle 1130 called the retail archives absent, having looked from a
subdirectory. Cycle 1145 called the parent assignment non-existent, having
filtered its own scan output to `^822`. This cycle capped a `find` at depth 3.
Each time the tool was correct and the question I asked it was too narrow, and
each time the wrong answer was a *negative* — which is the direction that stops
work rather than corrupting it, and therefore the direction nothing downstream
catches.

The section is retracted in place in cycle 1155's report rather than deleted.

## The surface rule is now derived

Cycle 1151 measured `payload = pad32(ceil(W/4)) · pad32(ceil(H/4)) ·
bytes_per_block` from single-level payloads and called it measured, not derived.
Reading `0x821FBE30` closes that.

`0x821FBE30` is **`XGSetTextureHeader`** — it allocates nothing, fills a 0x34-byte
header into the caller's `object+0x1C`, and **returns the byte size the caller
must allocate**. Both texture paths then call the allocator with 4096 alignment
and store the pointer at `object+0x50`.

Two frames down, `0x821DF838` computes a level's surface:

```
821df868  li r25,0x20                    ; X align = 32 blocks
821df870  addi r24,r11,0x1               ; Y align = 32 block-rows
821df8b8  divwu r9,r8,r9                 ; untiled only: 256 / bytesPerBlock
821df8c0  blt cr6,0x821df8c8             ;   floored at 32
821df8e0  andc r11,r9,r11                ; *W = roundUp(W, blockW * xAlign)
821df900  andc r11,r11,r9                ; *H = roundUp(H, blockH * 32)
821df928  rlwinm r10,r10,0x1d,0x3,0x1f   ; pitch bytes = alignedW * bpt / 8
821df92c  mullw r10,r10,r9               ;   * alignedH
821df938  rlwinm r10,r10,0x0,0x0,0x13    ; ROUND UP TO 4096
```

For BC1 the 256-byte-pitch floor gives `256/8 = 32`, and for BC2/BC3 `256/16 =
16` floored to 32 — so **X is 32 blocks either way, tiled or not**, and Y is
always 32 block-rows. That is exactly `pad32 × pad32`.

`0x821DF958` then picks among four base-size formulas. Two of them are
distinguishable on this corpus, and the corpus answers:

| shape | untiled formula | tiled formula | measured payload |
|---|---:|---:|---:|
| 320×240 | 92,160 | **98,304** | 98,304 |
| 120×720 | 92,160 | **98,304** | 98,304 |
| 600×424 | 271,360 | **327,680** | 327,680 |
| 800×720 | 645,120 | **688,128** | 688,128 |

Four for four on the tiled path, `roundUp(pitchBytes · alignedH, 4096)`, and the
two formulas differ by 6–20% so the test discriminates. The untiled branch uses
the *raw* height and skips the 4096 rounding; nothing in this corpus takes it.

## The mip chain, closed for the base level

The file states the geometry. At file `+0x40` and `+0x44`, present only when the
level count exceeds 1 — a single-level header has the ASCII `eXt\0` chunk
signature there instead:

```
payload == word[0x40] + word[0x44]              360 of 360 multi-level wrappers
word[0x40] == the pad32 surface rule            360 of 360
```

The second line is the one worth pausing on. The rule was measured from
**single-level** payloads and the declared value comes from the **file's own
header on multi-level wrappers**. The two derivations share nothing, and they
agree on all 360.

What `word[0x44]` is exactly — the whole chain or the first level — I am not
asserting. For a two-level texture they coincide, and on the 13-level atlas it
is 5,636,096 where level 1 alone would be 4,194,304, so it is chain-shaped; but
"chain-shaped" is not a derivation and the decoder does not need it.

## The decoder

`decode_ntxr_single_level` is now `decode_ntxr_base_level` and decodes the base
level of any block texture, chain or not. It checks both statements of the size
before addressing anything.

```
             before   after
decoded         308     668
refused          384      24     (22 not block format, 2 cube maps)
bad header        0       0
size mismatch     0       0
```

668 of 692, 324 distinct by content, 41 shapes of which 26 non-power-of-two.
Cube maps remain refused: six faces are not addressed.

Levels above zero are still not decoded. `0x8234B268` locates them by summing a
per-level array, and the two readings I have of that array's base disagree with
each other; nothing downstream consumes those levels yet, so it stays out.

## Decided rather than asked

The `refused_mip_chain` field is gone from the artefact rather than kept at
zero. A refusal cause that can no longer fire is a claim that something is still
being excluded, and it is not.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```
