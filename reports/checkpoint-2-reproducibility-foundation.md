# Checkpoint 2 — reproducibility foundation

Date: 2026-08-12

## Result

The PAL retail inputs, canonical Ghidra project, architecture catalog, oracle
commit, SIMDe overlay, boundary/runtime/capture configurations and 13-patch
capture overlay are now inventoried and fail-closed by
`analysis/oracle/ac6-recomp-dcd41b/reproducibility-v1.json`.
`analysis/contracts/global-checkpoint-2-v1.json` records the six structural
lanes over M01–M15. Checkpoint 2 remains open at 0/6 lanes; no mission gate
advanced.

The two pre-existing dirty AC6_recomp checkouts were inspected and preserved
without cleanup. A clean detached reference and a distinct detached runtime at
`dcd41b7457fcac8242f8ef40de83d1719390d5af` are the only qualification
targets. A repository scan found no remaining report claiming that AC6_recomp
is absent.

## Deterministic oracle overlay

The runtime begins with only the canonical boundary configuration
`450d6904…`. The applicator derives runtime `551c38c0…` and capture
`39c3e974…` configurations from sealed policies, checks the PAL XEX and
manifest identities, snapshots every patch path and the Git index, then applies
the overlay. Normal failures and concurrent interference restore file bytes,
modes, symlinks, missing paths and index bytes exactly. Traditional unified
patches without `diff --git` headers are included in the snapshot.

The deterministic replay lane now:

- validates exactly 3,600 sequential bounded TSV rows in Python and C++;
- stages an immutable runtime copy and verifies its pre/post SHA-256;
- requires the runtime's exact successful-load marker;
- defines the initial manager-tick wait and zero/one/multiple `GetState`
  behavior with a compiled micro-test;
- reads the tick atomically, avoiding the former reverse mutex order.

## Build and smoke

The capture host is explicitly configured with system Clang 21.1.8 and the
qualified include-only SIMDe overlay. Clang 20 was never selected as a new
baseline; it came from the upstream preset and was overridden.

```text
codegen functions                 10,478
generated files / bytes           56 / 104,738,907
generated tree SHA-256            34ac6c7e497bd585…
ELF bytes                         93,821,832
ELF SHA-256                       992221ab5f970030…
capture-host CTests               3/3 passed
```

The bounded `SDL_AUDIODRIVER=dummy` smoke initializes the qualified XEX,
NVIDIA Vulkan device and 1280×720 swapchain, then aborts fail-closed at the
known `0x8234530C -> 0x8234524C` frontier. This is not gate evidence.
Codegen's unresolved conditional-branch diagnostics remain explicitly open.

The oracle presents and samples input/state at 30 Hz. Native comparison holds
each sample for two 60 Hz simulation ticks. The attempted oracle presentation
unlock remains disabled after its crash at guest `0x7E980000`.

## Hygiene and residual risks

All owned `/tmp/ac6-*` directories were removed before this lot; the bounded
smoke left no `ac6recomp` process or `xvfb-run.*` directory. Durable
checkouts and overlays under the portfolio `.tools` directory are retained
intentionally.

The oracle route still stops before Mission 01, deterministic end-to-end replay
is not captured, and all six structural lanes remain open. The next named
static boundary is the target-selection block in canonical function
`0x82262A28`, followed by its live player locator producer.
