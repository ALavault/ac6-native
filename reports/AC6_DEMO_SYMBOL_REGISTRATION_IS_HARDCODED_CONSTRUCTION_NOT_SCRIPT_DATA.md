# Native-function symbol registration is hardcoded C++ construction, not read from script data

## Qualification

AC6 demo PAL, same XEX SHA-256. Static only: walked the comparator's
callers up through the full insertion call chain in generated code
already in the tree, no new run, no source change.

## What this closes

`AC6_DEMO_CORRECTING_CATEGORY_1_IS_ABSENT_TITLE_IS_BOUND_TO_ENDMODE_AS_CATEGORY_3.md`
(`7a565550`) named the map's insertion site as the next concrete step and
floated, explicitly as an unverified inference, that registration might be
"import-driven" from the script's own data — which would have implied an
`EndMode` call site exists somewhere in title's compiled script. This
report reads the insertion chain and finds no such data dependency.

## The call chain, traced by grepping each function's own (single) caller

```
sub_820D4748   (comparator -- a015f994)
  <- sub_820D4DB8   (lower_bound descent -- already known, find path)
  <- sub_820D89D8   (the generic map::insert primitive: descend, check
                      exact-match, else allocate+link a new node via
                      sub_820D6BC0, write the {iterator,bool} result pair)
       <- sub_820D95F8   (a higher-level insert wrapper: extra pre-checks,
                           then delegates to sub_820D89D8 -- ONE caller)
            <- sub_820DC808   (ONE caller)
                 <- sub_820DD5D8   (ONE caller from THIS module; genuinely
                                     called from FOUR sites, all in a
                                     different translation unit)
                      <- sub_820E68E0 (three of the four call sites; each
                                        preceded by: allocate a 68-byte
                                        object via sub_820CE750, construct
                                        it via one of three distinct
                                        constructors -- sub_820E3590,
                                        sub_820E3DA0, sub_820E5DE8 -- then
                                        insert it)
                           <- sub_820E8E18   (no direct `bl` caller found
                                               anywhere -- reached only via
                                               virtual dispatch, same
                                               pattern as everything else
                                               in this subsystem)
                      <- sub_820E8650 (the fourth call site; also reached
                                        only via virtual dispatch, no
                                        direct caller)
```

Every link from the comparator up to `sub_820DD5D8` has **exactly one
direct `bl` caller** — not a fan-out, a single thread. The chain only
branches at `sub_820DD5D8` itself, which has four call sites: three
inline, back-to-back registrations inside `sub_820E68E0` (each its own
allocate-construct-insert triplet, not a loop over external data), and one
inside `sub_820E8650`.

**Independently confirmed by enumerating the true allocator's callers
directly**, rather than trusting the single-caller chain alone: every
insertion into this map must call the node allocator, `sub_820D6BC0`.
Grepping its callers finds exactly **two** — `sub_820D89D8` (the chain
above) and `sub_820D95F8` calling it directly (a with-hint fast path,
skipping the generic wrapper). This is the complete, closed list of every
place this map type is ever inserted into, anywhere in the image — not an
inference from a single-caller walk, an exhaustive enumeration of the one
function every insert must go through. The four other comparator-calling
functions this campaign hadn't characterized (`sub_820DD8A8`,
`sub_820DDEB0`, `sub_820DE320`, `sub_820DE520` — one with five comparator
calls, the shape of another plausible insert) call neither
`sub_820D6BC0` nor `sub_820D89D8`: confirmed read-only (find/count/erase-
search-style operations using the comparator without ever constructing a
node).

A raw data-word scan for the mid-chain functions' own addresses (the same
technique that cleanly proved the doorbell gate's absence) does not give
as clean a signal here: `sub_820D95F8`/`sub_820DC808`/`sub_820D89D8`/
`sub_820D6BC0` each show exactly one raw hit, `sub_820DD5D8` shows six —
plausibly ordinary per-function unwind/relocation metadata (every function
in the image likely has at least one such reference regardless of call
graph), not necessarily an additional indirect caller, but this scan alone
doesn't distinguish the two. **The direct-`bl` chain and the allocator
enumeration are the load-bearing evidence here; the data-word scan is
inconclusive and is not relied on.**

## Reading

**No pointer into any script/movie data blob appears anywhere in this
chain.** Every registration observed is: allocate a fixed-size (68-byte)
object via the general-purpose allocator (`sub_820CE750`, already known
from this campaign's own boxing work), construct it via one of a small,
fixed set of named constructor functions, then insert the result into the
map. The three registrations inside `sub_820E68E0` are hand-sequential
code — three separate inline blocks, each with its own allocate/construct
call, not a loop reading N entries from a table. This is ordinary C++
member initialization, not a script/import loader.

`sub_820E8E18` (registers the 3 base symbols, reached only via vtable —
consistent with a virtual "RegisterCommonNativeCalls"-style base-class
step) and `sub_820E8650` (registers at least 1 more, also vtable-only —
consistent with a per-subclass override adding its own symbols) together
explain the campaign's own observation that title's context (same shared
base vtable as startup's, `0x82007974`) carries more registered categories
than the base alone would: the shared base contributes a fixed core set,
and each concrete subclass's own override contributes the rest. This is a
**structural, compile-time fact about the class hierarchy**, not something
that varies per script *instance* the way a loaded-at-runtime script would.

**This retracts `7a565550`'s "if registration is import-driven" inference,
scoped to what was actually traced: the concrete-category native-function
bindings.** It was stated there as an explicit hypothesis, not a
conclusion, and this reading — a direct `bl` chain plus an exhaustive
enumeration of the map's one allocator's callers — shows the premise
doesn't hold for the registrations traced here: `EndMode`'s presence in
title's table is not evidence of anything read from title's own script
data — it is compiled into every object of this shared base class,
unconditionally, regardless of which concrete subclass or which script
content that object will ever execute. **The inference built on it ("at
least one `EndMode` call site exists somewhere in title's script data")
does not follow for the native-function bindings and should not be
carried forward for them.**

**This does not extend to the whole table.** Title's own dump (`7a565550`)
recorded 33 wildcard-keyed (`category=0xFFFFFFFF`) entries whose `id`
field is a large, pointer-shaped value — not a small integer, and not
something a compile-time constant could produce. Those entries go into
the *same* tree, through the *same* two enumerated insert paths
(`sub_820D89D8`/`sub_820D95F8`), but with a runtime-computed key. **The
table as a whole is not exclusively static** — only the specific
concrete-category native-function bindings this report traced
(`EndMode` and its siblings) are shown to be hardcoded; the wildcard
entries are inserted dynamically, their own registration site and purpose
unread here.

## A reframe worth stating plainly

This bears on the campaign's oldest open item — "the swg/ActionScript
bytecode itself, whose location is still unfound." Every registration
traced here is ordinary compiled C++, not a loader reading an external
format. Combined with `f7c4e68f`'s finding that the AST-node evaluator's
own symbol keys are small integer pairs, not names, and this report's
finding that the symbols themselves are hardcoded per-class, not
data-driven — **the working assumption that "swg" interprets an
externally-authored bytecode/clip format may itself be the thing to
re-examine.** What this campaign has been calling a "script VM" reads,
after this and the prior several reports, more like a fixed, reusable
*internal* dispatch mechanism (a generic typed map + virtual-call
convenience layer) that some other piece of game logic drives by
selecting which symbol to look up on a given tick — not necessarily an
interpreter for authored external content at all. This is not established
here — it is the shape the evidence has been pointing toward across
several reports now, worth stating once directly rather than leaving
implicit.

## Not established

- What actually selects *which* category gets looked up on a given tick
  (the true "sequencing" question `sub_820D4498` and its own callers were
  read for, inconclusively, before this thread started) — not re-pursued
  here. **Concrete next move, live rather than static**: the six lookups
  in title's tick-3001 batch all read `r6` pointing at the same reused
  stack address (`0x7F040548`) with a different `category` word written
  in before each call. Bracketing that address with the existing
  `AC6_DEMO_WATCH_ADDR_LO`/`HI` watcher during the batch window would name,
  via `lr`, whoever writes each category number in sequence — the actual
  statement-sequencing mechanism, not another layer of map internals.
- The exact contents of the three base-class registrations in
  `sub_820E68E0` (which category each of the three constructed objects
  ends up bound to, and which of `sub_820E3590`/`sub_820E3DA0`/
  `sub_820E5DE8` is `EndMode`'s own constructor) — a first attempt at
  matching each constructor's installed base-class pointer against the
  dump's `value0` column did not resolve cleanly in the time spent; not
  decided here.
- **Who assigns the per-instance local index** (`1` for startup's
  `EndMode`, `3` for title's) — plausibly sequential-by-insertion-order
  given the three base registrations run in a fixed sequence per object,
  but this is a guess, not read.
- What `sub_820E8650` (the fourth, per-subclass registration site)
  specifically adds, and whether every `CSwg*`-family subclass has its own
  distinct override or shares startup/title's.
- Whether any OTHER map in the codebase reuses this same comparator/insert
  template for an unrelated key/value pair — the direct-call chain and the
  allocator enumeration found here are specific to what this campaign has
  been tracing, but the functions themselves are generic and could in
  principle be shared; not checked beyond the allocator-caller enumeration
  above.
- The wildcard entries' own registration site and purpose — noted as a
  counterexample to the headline claim above, not traced.

## Gates

No source changed; report-only commit.
