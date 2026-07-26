# AC6 cycle 233 — XEX consumer of `ACE_vSpecularParam`

Date: 2026-07-18

## Question

Does the qualified PAL XEX consume the four-float MATE parameter as a named
shader constant, or is that interpretation based only on its suggestive name?

## Target and method

- target: `default.xex`, Xbox 360 PAL;
- SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- canonical Ghidra project: `ghidra-projects/ace-combat-6`;
- tools: Ghidra 12.1.2 headless, `ReferencesTo.java`, bridge decompilation and
  the existing XenonRecomp output;
- mode: read-only, `-noanalysis`; no GUI and no Xenia session.

The flat export incorrectly listed no references for the string.  A fresh
headless query against the canonical project found two `PARAM` references:

```text
0x8209f0d0
0x820fe80c
```

This is a useful cache-staleness warning: the live canonical project outranks
the older `_globals.json` reference list.

## Producer sites

Both references materialize `0x820557d4`, the string
`ACE_vSpecularParam`, and call `0x82334178` with:

- `r3`: parameter-name pointer;
- `r4`: pointer to four contiguous floats.

At the first site, `0x8209f0d0`, the four values are assembled on the stack
before the call.  At the second site, `0x820fe80c`, a guarded initialization
writes four floats to a global vector before passing that vector to the same
callee.

## Named-constant setter

The exact XenonRecomp body and Ghidra decompilation agree on
`0x82334178`:

1. preserve the `r4` source pointer;
2. call `0x82340088` with the `r3` name;
3. call `0x823335c8` with the resulting 32-bit key;
4. if no entry exists, return false;
5. load the destination from entry `+0x08`;
6. load the component count from entry `+0x0c`;
7. copy `component_count * 4` bytes from the preserved source;
8. return true.

`0x82340088` processes the complete zero-terminated name through the MD5
initial state `67452301 efcdab89 98badcfe 10325476`. This corrects the earlier
working description of a "SHA-1-like" state. The MD5 digest of the unprefixed
name `ACE_vSpecularParam` begins with bytes `c8 91 5c 81`; interpreted as the
little-endian first digest word this is `0x815c91c8`. The qualified MATE
contains that exact value in each observed `NU_HASH_ACE_vSpecularParam`
record. This confirms the parameter-name key, but it is not a shader
permutation identifier. `0x823335c8` searches a count-bounded table of 16-byte
records by the resulting key. The copy target and count therefore come from
registered parameter metadata rather than from the MATE parser.

## Confidence and boundary

- **confirmed**: the XEX uploads a vector under the exact name
  `ACE_vSpecularParam`;
- **confirmed**: the upload size is the registered component count multiplied
  by four bytes;
- **cross-match**: the MATE record `NU_ACE_vSpecularParam` supplies four floats
  and is the qualified asset-side counterpart of that XEX name;
- **confirmed**: `NU_HASH_ACE_vSpecularParam=0x815c91c8` is the first MD5
  digest word for `ACE_vSpecularParam`, not a material-to-shader selector;
- **unknown**: the destination Xenos constant register;
- **unknown**: which shader instructions consume the register and the semantic
  role of each component.

Consequently, this cycle qualifies the MATE-to-XEX named upload but still does
not justify a Blinn-Phong, Phong or arbitrary host-side specular equation.

## Commands

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript ReferencesTo.java 0x820557d4 \
  -postScript FindPpcAddressMaterialization.java \
    0x820557d4 0x8205bf28 0x8205d7d0

.venv/bin/ghidra-bridge \
  --config workspaces/ace-combat-6/ghidra-bridge.yaml \
  decompile 0x82334178
```

No human action is required for the next step.  Continue with Xenos shader
reflection/bytecode and the existing XenosRecomp evidence before changing the
native renderer.
