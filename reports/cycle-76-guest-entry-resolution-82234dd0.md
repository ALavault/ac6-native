# Cycle 76 — AC6 guest entry resolution FUN_82234DD0

## Evidence

- target: AC6 PAL default.xex, SHA-256
  acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde;
- address: 0x82234DD0;
- headless export: workspaces/ace-combat-6/exports/82234dd0.json;
- callers include campaign/resource initializers and the scene-side callers
  listed by that export.

The XEX leaf compares a signed entry index with table +0x00, reads a nonzero
offset through the guest pointer at table +0x0c using address
(index * 4 + table_address), then returns table +0x04 plus that offset.

## Correction

The previous candidate was not promoted: its host-width pointer arithmetic
would differ from the XEX when the 32-bit guest calculation wraps. The native
guest resolver now computes the entry effective address in uint32_t, reads a
bounded big-endian guest snapshot, and returns a uint32_t guest address.
Unavailable guest memory is a bounded failure, never a host-pointer
dereference.

This is a guest-address primitive, not proof that a resolved entry is a scene,
an NDXR record, or a renderable resource. It does not resolve the disputed
historical 0x8226ECB0 traversal identity.

## Validation

The resource-archive test covers a zero offset, a normal entry, an out-of-range
entry, a negative signed index that cannot be read from the bounded snapshot,
and a 0xfffffffc plus 4 wrap to guest address zero. The last case guards the
Xenon 32-bit arithmetic that the previous candidate widened incorrectly.

Executed validation:

    cmake --build .build/ace-combat-6/native -j16 --target ac6-resource-archive-tests
    ctest --test-dir .build/ace-combat-6/native --output-on-failure -R '^ac6-resource-archive-tests$'
    ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
    cmake --install .build/ace-combat-6/native --prefix "$PWD"
    test ! -e bin/bin
    git diff --check

The targeted test passes 1/1 and the full AC6 corpus passes 41/41. The
installation retains the root bin/ layout.
