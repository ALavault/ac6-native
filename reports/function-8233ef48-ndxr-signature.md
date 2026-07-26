# `FUN_8233ef48` native signature leaf

## Target identity

- Target: AC6 Xbox 360 XEX, exported function `0x8233ef48`.
- Route: `deterministic-fast-path`.
- Static confidence: `confirmed` for the raw comparison and big-endian magic;
  no gameplay semantics are assigned.

## Evidence

The export catalog records a no-call leaf with two direct calls from
`Function_8234CA28`:

```text
bool FUN_8233ef48(int *param_1) { return *param_1 == 0x4e445852; }
```

On Xenon PowerPC's big-endian guest representation, `0x4e445852` is the four
bytes `NDXR`. The native NDXR parser already required that same signature.

## Native boundary and validation

`function_8233ef48_has_ndxr_signature` accepts a bounded byte span and tests
the first four bytes explicitly, preserving the guest-byte order without a
host-endian reinterpret cast. `parse_ndxr` now consumes that exact leaf.

The native test covers the valid signature, a distinct `GIDX` signature, and a
short payload. This verifies the leaf and parser integration only; it is not a
claim that the caller's full resource dispatch is reconstructed.
