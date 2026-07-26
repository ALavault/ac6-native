# Ace Combat 6 — bounded Ghidra and re-agent import plan

Status note: Gates 1 and 2 are complete for project `ace-combat-6`. Project
`ace-combat-6-corrected` and `exports-invalid-raw-pointer/` retain the rejected
experiment that incorrectly treated the XEX memory image as an on-disk PE.

## Gate 1: isolate and verify the XEX loader

1. Expand the downloaded XEXLoaderWV extension into an isolated Ghidra 12.1.2
   extension location; do not replace the PS2 plugin or existing projects.
2. Start with a no-analysis import of `game-files/default.xex` into project
   `ghidra-projects/ace-combat-6`, and verify mapped bytes against XEX memory
   image RVAs rather than PE `PointerToRawData` values.
3. Require the loader to report XEX2 headers, PAL/media metadata, image base,
   entry point, executable sections, imports and `.pdata` function records.
4. Reject the import if mapped ranges overlap unexpectedly, PowerPC code is
   decoded little-endian, the entry point is outside executable memory, or the
   loader silently treats XEX bytes as a raw PE.
5. Save loader/version/hash and import logs before auto-analysis.

## Gate 2: bounded Ghidra analysis

1. Run auto-analysis with a time limit and bounded CPU count.
2. Verify several entry/basic-block sequences as big-endian PowerPC and check
   Xenon/Altivec instructions for undefined or misdecoded opcodes.
3. Export functions, assembly, strings, imports and cross-references through
   `ghidra-bridge`; preserve original addresses as stable identifiers.
4. Seed names from Xbox 360 runtime and graphics imports only after import
   evidence supports them. Do not assign engine-specific names from intuition.
5. Locate `DATA.TBL` parsing by searching for the constants `926`, record stride
   `16`, the two PAC names, big-endian loads, 0x8000-aligned offsets and the four
   selector values. Use that function to prove or correct the table oracle.

## Gate 3: re-agent bootstrap without Ollama

1. Populate `exports/` and its `address_map.json` using `ghidra-bridge.yaml`.
2. Fill the executable `code_range_min/max` only from the successful Ghidra
   mapping, then add them to both configs.
3. Run `re-agent reverse --dry-run --address <verified-entry>` first. Dry-run
   must show the selected address and make no LLM call.
4. Use the configured Codex provider only when budget permits, or explicitly
   select a separately started OpenAI-compatible local server. Do not silently
   start Ollama or change provider.
5. Begin with small leaf functions around table/container I/O, endian helpers,
   D3D setup and viewport/projection state. Keep parity disabled until a host
   source skeleton and objective checks exist.

## Portable reconstruction order

1. **Complete:** the bounds-checked, read-only Windows/Linux `DATA.TBL`/PAC
   inspector validates all 926 rows without extracting payloads. It lives under
   `reconstruction/ace-combat-6/` and passes the supplied retail gate.
2. **Complete:** recovered pi-derived XOR plus custom LZ/raw-DEFLATE/stored
   decoding from XEX call sites. Exact-size validation passes all 926 retail
   payloads and all results expose the `FHM ` inner-container signature.
3. Parse FHM and build deterministic manifests for textures/models/UI/shaders while keeping
   audio and movie packs outside the first graphics tranche.
4. Reconstruct resolution-independent render state and UI layout before asset
   enhancement. Preserve an original-resolution parity mode.
5. Add optional texture/UI remastering as derived assets with source hash,
   transform parameters, alpha/colorspace metadata and reversible manifests.
