# AC6 cycle 264 — Linux runtime link and first retail branch frontier

Date: 2026-07-19

## Identity and scope

- Target: `ac6-xbox360-pal`, Xbox 360, not Xbox One.
- Module: `default.xex`.
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Host runtime: Linux x86-64, Clang 21, ReXGlue Vulkan backend.
- No Xenia, VNC, GUI automation or human interaction was used.

## Reversible build environment

The host has the GTK 3 and X11-XCB runtime libraries but not their development
packages. No system package was installed. The required development headers,
pkg-config files and matching runtime link targets were downloaded as 41
Ubuntu packages and extracted under:

```text
.tools/ac6-runtime-dev-sysroot/
```

The normal ReXGlue source set was configured against that local sysroot. This
is the real GTK/Vulkan runtime path, not the `REXGLUE_CODEGEN_ONLY` object
target and not a replacement headless runtime.

## Link fixes

Two source boundaries prevented a normal Linux link after GTK/X11-XCB became
available:

1. `src/main.cpp` referenced the D3D12-only
   `ac6_texture_swaps_dump_enabled` CVar in Vulkan-only builds. Its declaration
   and performance-mode write are now guarded by `REX_HAS_D3D12`.
2. `src/system/xmemory.cpp` linked directly to the game-specific
   `ac6_fix_trails` CVar. The SDK now uses the existing typed registry query,
   `rex::cvar::Query<bool>("ac6_fix_trails")`. Runtime behavior is preserved
   when the graphics flag is registered; the standalone code generator no
   longer requires an application symbol.

The resulting `ac6recomp` is a dynamically linked x86-64 Linux ELF. `ldd`
resolves GTK 3, GDK 3, X11-XCB and the Vulkan-capable runtime dependency set.

## Bounded runtime observations

An empty game directory reaches the expected fail-closed boundary:

```text
File not found: game:\default.xex
Runtime::LoadXexImage: Failed to load module, status C000000F
```

The qualified retail directory initially reached:

```text
Unresolved branch from 0x821F8AE0 to 0x821F89F4
```

Headless Ghidra and the generated control flow showed that configured entries
`0x821F8A00` and `0x821F8B38` split the real functions beginning at
`0x821F89C0` and `0x821F8AF8`. Neither artificial entry had an incoming
reference. Removing only those TOML entries and regenerating eliminated that
fatal branch without editing generated C++.

The next run reached:

```text
Unresolved branch from 0x821F7C20 to 0x821F7BFC
```

The same bounded check qualified five more internal entries as false starts:
`0x821F7B18`, `0x821F7B28`, `0x821F7C08`, `0x821F7C80` and `0x821F7CE0`.
Ghidra identifies the containing function entries at `0x821F7AE8`,
`0x821F7BC8`, `0x821F7C40` and `0x821F7CA0`; the internal entries have no
external reference. Regeneration removed both the observed `0x821F7C20`
fatal and the adjacent `0x821F7C8C -> 0x821F7C70` fatal.

After those qualified corrections, the runtime progresses to the new exact
frontier:

```text
Unresolved branch from 0x82384AD0 to 0x82384A88
```

This is progress through distinct startup locations, but continuing one crash
at a time would be an inefficient loop. The next pass must audit the remaining
configured function starts systematically against branch targets and real
entry evidence before another regeneration.

## Validation

- Normal ReXGlue Linux runtime: linked successfully.
- Empty-directory smoke: expected `C000000F`, no GTK/Vulkan crash before the
  missing-XEX boundary.
- Retail smoke after two boundary corrections: deterministic fatal at the new
  `0x82384AD0 -> 0x82384A88` frontier.
- XenonRecomp regeneration: completed; generated output was not edited.
- AC6 native build: PASS.
- AC6 CTest: **44/44 PASS**, including the bounded retail shader-cache test.
- Root installation: PASS; `bin/bin` absent.

Primary logs:

- `artifacts/ac6-cycle264-runtime-localdev-guarded-relink.log`
- `artifacts/ac6-cycle264-codegen-after-sdk-cvar-decouple.log`
- `artifacts/ac6-cycle264-codegen-after-821f7b-family-fix.log`
- `artifacts/ac6-cycle264-ghidra-821f89c0-boundaries.log`
- `artifacts/ac6-cycle264-ghidra-false-entry-family.log`
- `artifacts/ac6-cycle264-native-ctest.log`

## Archive status and next action

The newly announced AC6 archive was not visible during this cycle. Repository,
`/mnt`, user download/import locations and temporary locations were checked.
The three visible archives and their hashes remain unchanged. No provenance or
content is inferred for the absent upload.

Next action:

1. ingest and hash the new archive once it appears;
2. compare its function-boundary evidence with the current TOML;
3. audit the `0x82384A88` containing function and a bounded set of other
   configured internal entries in one static pass;
4. regenerate once, then repeat the same bounded retail smoke.

The runtime remains `candidate`, not playable or retail-parity.
