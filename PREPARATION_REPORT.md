# Ace Combat 6 (Xbox 360) — static preparation report

Prepared: 2026-07-14 (Europe/Paris)

Scope: read-only archive validation, confined XGD2/XDVDFS extraction, static
format triage and preparation of a separate Ghidra/re-agent workspace. No game,
emulator, XEX code, LLM or Ollama service was executed.

## Source identity and stability

- Source: `/fastdata/lavaulta/auto-re-agent/Ace Combat 6 - Fires of Liberation (Europe) (En,Fr,De,Es,It) (Rev 1).7z`
- Two observations 12 seconds apart: identical size, mtime and inode.
- Size: 7,219,707,823 bytes.
- Type: 7-Zip archive, LZMA method, one non-encrypted block/member.
- SHA-256: `db67238afbe1ec0e5978314a8f0f011851ea969d1786b8fd4146f3b124398dd6`.
- Member: `Ace Combat 6 - Fires of Liberation (Europe) (En,Fr,De,Es,It) (Rev 1).iso`.
- Declared ISO size: 7,835,492,352 bytes; member CRC-32 `22057441`.
- 7-Zip reports a 7,219,707,785-byte physical archive plus a 38-byte tail.
  Listing and extraction both warn about the tail, while extraction verifies
  the member CRC and ends with `Everything is Ok`. The tail is recorded as an
  anomaly; it is not evidence by itself that the member is corrupt.

The ISO was extracted only after the two stability observations and with about
1.2 TiB free. It is confined under `disc-image/`. `file` identifies an ISO
9660/UDF Xbox game disc volume named `XGD2DVD_NTSC`. Generic 7-Zip UDF listing
sees only the DVD-Video compatibility partition and reports a header error; it
must not be used as the game-file inventory oracle.

## XDVDFS inventory

XboxDev `extract-xiso` lists and extracts 13 files from the game partition,
totalling 5,094,128,108 bytes. The root-level extraction is under `game-files/`;
no PAC or media pack was expanded.

| Path | Bytes | Static classification |
|---|---:|---|
| `$SystemUpdate/su20076000_00000000` | 7,303,168 | Xbox system update payload; out of reverse-engineering scope |
| `default.xex` | 7,483,392 | Xbox 360 XEX2 executable, PAL, media ID `0379EFB3` |
| `DATA.TBL` | 14,824 | Big-endian PAC index; initial structure below |
| `DATA00.PAC` | 2,267,086,848 | Primary opaque data container |
| `DATA01.PAC` | 664,141,824 | Secondary opaque data container |
| `bgmpack.bin` | 724,762,624 | RIFF/WAVE container beginning with `XMA2` format data |
| `demopack_eng.bin` | 234,217,472 | RIFF/WAVE media pack; English |
| `demopack_jpn.bin` | 234,455,040 | RIFF/WAVE media pack; Japanese |
| `moviepack.bin` | 348,307,456 | Microsoft ASF media pack |
| `voicepack_eng.bin` | 279,078,912 | RIFF/WAVE voice pack; English |
| `voicepack_jpn.bin` | 327,245,824 | RIFF/WAVE voice pack; Japanese |
| `dummy.bin` | 4 | Placeholder |
| `dummy30.bin` | 30,720 | Unknown placeholder/alignment data |

Hashes of the initial analysis anchors:

- `default.xex`: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- `DATA.TBL`: `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`

The RIFF detections establish outer signatures only. The packs are not assumed
to be single ordinary WAV files, and no codec-wide claim is made beyond the
observed `XMA2` FourCC at the start of `bgmpack.bin`.

## Initial `DATA.TBL` structure oracle

The exact size is `8 + 926 * 16`. Interpreting all words as big-endian gives a
header `(entry_count=926, version_or_bank_count=2)` followed by records:

```text
u32 selector_flags
u32 aligned_offset
u32 stored_size
u32 expanded_size
```

The field meanings are strongly supported but remain provisional until XEX
cross-references identify the loader. All offsets are monotonic inside each
group, every range is inside its candidate PAC, and the two compression classes
are exact:

| selector | rows | candidate PAC | size relation | maximum offset + stored size |
|---:|---:|---|---|---:|
| `0x00010000` | 392 | `DATA00.PAC` | stored < expanded for all rows | `0x8720ca9f` |
| `0x00020000` | 74 | `DATA00.PAC` | stored = expanded for all rows | `0x80778024` |
| `0x01010000` | 408 | `DATA01.PAC` | stored < expanded for all rows | `0x277db9c4` |
| `0x01020000` | 52 | `DATA01.PAC` | stored = expanded for all rows | `0x27958034` |

Container sizes are `DATA00=0x87210000` and `DATA01=0x27960000`, so even the
largest ranges remain bounded. The first selector byte plausibly chooses PAC 0
or 1 and the next flag plausibly chooses compressed or stored data. These two
semantic labels are inference, not yet proven names.

## Tools and prerequisites

Available and verified locally:

- Ghidra 12.1.2 and headless analyzer under `.tools/ghidra_12.1.2_PUBLIC`;
- OpenJDK 21.0.11;
- repository `.venv` with `re-agent 0.1.0` and `ghidra-bridge`;
- Ghidra's built-in big-endian PowerPC/Altivec processor definitions;
- `file`, `xxd`, `strings`, `radare2`/`rabin2`, and `llvm-objdump`;
- XboxDev `extract-xiso` release `build-202505152050`, installed locally under
  `.tools/extract-xiso-build-202505152050`; downloaded ZIP SHA-256
  `982bbfefc9255d51f5348a477d7135d68abf81c0af9600e5728edb1246cfa200`.

Downloaded and later installed locally after the Java-21 rebuild described
below:

- XEXLoaderWV release for Ghidra 12.1,
  `.tools/xexloaderwv-12.1/ghidra_12.1_PUBLIC_20260604_XEXLoaderWV.zip`;
  SHA-256 `4f5e4f817abab4810055e4904093c90879dcb44f329b3ba99ac52bb2a8a1f944`.

The XEX loader was compatibility-tested against local Ghidra 12.1.2 in an
isolated project before its decompile output was accepted. Neither LLVM nor
radare2 recognizes raw encrypted XEX2 as a normal object, so their failure on
`default.xex` is expected and is not an architecture result.

No sudo package is currently required. A native PowerPC GNU binutils package
would be useful only after obtaining a valid decrypted PE/image export; it does
not replace the XEX loader. Xenia is absent and is not required for this static
preparation pass.

## Boundaries

- This target is Xbox 360 Xenon/PowerPC big-endian, not PS2 R5900/MIPS.
- Proprietary inputs remain under the ignored workspace and must not enter
  source control or release archives.
- `default.xex` may be compressed and/or encrypted. Architecture, image base,
  entry point, imports and code ranges must come from a successful XEX-aware
  load, not from guessed offsets.
- Resolution modernization should begin from XEX references to D3D device,
  render-target, viewport/scissor, projection and UI constants. Asset
  super-resolution belongs to a later, independently reversible layer.
- No Ollama process was active during this preparation.

## Executed import validation

The preparation gates were subsequently executed. `xex1tool` validated the
retail signature, decrypted the uncompressed base image, and identified image
base `0x82000000`, entry `0x821f5e90`, and original image size `0xaa0000`.

The downloaded XEXLoaderWV JAR required a Java compatibility rebuild before it
could run: it was compiled as Java class version 67, while Ghidra 12.1.2 uses
Java 21 (class version 65). It was rebuilt from its bundled source without
changing its image mapping.

An apparent RVA/raw-offset discrepancy was investigated with a second import.
That experiment was rejected: Xenia's `ReadImageUncompressed` confirms that an
uncompressed XEX payload is copied directly at the image base and is therefore
already a memory image. Consequently, XEXLoaderWV is correct to use section
virtual addresses here. Treating the payload as an ordinary on-disk PE via
`PointerToRawData` produced invalid control flow.

The rejected raw-pointer experiment is retained as negative evidence under
project `ace-combat-6-corrected` and `exports-invalid-raw-pointer/`. It must not
be used for reverse engineering.

The active import is project `ace-combat-6`. It recovered all 8,246 `.pdata`
functions directly plus 251 import references and 241 thunks; bounded analysis
and bridge export produced 8,824 functions. The entry bytes agree with the XEX
memory image at RVA `0x1f5e90`:

`7d8802a64818d0653be1fe109421fe10600000007d0843787d0843783d4082a6`

The active Java-21 loader JAR SHA-256 is
`1f2f1fd4217cc7247743d158e95cc7436083c7a0569d325fb11004034c2dfee1`.
The active bridge configuration targets only project `ace-combat-6` and writes
only to `exports/`.
