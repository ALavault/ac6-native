# Cycle 734 — qualified native Vulkan baseline and PAC boundary

Date: 2026-08-04 (Europe/Paris)

## Result

One bounded fresh-profile run reproduced the three required states with one
executable, asset set, route and user-data root:

| checkpoint | present | observation | PNG SHA-256 |
| --- | ---: | --- | --- |
| step 69 | 9754 | textured F-16C and hangar | `7f961709bb6c511ca422707c56a3a490afac624d0d3137480c597974a296220c` |
| step 75 | 10524 | textured terrain, white aircraft | `ac148bc572e9db945e0ea4a3879301fd1f83373c244d046455b76fca822b0778` |
| step 78 | 10649 | black world, green HUD retained | `e052e1d98f3cfa7568381d45259727b3c9ecf27e9d1bc9eb9dade2d4ad977da9` |

Vulkan selected the NVIDIA RTX PRO 4000 Blackwell. Every observed swap used a
non-null 1280x720 guest swap texture. The route continued through pitch, roll,
yaw and throttle captures and ended by its owned timeout. Only its AC6 process
and Xvfb `:97` were stopped.

The white-aircraft draw `VS A1863AF658456A14 / PS D5B4F4A878949938`
bound BC3 base `0x06B30000`, 256x256, tiled, endian 1, mip 0..2. Its captured
constants are in the JSON report. This is direct IssueDraw state; cycle 679
remains the positive control for a non-null host image view.

## Identity

```text
workspace commit / dirty diff: 442c6dbcd5188fb84b056293a3ce7a000bd20669 / 3f175de441fd4c9fb02944ad125e3ac67ef6b5d7685b99617c1a16405b4086f7
runtime commit / dirty diff:   b8b03c7a89dc7f23bcd7844d15aa5080d480bf11 / 669b524e0563eba2626318a8880fe01a99936a9b91d30105422a71e89d82f03b
RexGlue SDK provenance:        31a36d69f796cddd6b3ce545f6c6c332544ab294
executable SHA-256:            685db465cf6f48f4aea328f85cff257009b6a4a8a6cc089b393639aa103522fb
TOML SHA-256:                  fed716e3ff77b50e4866e2a67c5a183f21651f6cf29fdae930091c3fdf1c85b0
default.xex:                   acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL:                      82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA00.PAC:                    c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816
DATA01.PAC:                    eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4
```

Configuration: performance off, debug log, `ac6_unlock_fps=false`, render
capture and backend signature diagnostics on, D3D trace off, scales 1, direct
host resolve off, native 2x MSAA on and invalid fetch constants disallowed.
The source preserves explicit false instead of forcing true. The effective
configuration marker ran before file logging became active, so a direct runtime
readback of the timing cvar is still required in the next run.

GPU: Vulkan 1.4.329, NVIDIA 595.84, vendor `10de`, device `2c34`. No Xenia was
used. Shared Ollama PID 3585823 on `127.0.0.1:11435` was left untouched.

## PAC gate

Dump output was isolated and allowlisted to entries `9,119,165,199,210`.
Stored captures for entries 9, 165, 199 and 210 exactly equal their DATA00.PAC
ranges. Entry 119 was not read by this route. No decoded output was emitted.

The instrumentation defect is localized without changing the frozen corpus:
both generated calls are inside the `0x821CCC54` cache-flush loop. There `r10`
is the output base and `r11` the incrementing flush pointer, not the 16-byte
archive record. The non-generated hook incorrectly treated `r4` as output and
`r11` as record, then returned before publication. GPU causal work remains
paused until the corrected adapter proves runtime/offline decoded equality.

## Validation

```text
focused PAC test: 1/1 passed
AC6 CTest PAL:    8/8 passed
Vulkan build:     passed; AC6_ALLOW_CODEGEN=OFF
follow log:       e743897c7f58e34e58e403d00cbc8f0a60228dec24337bd1d91b57fcf6c31627
runtime log:      68e6cbb22e35d69d01a5311e642d04a05fcbb6111c78cf91d8e137c120e52d7b
```

Residual risk: exact GPU submission ordinals, actual per-frame draw/clear
counts and intermediate reductions are not published yet. Guest capture
counters remain zero and do not prove absent draws.
