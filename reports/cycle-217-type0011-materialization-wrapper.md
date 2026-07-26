# AC6 cycle 217 — bounded type-0x0011 materialization wrapper

## Identity and evidence

- Target: `ac6-xbox360-pal`
- Module: `default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Canonical project: `ghidra-projects/ace-combat-6`
- Retail wrapper: `0x82339718`, called from `0x82118a50`
- Callee boundary: `0x823456d0`

The canonical headless export shows that `0x82339718` is independent of the
already reconstructed `0x8181` relative-table branch. It has four observable
outcomes:

1. null record returns signed status `-30`;
2. a record with bit 31 already set at `+0x18` returns its unsigned `+0x14`
   count without inspecting the tag;
3. a fresh tag `0x0011` record delegates to `0x823456d0` and propagates that
   result;
4. any other fresh tag yields `-1`.

In both fresh-record cases it sets bit `0x80000000` at `record+0x18` after the
optional delegation.

## Native boundary

`materialize_function_82339718` in `ac6/motion_record.h` models the wrapper on
a bounded big-endian guest view. At this cycle the `Function823456d0Normalizer`
callback made the mandatory retail call explicit without inventing the child
format. The callee was subsequently materialized in cycle 218; see
`cycle-218-type0011-child-normalizer.md`.

The tests cover the delegated first call, cached repeat without a callback,
fresh wrong-tag `-1`, null-record `-30`, and the bit-31 write. They do not
claim an aircraft, camera, mission, XAM service, or Xenia runtime behavior.

## Validation

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6/native
cmake --build .build/ace-combat-6/native -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Result: targeted test **1/1 PASS** and full AC6 corpus **42/42 PASS**.
No Xenia, GUI, generated XenonRecomp edit, retail asset copy, GPU experiment,
or human interaction was used.
