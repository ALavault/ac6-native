# AC6 retail NTSC-U/J — documentation only: the entire 29-import networking cluster is dead or already adequate (r231)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 209/209
(unaffected — no source touched).

## What was found

r218's bucket catalog named the networking cluster (~29 `NetDll_*`
imports) as an untouched, out-of-scope-by-size bucket. This cycle checked
every one of the 29 individually with `scripts/FindSymbolReferences.java`
and, for each import reached only through a one-instruction trampoline,
`scripts/ReferencesTo.java` on the trampoline's own address.

**26 of 29 have zero real callers anywhere in this XEX** — not even a
runtime-gated one; the import label (or its sole trampoline) is never
referenced by any `bl`/`bctr` this static pass can see:
`NetDll_WSACleanup`, `NetDll_XNetCleanup`, `NetDll_XNetCreateKey`,
`NetDll_XNetGetTitleXnAddr`, `NetDll_XNetInAddrToXnAddr`,
`NetDll_XNetQosListen`, `NetDll_XNetQosRelease`,
`NetDll_XNetQosServiceLookup`, `NetDll_XNetRandom`,
`NetDll_XNetRegisterKey`, `NetDll_XNetXnAddrToInAddr`, `NetDll_accept`,
`NetDll_bind`, `NetDll_closesocket`, `NetDll_connect`,
`NetDll_getsockname`, `NetDll_getsockopt`, `NetDll_ioctlsocket`,
`NetDll_listen`, `NetDll_recv`, `NetDll_recvfrom`, `NetDll_select`,
`NetDll_send`, `NetDll_sendto`, `NetDll_setsockopt`, `NetDll_shutdown`,
`NetDll_socket`.

**3 have real static callers, all already safe with the current generic
default:**

- `NetDll_WSAGetLastError` — 4 real callers (`Function_8216C8B8`,
  `Function_820AA790`, `Function_820AA818`, `Function_82170928`), all
  network-send/receive error-recovery paths that compare its result
  against specific values (`0x2733`/WSAEWOULDBLOCK-shaped,
  `0x2747`/WSAEISCONN-shaped) to decide whether a failure is benign. The
  generic offline default (`kOfflineStatus = 0xC00000BB`, truncated to
  32 bits as a signed error code) never matches either value, so every
  caller takes its own honest non-benign-failure path.
- `NetDll___WSAFDIsSet` — 3 real callers (`Function_8216C4A0`,
  `Function_8216C588`, `Function_82170928`, the last shared with
  `WSAGetLastError` above), each polling whether one specific socket is
  "ready" after a local `select`-style setup. The generic default never
  equals `1`, so every caller treats the socket as never-ready — the
  same honest, safe outcome a real closed/absent socket would produce.
- `NetDll_XNetQosLookup` — reached through its own tiny wrapper
  (`Function_821FCE30` at `0x821fce30`, which inserts a literal `1` as
  its first argument), itself called from exactly one real site
  (`Function_821621F8`, `0x82162300`). Its result is compared against
  `0` to decide whether QoS lookup succeeded; the generic negative
  default never equals `0`, so the caller takes its own honest failure
  branch.

Critically, **all three live call chains are themselves gated behind a
per-object socket-handle field checked against `-1`** (e.g.
`Function_8216C4A0`'s `if (*(int *)(iVar1 + 8) != -1)`) — a handle that
can only become valid through a prior successful `NetDll_socket`/
`NetDll_connect` call, both of which are in the confirmed-dead list
above. In this qualified build that handle is never anything but its
initial `-1`, so even these three "live" call chains are runtime-dead in
practice, not merely statically reachable.

## Consequence

The entire 29-import networking cluster r218 flagged as an untouched
bucket needs no further work: 26 imports have no caller to derive a
contract from, and the remaining 3 are already safe under the existing
generic default because nothing in this offline-only build ever produces
a live socket for them to act on. This closes the bucket, not merely
narrows it.

## Next

1. Do not re-open the networking cluster without new evidence that a
   socket ever becomes valid in this build (a different game mode/entry
   point) — repeating this trace would be pure waste.
2. Continue the offline-import sweep per r218's catalog for what remains
   genuinely untraced: `_vsnprintf`/`sprintf` (varargs printf engine,
   larger than a single bounded cycle), `XeKeysConsolePrivateKeySign`/
   `XeKeysConsoleSignatureVerification` (permanently out of scope per
   r202), `XamContentCreateEx` (already confirmed dead, r229),
   `VdGetSystemCommandBuffer`/`VdPersistDisplay` (renderer-policy
   territory, out of scope).
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
