# XEX function 0x822c3150 record-pool contract

Date: 2026-07-15 (Europe/Paris)

## Recovered contract

The four-instruction PowerPC leaf at `0x822c3150` computes:

```text
pool_base = load_u32(runtime_container + 0x20)
record_index = packed_handle & 0x03ffffff
return pool_base + record_index * 0x40
```

This establishes a 26-bit record index and an exact `0x40`-byte stride. The
upper six handle bits do not participate in the address calculation. The
three-instruction tail wrapper at `0x822c8e48` supplies the packed handle from
wrapper offset `+0x04` and the runtime container from wrapper offset `+0x2c`,
then branches to `0x822c3150`.

The native implementation represents the XEX pool base as a
`std::span<const std::byte>`. It returns one bounded `0x40`-byte record and
rejects indices outside the complete-record capacity. This bounds check is a
native safety adaptation; it is not present in the retail leaf.

The recovered evidence does not yet identify the record contents, pool owner,
serialized source, or whether the pool represents geometry, textures,
materials, GPU state, or another subsystem. Function names therefore retain
their XEX addresses pending the later project-wide semantic naming pass.

## Caller evidence

Ghidra reports two callers of `0x822c3150`: `Function_82223090` and
`FUN_822c8e48`. The latter is the exact tail wrapper described above. Nearby
call chains use `0x822c31e8` to find a keyed runtime record and then
`0x822c8e48` to resolve its packed handle. This adjacency supports the runtime
container contract, but does not prove identity with the serialized NDXR
candidate slots.

## Re-agent verification

One bounded real re-agent round used the configured Codex provider
(`gpt-5.4`), with no Ollama process or provider. The reverser preserved the
`+0x20` load, 26-bit mask, `0x40` multiplier and addition order. The independent
checker returned `VERDICT: PASS` with no issues:

- `reports/logs/round1-20260715-071145-reverser.json`
- `reports/logs/round1-20260715-071145-checker.json`

## Executed gates

- normal Linux CMake/CTest: 8/8 passed;
- ASan/UBSan Linux CMake/CTest: 8/8 passed;
- MinGW-w64 x86-64: produced a PE32+ console executable;
- MinGW-w64 i686: produced a PE32 console executable;
- full retail asset-manifest regeneration: byte-identical to the canonical
  manifest, SHA-256
  `e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa`.

## Vertical-slice blocker

The exact current blocker for `boot -> campaign -> first mission` is not archive
decompression: all 926 archive entries already decode. It is the missing XEX
load-path mapping from campaign/mission selection to the requested `DATA.TBL`
entry and then to the root `Scene`/resource graph instantiated for that mission.
The current manifest exposes 1,293 structurally identified `Scene` payloads,
but provides no proven semantic mapping that selects the first campaign mission.

The next AC6 front should therefore recover the caller chain that translates a
campaign mission identifier into archive entry/member paths and constructs its
runtime resource pools. That path is the shortest evidence-backed bridge from
the validated archive foundation to a native first-mission loader and, in
parallel, to deterministic extraction of mission assets for remaster tooling.
