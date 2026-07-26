# AC6 DPL node to DATA.TBL handle chain

Date: 2026-07-15

## Exact chain

The missing join from the second `0x821d2fc0` lookup in `0x820a85e0` is now
recovered.

1. `0x821d1dd0` formats `DPL::[%#x,%#x]`, hashes it with `0x821d0ef0`, finds or
   creates the `0x44`-byte DPL object, and creates a `0x28`-byte pending request.
   The numeric DPL id is written unchanged at request offset `+0x10`; the target
   DPL object is stored at request offset `+0x08`.
2. Request virtual method `0x821d1128` compares that id with `0x39d`. Every id
   below `0x39d` enters the direct DATA.TBL branch. It is passed by address,
   unchanged, to the queue block at `0x821cd130`.
3. `0x821cd130` checks the queued index against the entry count at
   `0x8293ba30`, then records it in the asynchronous archive request.
4. The archive state-machine blocks at `0x821ccdb4`, `0x821cce60`, and
   `0x821ccf6c` load the table root at `0x8293ba38` and use the queued index to
   select the internal halfword tables and `0x44`-byte runtime entry records.
5. When the request completes, `0x821d1128` invokes target virtual slot `+0x40`.
   For the DPL object this is exact function `0x821d1018`, which stores the
   decoded allocation at DPL offset `+0x18` and calls `0x82234c18` on DPL
   offset `+0x20`.
6. `0x82234c18` initializes four child-offset arrays. `0x820a85e0` then uses
   exact function `0x82234dd0` with child index 1, returning
   `decoded_base + first_offset[1]` when the index and offset are valid.

Consequently the campaign selector 1 path is no longer merely a coincidental
numeric match: DPL id 9 is queued as physical DATA.TBL index 9.

## Namespace boundary

The general namespaces still are not identical. `0x821d1128` uses the direct
physical route only for ids `0..0x39c`. Id `0x39d` does not enter that branch,
and larger ids use a separate special-resource path. This explains how valid
DPL ids such as `0x75e` coexist with a 926-entry physical table without
invalidating the direct low-id mapping.

`0x821cc250` establishes the archive globals from decoded `sim:DATA.TBL`:

- `0x8293ba30`: entry count (`buffer+4`);
- `0x8293ba38`: first internal table (`buffer+8`);
- `0x8293ba3c`: computed end pointer;
- `0x8293ba40`: header word at `buffer+0`.

## Native slice and gates

The portable reconstruction adds:

- `function_821d1128_direct_data_table_index`, preserving the exact `<0x39d`
  discriminator;
- bounded `Function82234c18TableView` parsing for both source byte orders;
- bounded `function_82234dd0_offset` / child-tail lookup;
- deterministic mission diagnostic output assigning selector 1 to physical
  entry 9.

Validation: GCC ASan+UBSan 10/10, Clang ASan+UBSan 10/10, and MinGW x64 PE32+
compilation. No i686 target was built.

The re-agent pass on `0x821d1be8` retained objective PASS but rejected the
first proposed C++ class layout because it did not model the exact vtable
`+4` slot. That rejected layout was not copied into the native reconstruction.
The bounded child-table slice uses explicit offsets instead of speculative
class names.

## Linux campaign-scene bridge

`ac6-scene-shell` now accepts the proved campaign route directly:

```sh
SDL_VIDEODRIVER=dummy ac6-scene-shell --campaign-selector 1 \
  DATA.TBL DATA00.PAC DATA01.PAC --smoke
```

For a deterministic native SDL capture through that same resource route,
replace `--smoke` with `--capture-frame CUT_FRAME FRAME.bmp`. The renderer
still refuses every route except the independently proved selector-1 to
entry-9 mapping; a capture is evidence only for that bounded native scene
surface, not campaign or mission activation.

It derives DPL resource ID 9 from selector 1, applies the exact direct-resource
discriminator, validates the complete `DATA.TBL` against both PAC file sizes,
reads only entry 9's selected PAC span, and decodes it through the existing
retail payload codec before opening the bounded Scene group. The shell rejects
all direct resources except entry 9: this renderer has evidence for that Scene
payload, not for an arbitrary campaign resource's shape.

The asset-conditional `ac6-campaign-scene-shell-smoke` CTest executes that
route with the installed retail files. On 2026-07-15 it passed in the Linux GCC
suite (18/18) and Clang ASan/UBSan suite (18/18), reporting two world objects,
115 texture-bound polygons, and `world_renderer=native-partial`.

This closes resource selection and decoding for the first campaign scene only.
It does not establish the XEX mission scene-group activation, unit spawning,
or playable flight state.
