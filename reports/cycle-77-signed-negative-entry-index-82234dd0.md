# Cycle 77 — AC6 signed negative index guard for `FUN_82234DD0`

## Evidence

- target: AC6 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- address: `0x82234DD0`;
- source: headless export `../exports/82234dd0.json` and the bounded native
  guest-memory translation introduced in Cycle 76.

The retail guard is `entry_index < table+0x00` with a signed PPC value. It
does not require `entry_index >= 0`. When a negative index passes that guard,
the subsequent `entry_index * 4 + table+0x0c` computation is guest-pointer
arithmetic and may read a mapped word immediately preceding the table.

## Regression boundary

The native resolver already used the signed comparison and a `uint32_t`
effective guest address. Its test fixture now maps the preceding word:

- guest base: `0x81000000`;
- offset-table guest address: `0x81000004`;
- entry index: `-1`;
- preceding word: `0x00000060`;
- resource blob base: `0x82000000`;
- returned guest address: `0x82000060`.

This prevents a future host-side range check from silently changing the XEX
leaf into `0 <= index && index < count`. An unmapped negative effective
address remains a bounded `nullopt`; the test does not create a host pointer
dereference.

## Validation

The targeted `ac6-resource-archive-tests` test passes **1/1** and the full
AC6 CTest corpus passes **41/41**. Root installation preserves the root
`bin/` layout and `git diff --check` passes.

This validates only the bounded guest-address primitive. It does not prove
payload type, scene activation, mission flow, or Xenia parity.
