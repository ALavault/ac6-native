# Cycle 1241 — two control styles, and the gap the UV bug lived in

## The question

Cycle 1233 found the product reading texture coordinates four bytes early on
every vertex, and `ctest` passed 27 of 27 before and after. That is a class of
defect, not an incident: **a field offset with no assertion whose outcome depends
on the value at that offset.** Where else does this suite have that shape?

## My first instrument was bad, and it said so immediately

I ranked the twenty-two test files by occurrences of `hash|memcmp|expected_|
golden|float-compare`. **`retail_ndxr_container_tests` scored 0** — the test I
built this session, which carries the rival string base, two rival strides and the
UV plausibility control.

A proxy that scores the strongest test in the suite at zero is measuring its own
vocabulary. Discarded rather than reported.

## The better proxy, and what it shows

Counting mentions of `rival` / `must fail` / `could have failed` /
`discriminating`:

| test | rivals named |
|---|---|
| `retail_ndxr_container_tests` | **61** |
| `retail_scenario_parser_tests` | 1 |
| `ntxr_texture_tests` | 1 |
| the other nineteen | **0** |

But that is not the whole story either, and reading `retail_bin_readers_tests` —
which scored 0 on both proxies and covers **ten readers** — shows why: it asserts
against **p-code snapshots**, comparing native output byte for byte with
microexecution of the retail functions. That is a value-level control of a
different kind, and a strong one: it cannot pass with a wrong field offset,
because the oracle disagrees.

## So the suite has two control styles

| style | where | what it catches |
|---|---|---|
| **snapshot vs. microexecution** | `retail_bin_readers`, `mission01_compare` | any field misread, because an oracle supplies the truth |
| **named rival** | `retail_ndxr_container`, and thinly elsewhere | a specific wrong hypothesis, scored against the right one |

And the geometry and raster path had **neither**. `native_geometry_tests` and
`native_raster_tests` assert counts, strides, bounds and pixel-write totals —
every one of which is invariant under a UV offset. There was no oracle for that
path and no rival named against it.

**That is the gap the bug lived in**, stated structurally rather than as an
anecdote: not "the tests were weak" but "this path had neither of the two control
styles the rest of the suite uses".

## What has changed, and what has not

The container test now carries the named-rival style for the NDXR path, including
the UV control that scores 99.8% against 0.0% for both offsets the product used.

**No snapshot control exists for the geometry path**, and none is proposed here —
`mission01_compare_tests` is the machinery for oracle comparison and spending an
oracle is a milestone decision, not a cycle's.

## Not established, stated plainly

- **I read one of the nineteen zero-scoring tests.** `retail_bin_readers_tests`
  turned out well-controlled by a style my proxies do not see, so the other
  eighteen may be too. **The table above is a keyword census, not an audit**, and
  should not be cited as one — which is precisely the mistake the first proxy
  made and the reason it was discarded.
- Whether any other product reader has an offset with no value-dependent
  assertion. That is the question this cycle asked and did not answer; it names
  the two styles to look for, which is less than an answer and more than nothing.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
twenty-two test files counted; one read
```

No product code changed.
