# Cycle 1424 — forty percent of the guard is bookkeeping

## Qualification

- **No Ghidra run and no oracle pass.** Product C++ only.
- No product C++ changed; ctest stays **52**. **No contract entry.** This is the
  decision cycle gap 6 asks for, and it deliberately ends without code.

## The gap, measured instead of quoted

The plan states gap 6 as: *"`draw_world_geometry` exige tout le contrat
manifeste, et son premier terme est `!frame.mission_ready`, faux sur le chemin
retail par conception."*

Both halves are true and neither is the size of the problem.
`prepare_geometry_draw` is **one boolean expression of 73 terms**:

| | terms |
|---|---:|
| the framebuffer itself — width, height, colour and depth buffers | **8** |
| **geometry** — buffer ids, counts, strides, byte sizes, bounds finiteness | **26** |
| **manifest bookkeeping** — `mission_id`, `stable_id`, `.valid()`, graph wiring | **29** |
| real render state — formats, depth enable, sample counts | **10** |

**Twenty-nine of seventy-three terms check a declaration, not a drawing.** They
ask whether the drawable, its transform, its material and its texture all carry
the same `mission_id` and `stable_id`, whether each record calls itself valid,
and whether the pass's colour target names the render target that names the
resolve that names `"present"`.

`!frame.mission_ready` is one of those twenty-nine, and `mission_ready` turns out
to have nothing to do with assets at all: `MissionExecution::tick` clears it when
the scenario leaves `Gameplay`, when a wave, AI or sequence dispatch is not due,
or when the mission aborts or completes. It is a **gameplay-state** flag standing
in a **rendering** guard.

## The decision

**A retail-path drawing entry point, taking retail-derived inputs and making no
manifest claims.** Concretely: keep the 8 surface terms and the 26 geometry
terms, pass the 10 render-state values as explicit arguments, and **drop the 29
bookkeeping terms**, because they verify a declaration the retail path does not
make.

The alternative — synthesising `MissionDrawable`, `MissionDrawableTransform`,
`MissionMaterial`, `MissionTextureBinding`, `MissionRenderPass` and the rest so
the existing entry point accepts retail geometry — is refused. Twenty-nine of
those terms would then be checking records written *by the caller that is about
to be checked*, which is a contract validating its own author. The plan says
"synthétiser reviendrait à inventer la preuve" and the count is what that costs:
40% of the guard would become a tautology.

## What is given up, stated rather than glossed

The manifest path guarantees that a drawable's transform, material and texture
belong to the same object, by requiring all four to agree on `stable_id`. The
retail path gives that up.

It does not lose the property. On the retail path those four come out of **one
NDXR container reached by one index** — `MDLP[id] → FHM[j] → NDXR`, every hop
contracted as of cycle 1423 — so they belong to the same object *structurally*
rather than *by assertion*. That is a stronger guarantee than a matching string,
and the entry point should say so where the 29 terms used to be.

## What is not given up

The 10 real render-state terms are not manifest fictions: a shader permutation
that disagrees with the material's, a colour format the shader does not write, a
material asking for depth test against a target with no depth buffer, a sample
count that cannot resolve. Those are genuine and the retail entry point needs
them — as **parameters it is given**, not as records it cross-references.

## And the reason to take this before writing code

`NdxrContainer` opens all 292 containers and exposes `Record`, `Material` and
`TextureRef`. It would have been easy to start converting those into
`MissionDrawable`s, because that is the type the existing entry point wants —
and each one would have been a small, reasonable-looking step toward a
tautology.

Gaps 4 and 5 point the same way: *"le décodeur est verrouillé au manifeste: il
exige un `MissionDrawable` pour recouper"*, and *"`unit.asset = object.category`
jette `model_bindings`"*. Three gaps, one shape — the manifest type is the lingua
franca of the drawing side, and every retail-derived value has to become one to
get through. The decision above is what stops that at the boundary instead of
one file in.

## Not established

- Whether `DecodedGeometry` can be produced from an `NdxrContainer` at all. Its
  26 geometry terms name vertex strides, index sizes and polygon descriptor
  counts; whether the NDXR records carry those is unread.
- Which of the 10 render-state values retail actually uses for world geometry.
  They are currently strings from a manifest and have no retail derivation.
- Whether `rasterize_geometry_draw` — the other half, unread this cycle — has
  bookkeeping of its own.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 30 behaviours
ctest                                 100% passed, 0 failed out of 52
tools/tests                           Ran 79 tests, OK
```

## Next

**Read `NdxrContainer`'s records against `DecodedGeometry`'s 26 terms** and find
out which of them the container can actually answer. That is the question the
decision above defers to, it is a read rather than a design, and it decides
whether the retail entry point takes a `DecodedGeometry` at all or a narrower
thing built from what NDXR really carries.

If the container cannot supply vertex strides and index sizes, the entry point's
input is not `DecodedGeometry` and the decision above needs its shape adjusted
before, not after, it is written.
