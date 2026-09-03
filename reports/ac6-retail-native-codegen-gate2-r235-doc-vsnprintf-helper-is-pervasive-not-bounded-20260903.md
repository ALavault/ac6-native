# AC6 retail NTSC-U/J — documentation only: `_vsnprintf`'s internal consumer is pervasive across real call sites, reaffirms r234 (r235)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 209/209
(unaffected — no source touched).

## What was found

r234 scoped `sprintf`/`_vsnprintf` as needing a real varargs printf
engine and declined to build one within a bounded cycle. This cycle
attempted to narrow that scope further, on the reasoning that this
project's own precedent (bounded, evidence-scoped fixes rather than
general engines) might apply if the actually-*live* specifier set turned
out to be small.

Reading the raw bytes of the 5 distinct format-string addresses already
traced by r234 (`scripts/DumpBytes.java`) confirmed a small, closed set
of specifiers actually used at the **import-level** `sprintf` call sites:
`"%s 0x%02x: "`, `"%08x "`, `"%d%s"`, `"%s:\%s"`, `"%s 0x%08X"` — only
`%s`, `%d`, `%x`/`%X` with zero-padded fixed widths, no other flags,
precision, or length modifiers. That part of the scope genuinely is
small.

But `_vsnprintf`'s two real call sites are not direct per-message calls —
they are inside two small internal wrapper functions,
`Function_821EF4E0` and `Function_821EF458`, that forward *their own*
incoming varargs into a `_vsnprintf` call with a format string supplied
by *their own caller* (a `va_list`-style pointer built by spilling the
wrapper's own `r4`–`r10` onto its local stack, matching this project's
already-established "stack-passed-argument" reading convention from
r222). These two wrappers are not single-purpose — `ReferencesTo.java`
against their own addresses found **20 real callers of
`Function_821EF4E0`** and **8 real callers of `Function_821EF458`**,
spread across at least four distinct functions this cycle touched
(`Function_821EF878`'s large diagnostic/crash-dump path,
`Function_821E9F50`'s save-path construction code already traced by
r202/this session, and others not yet individually decompiled). Each
call site supplies its own literal format string, most of them not yet
individually decoded.

One specific chain was checked end-to-end for reachability:
`Function_821EF878` (a version/crash-diagnostic dump, gated on a
specific status value `0x80050110`) traces back through
`Function_821EFAF0` → `Function_821E6AC8`, and **`Function_821E6AC8` has
zero callers of its own** — that one particular diagnostic-dump chain is
confirmed dead. But `Function_821EF4E0`/`Function_821EF458` have many
*other* real callers outside that dead chain (at least
`Function_821E9F50` and several call sites at `0x821e6428`, `0x821f13a0`
not part of the dead chain), so the pervasive-use finding stands: this
is a general internal formatted-output helper with dozens of real,
individually-distinct format strings across multiple live code paths,
not a small closed set.

## Why this remains out of a bounded cycle's scope

Implementing `_vsnprintf` correctly for *only* the handful of specifiers
already decoded would silently misparse any of the many still-undecoded
format strings reaching it through `Function_821EF4E0`/`Function_821EF458`
— an unrecognized specifier consuming the wrong number of varargs bytes
does not fail safely, it desynchronizes every argument read after it,
producing garbage that looks like real data rather than an honest
failure. That is a *worse* outcome than the current behavior (the
destination buffer is simply never written), and this project's own
evidence discipline explicitly rejects a fix built on an unverified
completeness claim. Decoding and verifying every reachable format string
individually, across at least four call sites' worth of literal-string
tables, is real work but is not bounded to a single cycle — this
reaffirms, with substantially more evidence than r234 had, that
`sprintf`/`_vsnprintf` are correctly out of scope for this sweep's
methodology.

## Consequence

No fix this cycle. This does not reopen or contradict r234 — it replaces
r234's estimate ("a general printf engine is needed") with a
concretely-verified one ("the pervasive internal consumer has dozens of
real, mostly-undecoded call sites across multiple live functions, not a
small closed set"), which is a stronger version of the same conclusion.

## Next

1. If `sprintf`/`_vsnprintf` are ever taken up, the correct starting point
   is enumerating every literal format-string address reaching
   `Function_821EF4E0`/`Function_821EF458` (at minimum the 20 and 8 real
   callers found this cycle) and decoding each one with
   `scripts/DumpBytes.java` before writing any parser, so the parser's
   specifier coverage is verified complete rather than assumed.
2. The offline-import sweep's own conclusion from r234 stands: no
   further bounded candidate remains in r218's catalog.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
