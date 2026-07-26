# AC6 campaign mission load-path reduction

Date: 2026-07-15 (Europe/Paris)

## Result

The XEX path from the current campaign selector to a resource-registry key is
now exact and has a tested portable implementation. For campaign selector 1,
the retail executable computes:

```text
selector 1 -> DPL resource id 0x09 -> "DPL::[0x9,0]"
           -> CRC 0xfc76b3c6 -> recursive resource-registry lookup
```

This closes the mission-selector-to-DPL-resource portion of the vertical path.
It does **not** yet prove that DPL resource id 9 is identical to physical
`DATA.TBL` record index 9. Consequently no first-mission asset allowlist is
promoted as proven by this pass.

## Exact XEX evidence

### Runtime mode versus mission selector

The ActionScript registration table at `0x82694d30` associates
`GetCurrentMission` with leaf `0x820f7d38`. That leaf only copies the word at
global runtime offset `+0x78`. Writers assign values 0 through 5 to that word,
so it is retained as a neutral runtime-mode field rather than being renamed as
a campaign mission number. A real re-agent/Codex run on this bounded leaf
passed both model and objective checks in
`reports/logs/round1-20260715-072803-checker.json`.

The adjacent `GetCurrentLevel` registration calls `FUN_820943b0` on the
subsystem at global runtime `+0x70`. `FUN_820943b0` selects one of several
mode-specific words and is the current mission/level selector frontier.

Two independent consumers bound the campaign domain:

- `Function_821A6048` increments the value returned by `FUN_820943b0` while it
  is below 15 when runtime mode is 1;
- `Function_82212688` accepts only unsigned values 1 through 15 and falls back
  to 1 otherwise.

This establishes selector 1 as the first bounded campaign mission value.

### Selector to DPL resource

Raw PowerPC disassembly of `Function_820A85E0` is decisive where the Ghidra
decompile lost register dataflow:

```text
0x820a8624  addi r3,r11,0x70
0x820a8628  bl   0x820943b0
0x820a862c  bl   0x821b6e58
0x820a8630  li   r4,0
0x820a8634  bl   0x821d1060
0x820a8638  mr   r4,r3
0x820a8640  bl   0x821d2fc0
```

There is no intervening write to `r3`: the current selector is the first
argument of `0x821b6e58`. For runtime mode 1, `0x821b6e58` uses the 16-word
big-endian table at `0x82065840`:

```text
33 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17
```

Therefore selector 1 maps exactly to DPL resource id 9. Out-of-range values
map to table slot 0 (`0x33`), matching the XEX unsigned bounds check.

`0x821d1060` formats `DPL::[%#x,%#x]` with the mapped id and variant 0, then
passes the complete string to `0x821d0ef0`. That leaf is a non-reflected,
initial-`0xffffffff`, final-complement CRC using polynomial `0x04c11db7`.
`Function_821D2FC0` recursively resolves the resulting hash in the selected
resource registry. The following `FUN_82234dd0(..., 1)` selects subrecord 1
before `FUN_8228e988` constructs the returned bounded view.

## Native translation and gates

The portable implementation is in `include/ac6/mission_resource.h`,
`src/mission_resource.cpp`, and `tests/mission_resource_tests.cpp`. It preserves
all `0x821b6e58` branches and all three retail tables, the exact DPL spelling,
and the exact CRC. Public names remain XEX-address-based until the complete
ownership and resource semantics are recovered.

Executed gates:

- Linux Debug: 9/9 tests pass;
- Linux AddressSanitizer + UndefinedBehaviorSanitizer: 9/9 tests pass;
- MinGW x64: PE32+ x86-64 executable compiled;
- MinGW x86: PE32 i386 executable compiled.

The sanitizer gate also caught and led to correction of an initializer
evaluation-order bug in the first native draft before this report was closed.

## Asset-manifest claim boundary

Corpus record 9 is a strong correlation candidate: the existing recursive
manifest contains 1,111 rows for physical `DATA.TBL` entry 9, including 44
`Scene`, 133 nested `FHM`, 28 `NTXR`, 46 `NFIC`, 28 `NFH`, and strings related
to `m01`. Correlation is not identity proof. An allowlist filtered from those
rows would silently assume that `DPL::[0x9,0]` means physical table record 9,
so this pass deliberately does not publish one as the first-mission manifest.

## Next exact frontier

Trace construction of DPL registry nodes from the archive loader, beginning at
the proven `sim:DATA.TBL` open function `0x821cc250` and its state machine
`0x821cc4d0`. The required next lock is the point where a physical table record
number (or another archive identifier) becomes the numeric first field of
`DPL::[%#x,%#x]`. Once recovered, either physical entry 9's 1,111 rows can be
promoted to the first-mission allowlist or a different record will be selected
and inventoried instead.
