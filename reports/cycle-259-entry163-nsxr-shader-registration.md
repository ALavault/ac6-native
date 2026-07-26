# AC6 cycle 259 — DATA entry 163 to active NSXR shader registration

## Scope and identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- canonical Ghidra project:
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`;
- method: `analyzeHeadless -readOnly -noanalysis` plus the already qualified
  native DATA/FHM manifest.

This pass corrects the cycle-258 false negative for callers of `0x82343F60`
and qualifies the resource provenance immediately before
`NU::Shader::ShaderContextXenon` construction. It does not assign MATE
technique/pass/permutation semantics.

## Archive visibility

The AC6 archives currently visible at repository root have unchanged hashes:

- `ac6-entry163-instrumentation-evidence-v1.zip`:
  `2b4d94219731ac6b148bf322b8874e358d45e654ca27f337673c2a13e4a9dba8`;
- `ac6_ordered_draw_hook_map_v1.zip`:
  `5c90f515a187447c4f87a5607596f010e96cd0cff5897e39bdb10bff06c3608d`;
- `ac6_material_bind_xex_boundary_v1.zip`:
  `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6`.

They were already qualified in cycles 238, 240 and 258 respectively. No
fourth or replaced AC6 archive is visible in the current tree.

## Corrected active caller

`0x82338500..0x823385D4` is an active shader-container iterator. It:

1. validates the four-byte container signature through `0x82344148`;
2. reads the entry count with `0x823440A0` (`u16be(container+0x0A)`);
3. obtains the first description at `container+0x20` through `0x823440A8`;
4. derives the context key either from the explicit sequential input or from
   `u32be(entry+0)` through `0x823369E0`;
5. calls `0x82343F60` at `0x823385A8` with service `0x828CC280`, category,
   key, current description and flag in `r3..r7`;
6. advances by `entry + u32be(entry+0x18)` through `0x823440B0`.

The signature validator uses the one-entry pointer table at `0x826762A0`.
That table points to `0x820110CC`, whose big-endian word is `0x4E535852`,
ASCII `NSXR`. The active chain is therefore:

```text
NSXR description entry
  -> 32-bit context key
  -> 0x82343F60 registry create/lookup
  -> ShaderContextXenon initializer +0x10
  -> paired vertex/pixel objects
  -> device shader state
  -> draw
```

## Zero-based DATA entry 163 provenance

The enclosing initializer at `0x821D55B0` queues literal `0xA3` in its
resource request list, invokes the qualified DATA/PAC loader chain
`0x821CBFD0 -> 0x82222E98`, and waits through `0x821CC4D0`. That loader indexes
the DATA catalog directly with the queued 16-bit ID; `0xA3` is therefore the
zero-based DATA entry 163, not a one-based label or an unrelated shader key.

After the load completes, the initializer calls the qualified packed-subrecord
accessor `0x82234DD0` on the loaded object and invokes `0x82338500` exactly 50
times. The accessed indices are a fixed permutation of every integer `0..49`:

```text
0,1,2,3,4,5,6,48,46,47,7,8,9,10,13,14,15,16,17,19,20,21,22,
11,12,18,23,44,49,45,24,25,26,27,34,35,36,37,29,30,31,32,39,
40,41,42,28,38,33,43
```

Independent native retail evidence agrees exactly:

- extraction of entry 163 yields a 7,836,352-byte FHM with count `0x32`;
- `reports/fhm-asset-manifest.csv` contains exactly 50 rows for entry 163;
- their paths are `0..49` and all 50 are `NSXR` with prefix `4e535852`.

This is a `confirmed` static/native cross-check from DATA entry 163 to every
top-level NSXR and onward to active ShaderContext registration.

## MATE boundary remains separate

This result does not turn an NSXR key into a MATE identity. The already
qualified first word of a MATE texture subrecord resolves through the texture
registry at `0x828C8100`; the shader contexts here use the separate registry
at `0x828CCB80` and service at `0x828CC280`.

`NO_QUALIFIED_MATERIAL_BIND` therefore remains valid. The next static task is
now narrower: extract the internal ShaderContext keys from the 50 entry-163
NSXR containers, then locate an exact technique/pass/permutation consumer in
one representative MATE family that selects one of those keys. Only after
that exact key join may the path be related to a draw.

## Validation

- `VerifyShaderContainerRegistrationContracts.java`: **78/78 assertions
  pass**, including the 50-call permutation contract;
- `VerifyShaderContextFactoryContracts.java`: **24/24 pass** from cycle 258;
- `VerifyShaderContextXenonContracts.java`: **22/22 pass** from cycle 258;
- native extraction of entry 163: FHM count 50, expanded size 7,836,352;
- native manifest cross-check: 50/50 entry-163 top-level payloads are NSXR;
- cycle-258 report now contains an explicit erratum.

No generated/config/runtime source, Xenia, GUI, VNC or human action was used.
Native AC6 remains **44/44 PASS** from cycle 256; no full CTest rerun was
needed because this pass changes only a read-only verifier and documentation.
