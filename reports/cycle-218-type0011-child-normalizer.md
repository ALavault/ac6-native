# AC6 cycle 218 — type-0x0011 child normalizer

## Identity and evidence

- Target: `ac6-xbox360-pal`
- Module: `default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Canonical project: `ghidra-projects/ace-combat-6`
- Retail function: `0x823456d0`
- Caller: `0x82339718`

The headless export closes the normalizer called for a fresh `0x0011` record.
It reads the count at `record+0x14`, then walks a u32-relative child table at
`record+0x20`. For every item it adds the record guest address to the relative
word in place. If the relocated child has u32 kind `0x10` at `+0x08`, it writes
the observed raw guest pointer `0x82678480` at `+0x0c`. Its result is the
unsigned count.

## Native implementation

`normalize_function_823456d0_type_0011` performs that bounded big-endian
mutation. It preflights every table word, child address, kind and target field
before the first write; a truncated or overflowing record is rejected with no
partial relocation. The observed `0x82678480` value remains guest data only:
the host never follows it as a native pointer.

`normalize_function_823456d0_result` supplies the exact count-returning
adapter to `materialize_function_82339718`. The latter now propagates a
normalizer failure without setting its materialized bit. This is a bounded host
safety condition for invalid inputs, not an assertion about malformed XEX
fault behavior.

## Validation

The motion-record regression covers:

- two child relocations;
- exactly one kind-`0x10` pointer replacement;
- retention of a non-`0x10` child pointer;
- full preflight rejection of a truncated second child;
- wrapper delegation, cached repeat, nonmatching tag and null record.

Executed:

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Result: focused **1/1 PASS**, full AC6 **42/42 PASS**. No Xenia, GPU,
generated XenonRecomp edit, retail asset copy, GUI, VNC, or human input was
used.

## Boundary

This does not identify the child format, the raw guest pointer's concrete
runtime type, an aircraft, camera, mission, XAM service, or a flight consumer.
Those remain `needs-types` / `needs-dynamic-evidence`.
