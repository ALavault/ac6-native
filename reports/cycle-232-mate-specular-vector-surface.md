# AC6 cycle 232 — bounded MATE specular-vector surface

Date: 2026-07-18

## Question

Can the entry-9 native shell recover an additional authentic MATE material
parameter without assigning unproved Xenos shader semantics?

## Qualified input

- target: AC6 Xbox 360 PAL retail data, campaign selector 1, DPL resource 9,
  `DATA.TBL` entry 9;
- first linked MATE sample:
  `workspaces/ace-combat-6/exports/first-linked-r_f16c.mate`;
- sample SHA-256:
  `dfe0a409829f9c6bc0a41b52ddeedf041c3b408bc9fb3c61eb43cc8aece4460e`;
- sample size: 3,728 bytes.

## Result

`find_mate_named_vector_parameters` scans only parsed material ranges, requires
an exact bounded string match and a four-component record, and decodes each
component from big-endian IEEE-754 words.  It preserves the material index,
record offset, original name and four raw float values.

The retail campaign shell reports:

```text
mate_bound_objects=16
mate_specular_vectors=48
first_mate_specular_vector=[3,0.4,0,20]
```

The existing selector-1 scene CTest now requires that exact tuple, so the
retail data path—not only a synthetic fixture—consumes the decoder.

## Confidence and boundary

- **confirmed**: the named `NU_ACE_vSpecularParam` records exist in the
  qualified MATE payloads and contain four big-endian floats;
- **confirmed**: 48 such records are attached to the 16 MATE-bound objects in
  the selected native frame;
- **unknown**: the exact meaning of each component and the Xenos shader or
  render-state equation that consumes them.

The native shell therefore surfaces the values but does not apply them to its
lighting.  Treating the fourth component as a shininess exponent merely
because values such as 20 or 150 look plausible would turn a useful clue into
an invented contract.

## Validation

```bash
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16

cmake -S reconstruction/ace-combat-6 \
  -B .build/ace-combat-6-clang-probes \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build .build/ace-combat-6-clang-probes -j16
ctest --test-dir .build/ace-combat-6-clang-probes --output-on-failure -j16
```

Results:

- GCC: 42/42 passed;
- Clang plus XenonRecomp probes: 46/46 passed.

No Xenia, VNC, controller, GUI automation or human action was used.

## Next evidence boundary

The binary-qualified named-constant consumer is now closed in cycle 233.  The
remaining boundary is narrower: locate the Xenos constant register and shader
operation before mapping the four values into the native renderer.
