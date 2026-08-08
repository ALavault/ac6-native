# Cycle 1186 — a gameplay geometry render is reachable, and here is its exact price

## The question

Whether the campaign can push to real gameplay rendering tests. Scoped by
measurement rather than by opinion.

## What is already there

```
NDXR meshes in Mission 01's MDLP                      292
  passing the product's binary ingest gate            292 of 292
```

`src/native_geometry_raster.cpp:384` accepts a raw retail NDXR —
`byte_size >= 0x30`, signature `NDXR`, version byte 0 — and its own comment says
*"a qualified retail slice must be decoded without rewriting its bytes into a
synthetic container."* Every one of Mission 01's 292 meshes satisfies it
unmodified.

Beside it: the rasteriser, the overview camera, 95 container positions, and the
model binding derived end to end from byte `+0x61` to the MDLP entry.

## The one real blocker, and its shape

Getting a mesh **out of** a bundle needs the FHM walker, whose layout is measured
and not derived (cycle 1175), so it stays out of the product. I extracted all 292
in Python for this cycle, which is the marker-lane arrangement: the product does
not carry the measured format, a script feeds it, and no capture is offered as
parity.

## The cost I had not counted

`MissionDrawable` is **fail-closed on a contract**: `has_buffer_contract()`
requires `vertex_count`, `index_count` and `content_hash` to be declared *before*
the loader runs, and the decode is verified against them.

So a diagnostic render cannot simply hand the loader a mesh. It must first read
the NDXR header to declare what it expects — which means the diagnostic lane
needs an NDXR header parse of its own, on top of the FHM walk. That is more
measured format in the harness, and it is the honest price of the shortcut.

This is a good design and it is why the price exists: the product refuses to
decode something nobody has said anything about. Discovering that by trying to
bypass it is the intended outcome.

## What such a render would and would not show

**Would**: that Mission 01's retail geometry decodes and rasterises from the
container alone, at container-derived positions, through a derived binding.

**Would not**: parity. It would be untextured — the texture decoder exists, the
MATE→material→GIDX binding does not — under a chosen camera, at positions cycle
1182 flagged as possibly destinations rather than spawns.

## Decided rather than asked

Not built on this cycle. The remaining scaffolding is an NDXR header parse to
satisfy the contract, and doing it at the end of a long session is how the three
windowed-reading errors happened. The feasibility is now a measurement rather
than a guess, which is what the question asked for.

**The better route is still to derive the FHM reader** and let the geometry into
the product properly, rather than build a second harness beside it. Task 2d holds
that thread at `0x82342D70` and the seven-function API cluster.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
all four gates                                      ->  pass
```

No product code changed. The extracted meshes are retail bytes and stay local.
