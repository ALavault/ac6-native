# AC6 retail NTSC-U/J — r141/r142's uninitialized-`[r1+88]` mechanism is fully re-confirmed, byte-for-byte, against the current (post-r145/r148/r149) binary (r153)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Purely static — reading the already-generated, gitignored XenonRecomp
C++ (`generated/ppc_recomp.38.cpp` and `.52.cpp`) as literal cross-match
evidence, per this project's own discipline (never a source for native
behaviour by itself, but a faithful decompilation of the real retail
function bodies). No diagnostic added or reverted this cycle — no build, no
`ctest` re-run needed; the previously-verified 9/9 from r152 still applies
since nothing changed.

## What this closes

r152 named the internals of `sub_822834C0` as the next, single-function
trace. This cycle walks that function and every function it calls, down to
the actual stack read, and finds the **exact same mechanism r141/r142
identified in this session's earlier arc** — not a similar one, the same
one, reproduced instruction-for-instruction against the post-r145/r148/r149
binary:

1. `sub_822834C0(category=1, setting=3, r6=4, r7=0)` calls
   `sub_82338388(1,3,4,0)`. r150 measured this returns `1` this run (not
   `0`, as it was at the time of r139-r142). Since `1 >= 0`, execution
   continues (the `< 0` early-return-`0` branch is not taken).
2. It then calls `sub_82338568(1)` (`ppc_recomp.52.cpp:2191`). Its return
   value flows straight back out of `sub_822834C0` (a `sub_82338410(1)`
   side-effect call happens after, but its result is discarded — `r3` is
   restored from `r30` before `sub_822834C0` returns).
3. `sub_82338568` first calls `sub_823382A8(&buf)` where `buf = r1+80`
   (its own 112-byte frame). Reading `sub_823382A8` in full
   (`ppc_recomp.52.cpp:1747`) shows it stores exactly two fields through
   that pointer: offset `+0` and offset `+4` (`stw r11,4(r31)` /
   `stw r11,0(r31)` / `stw r10,4(r31)`). **It never touches offset `+8`**
   (i.e. `r1+88` in `sub_82338568`'s frame).
4. `sub_82338568` then calls `sub_82339D10(handle=1, const, &buf)` — the
   same `&buf` pointer, passed through as `r28`. Reading `sub_82339D10` in
   full (`ppc_recomp.52.cpp:6168`, 73 lines, ends at its own `blr`) shows
   it **never stores through `r28` at all** — no `PPC_STORE_*(r28...)`
   appears anywhere in the function body. Since `handle=1 > 0`, it calls
   `sub_82343F20(const)` (the function r138 already measured live as
   succeeding 5/5 this session — refuting r138's own now-abandoned
   hypothesis about it, but confirming it takes the success branch here
   too), then `sub_82345AB8(...)`, then `sub_82344058(...)` — the function
   r140/r141 already measured live as returning `1,2,3,4,5` sequentially,
   never `0`. `sub_82339D10`'s own return value is exactly
   `sub_82344058`'s return value: a small positive integer.
5. Back in `sub_82338568`: `cmpwi r3,0 / bge` — since the return value from
   step 4 is a small positive integer, the branch is taken to
   `loc_823385C4`: it discards that return value entirely, calls
   `sub_821F4128(handle_from_buf_offset0, -1)` for a side effect only (its
   result is never read), and then loads `r31 = [r1+88]` — **the exact
   stack offset step 3 and step 4 both proved is never written by anything
   in this call chain** — and returns it as `sub_82338568`'s own result.

**This is r141/r142's mechanism exactly**: a genuinely uninitialized stack
slot, read as if it were a real value, propagated up three call frames
(`sub_82338568` → `sub_822834C0` → `sub_821CC288`) and used as an
allocation size. The only thing that changed since r139-r142 is *what
garbage happens to occupy that slot* (`0` then, `1`/`0xfeffffee` now) — a
direct consequence of this session's r145/r148/r149 fixes altering prior
stack-reuse history, exactly as r150 and r151 each independently
hypothesized without walking the chain far enough to prove it.

## What this establishes vs. does not

**Establishes**: the causal chain r130-r142 mapped is structurally identical
in the current binary — not just "still reached" (r151) or "the same call
graph" (r152), but the exact never-initialized-memory mechanism, confirmed
by reading all four functions in the chain to completion. This retires the
main open item from r150/r151/r152's "Next" lists: the DATA.TBL causal
chain's mechanism does not need to be re-derived from scratch: it is
unchanged. Only the numeric garbage value differs, and that is expected,
not a new finding requiring further tracing.

**Does not establish**: why `sub_82338568`'s frame leaves `[r1+88]`
specifically uninitialized in the *original retail binary's own design* —
whether it's a genuine off-by-one/wrong-field retail bug (unlikely, since
this presumably worked on real hardware, so real hardware's stack layout or
ABI convention must differ from what this recompilation reproduces) or an
artifact of this recompilation's own stack-frame handling. That question
was explicitly out of scope for r130-r142 and remains so here — this cycle
only confirms the mechanism is unchanged, not why it exists.

## Decision

This closes the "is the mechanism still the same" question r150 opened and
r151/r152 progressively narrowed. Given r144's already-established
cost-benefit conclusion for this specific sub-thread (traced to completion,
no further actionable native-runtime lever without deeper original-firmware
comparison this project has no oracle for), and given this cycle adds no
new lever either, active tracing of *this* particular leaf is not
re-opened. The remaining named-and-open item from r150 — which caller
reaches `sub_82390880` on the `DATA.TBL` handle — is the one piece of the
post-fix landscape not yet re-examined, and is the natural next target.

## Gates

No native code changed, no diagnostic added or reverted this cycle — purely
static reading of already-generated, already-built files. `ctest` 9/9 stands
unchanged from r152's own build (last rebuild this session). `git status`
unchanged (only pre-existing, unrelated dirty state).

## Next

1. Determine which specific caller of `sub_82390880` reaches it on the
   `DATA.TBL` handle in the current run (r150 item 2, still open across
   three cycles now) — likely the next concrete, boundable piece of work.
2. No further re-tracing of the `sub_821CC288`→`sub_82222D80` allocation
   chain is needed; r150-r153 together fully close it against the current
   binary.
