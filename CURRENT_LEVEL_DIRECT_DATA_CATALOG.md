# AC6 current-level direct DATA catalog

The native `ac6-current-level-catalog` tool joins the exact, recovered
current-level callback domain to the mode-1 `Function_821B6E58` DPL table and
then to the direct `Function_821D1128` DATA.TBL branch.

Its input is the retail `DATA.TBL`, `DATA00.PAC`, and `DATA01.PAC`. Before it
exports a route, it parses and validates every physical DATA.TBL range against
both PAC sizes. Each exported record includes the physical bank, storage
class, offset, stored size, and expanded size for the direct resource.

```text
ac6-current-level-catalog DATA.TBL DATA00.PAC DATA01.PAC
```

The closed input range is the one directly proved for event `0x259`: values
`0..7`. The exact mode-1 table maps it to DPL IDs
`[0x33, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f]`; all are below the retail
direct-DATA threshold `0x39d`, so each is its physical DATA.TBL index.

This is an archive catalog only. It does **not** identify a UI control,
campaign progression, a playable mission, or a Scene group. In particular, an
event value has no inferred relation to an entry-9 Scene group.
