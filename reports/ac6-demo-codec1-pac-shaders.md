# AC6 demo PAL — codec 1 PAC and shader inventory

## Result

The 40 compressed demo entries are now decoded deterministically.  Their
storage format is the archive-local AC6 pi-word XOR followed by a raw DEFLATE
stream (`wbits=-15`).  All 40 outputs have the exact expanded size declared by
`DATA.TBL` and start with `FHM 01 01 00 10`.

The important correction is the key ordinal.  It resets for each PAC:

- `DATA00.PAC`: global entries 0..411 use codec indices 0..411;
- `DATA01.PAC`: global entries 412..860 use codec indices 0..448.

Using global entry 412 as the XOR index produces an invalid stream.  Using
archive-local index 0 produces a 1,356,512-byte FHM exactly matching the table.
The same rule yields FHM for all 448 raw entries in `DATA01.PAC`.

## Recursive closure

The decoded corpus contains 40 roots, 4,389 node occurrences and 2,583 unique
nodes.  Of these, 231 unique nodes are FHM containers and 2,352 are leaves.
There are 1,075 shared nodes and no parser note.  A valid empty FHM may be only
20 bytes (`0x14`); the parser previously rejected this observed count-zero
form.

The 49 `NSXR` occurrences contain 3,802 shader occurrences and 1,891 unique
microcodes.  Every microcode was hashed both as stored and after a 32-bit
endian swap.  None matches:

- `099625f3…21e4e3`;
- `4913603d…c98e25`;
- `93488cb9…a0402b`;
- `586168ec…a83cc0`.

The negative result is reproducible: two fresh extractions produced
byte-identical 1,325,892-byte inventories with SHA-256
`e9f90d32d50c1a1694dd26697642550b379fb3c6796a855cdff52d6025416f1e`.

## Validation

- source XEX SHA-256: `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`;
- `DATA.TBL`: 861 entries, SHA-256 `0d9e11cf…591eef8`;
- split: 821 raw and 40 compressed;
- compressed ranges: 164,955,693 stored bytes, 320,021,224 expanded bytes;
- focused extractor/FHM/shader tests: 9/9 pass;
- decoded roots: 40/40 FHM;
- recursive parser notes: 0;
- no payload, PAC byte, shader or microcode was added to the repository.

The durable compact receipt is
`analysis/demo/ac6-demo-codec1-pac-shaders-v1.json`.  The full inventory is
regenerated outside the repository with `tools/inventory_ac6_pac_shaders.py`.

## Residual risk and next checkpoint

The four reached shaders are not sourced by the compressed PAC NSXR corpus.
They do, however, have exact static ranges in the qualified PAL basefile;
therefore this negative PAC result must not be interpreted as dynamic shader
synthesis.  The separate static-source receipt qualifies all four ranges and
their offline translations without running the guest.  A new Ghidra import is
not required to identify the codec, enumerate the PAC shaders or validate the
reached shader bytes.
