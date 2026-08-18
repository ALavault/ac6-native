# The falsifier: EndMode's own statement executes exactly as compiled, and still never calls the completion trigger

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: a new opt-in, write-only
instrument (`AC6_DEMO_FORCE_ENDMODE_AT_TICK`), forcing the tracked
execution-context slot's PC field (`0x2E3DFA08+20`) to EndMode's own
address (`0x2DCB2024`) exactly once, at a chosen tick, then letting
execution run undisturbed — `probe --until frontend`, correctly-timed
START, no oracle. Three runs: a first attempt (non-one-shot, discarded,
see below), a full-length run to `max_ticks=3300` watching native calls,
and a short verification run to `max_ticks=3110` bracketing the PC field
directly to observe the forced statement's own execution.

## The instrument, and a mistake caught before it produced a wrong answer

This is the falsifier this campaign has named as its next step in five
separate reports (`bfc927e1`, `a42271ec`, `42731207`, `704b27b6`, and
implicitly throughout the whole `swg::MovieController` arc) without ever
running it. The first attempt wrote the forced PC on **every** box-call
within the target tick, not once — reasoning that jumping PC far past
`sub_823246C0`'s own local bound would make its inner loop exit on its
own. That reasoning was backward: EndMode's address (`0x2DCB2024`) sits
*before* the idle loop's own entry (`~0x2DCB29A8`) in memory, so the
"far" jump was actually a jump *backward*, which does not break a
`pc < end` bound check — it satisfies it forever. The result was 15,129
identical forced writes in one tick and the outer dispatcher never
regaining control — an inconclusive run, not a real test. Corrected to a
genuine one-shot (a `static bool already_fired` guard), the instrument
behaves exactly as intended: one write, then normal execution resumes.

## The result: the statement runs, word for word, and stops there

Bracketing the PC field (`0x2E3DFA1C`) directly around the forced tick
shows the interpreter's own outer loop (`sub_82325160`) and opcode
`0x2E`'s handler (`sub_823246C0`, `bfc927e1`) picking up the forced value
and running it exactly as any other statement:

```
tick=3100  [normal statement executing: 0x2DCB2130 -> 2148, box-calls as usual]
           <-- forced write lands here: [0x2E3DFA08+20] = 0x2DCB2024 -->
0x2DCB2028  (lr=sub_823246C0, generated_line=18071 -- the box-call PC-advance line)
0x2DCB202C  (same)
0x2DCB2030  (same)
0x2DCB2034  (same)
0x2DCB2038  (same)
0x2DCB203C  (generated_line=18120 -- the loop-exit/continuation line, not
             the advance line: sub_823246C0 has finished this statement)
```

This is `1fcc88b3`'s own validated template, walked live, word for word:
`0x16` (2024→2028), `0x19` (2028→202C), `0x2E` (202C→2030), `0x08`
(2030→2034), `0x00` (2034→2038), then category `0x03` fetched and boxed
at `2038→203C` — the exact six-word statement this campaign identified
as EndMode's own, over a year of static reconstruction and inference,
now genuinely executed rather than inferred. **Category 3 does get
boxed.** Then `sub_823246C0` returns control to the outer dispatcher
(the line-18120 exit), and the interpreter moves straight on to
`0x2DCB203C` — the next statement in the buffer (`0x4D...`, an unrelated
statement) — with no special handling in between.

**Across the full run** (`max_ticks=3300`, 200 ticks past the forced
tick, `outcome=max_ticks`, no trap, no crash), the native-call trace
(`AC6_DEMO_WATCH_SWG_NATIVE_CALL`) shows exactly the same **9** calls
this campaign has always seen in this scenario — the same five at
tick 3001, the same one at tick 2452, the same two at tick 1045, and the
same single `target=0x820EA4A8` at tick 2425 (startup's own, unrelated
call). **No new call to `sub_820EA4A8` — or to anything else — ever
appears after the forced jump.**

## Reading: boxing the category is necessary but not sufficient

This settles the campaign's oldest open question with more precision
than "never reached, cause unknown." EndMode's statement is not
malformed, not guarded by some precondition that silently traps, and not
skipped by the interpreter — it runs cleanly, exactly as written, and
completes by boxing `category=3`. What it does *not* do, by itself, is
call anything. The actual native call (`sub_820EA4A8`, reached
everywhere else in this campaign's evidence only through the marshaller
`sub_820E8F90`, `346255b2`/earlier campaign work) must be issued by a
**separate mechanism** this six-word statement does not contain — most
likely a later, unrelated opcode elsewhere in the script that consumes a
previously-boxed lookup result and performs the actual call, the way
`4ee47a17`'s original six-statement batch showed multiple lookups
happening in sequence before any call was ever issued. Reaching category
3's own statement was never the missing piece; whatever turns "a boxed
lookup result" into "an actual native call" is.

## Not established

- What opcode or statement, if any, in title's own bytecode issues a
  native call following a boxed lookup result — not identified. This is
  the concrete next step: find the call-issuing opcode (a candidate:
  revisit the still-unread slot-8 virtual call from `6d61b5cd`'s Phase 2,
  or the marshaller's own callers, `sub_820E8F90`) and check whether it
  is ever reached with category 3's resolved node as its argument.
  Neither has been checked.
- Whether forcing PC to land mid-buffer (skipping whatever precondition
  a real dispatch through the entry table would set up first) suppressed
  some setup step the call mechanism depends on — the force bypasses
  everything upstream of the jump, by design, but that means a genuine
  "reached via a real dispatch" case is not what was tested.
- The symbol-table node category 3 resolves to, and whether it is ever
  read by anything downstream — not read in this report.

## Gates

Source changed: `swg_native_call_trace.hpp` gained one new opt-in,
write-only, env-var-gated instrument (`AC6_DEMO_FORCE_ENDMODE_AT_TICK`),
one-shot, no effect unless set. Both build trees rebuilt. Native gate JF,
demo `ctest`, and both contract audits verified below before commit.
