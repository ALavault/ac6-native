# Cycle 1231 — the duplicate parser is retired

## What changed

`native_geometry_raster.cpp` parsed the NDXR header itself: declared size, object
count, the four section extents, the object table, the per-object descriptor
counts, and the two block bases. All of it correct, and all of it **citing no
retail address** — which is exactly what cycle 1189 flagged about this file and
what cycle 1203 could not adjudicate.

That block is gone. It now calls `NdxrContainer::Open` and reads the sections
from it.

```cpp
const auto container = NdxrContainer::Open(raw.data(), raw.size(), &refusal);
if (!container) return false;
const NdxrSections& sections = container->sections();
const std::size_t polygon_base = sections.first;   // 0x82362190's index block
const std::size_t vertex_base  = sections.second;  //   and its vertex block
```

Per-object descriptor counts come from `container->Record(i)->descriptor_count`
rather than a hand-rolled `be16` at `+0x2A`.

## Why this is more than tidying

There were **two parsers of the same bytes** in one product: one measured and
unaudited, one derived and under contract. They agreed — cycle 1203 checked six
header fields and found six agreements — but agreement between two readers is
worth less than one reader with a citation, and a second copy is a second place
for a future correction to be missed.

Cycles 1212 and 1217 corrected this format twice. Had those corrections landed
while both parsers existed, only one would have moved.

The derived reader also **validates before serving**, which the inline block did
not: it refuses a size mismatch, an unsupported type code, and section extents
that leave the buffer, each with a named cause.

## What did not change

Behaviour. `ctest` is 27 of 27, including the raster tests that consume this
path and the JF capture bundle whose numbers are asserted before its images are
written. The section arithmetic is identical by construction —
`sections.first` *is* `0x30 + [buf+0x10]`, and `sections.second` *is*
`polygon_base + [buf+0x14]`.

## What is still measured in this file

The descriptor loop below the header still reads `+0x00`, `+0x04`, `+0x0C`,
`+0x0E`, `+0x20` inline rather than through `NdxrContainer::Descriptor`. Cycle
1212 derived those fields and cycle 1231 does not consolidate them, for one
reason: the loop also computes running totals, bounds and a vertex-count check
against the drawable, and lifting it would be a behavioural rewrite rather than a
substitution. **It is the obvious next consolidation and it is not done.**

## Gates

```
ctest                    27 tests, all passed (1 skipped, no DISPLAY)
audit --require JF       mission01_final_gate=audit-valid JF=pass open=none
contract_addresses       pass cited=108 supported=108
```

## Not established, stated plainly

- Whether any caller depended on the inline parser accepting something
  `NdxrContainer` refuses. The corpus says no — 537 of 537 open — but the
  product's own fixtures are text-form NDXR and take a different path entirely,
  so the binary path is exercised only by retail bytes.
