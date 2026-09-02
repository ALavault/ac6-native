# AC6 retail NTSC-U/J — documentation only: `XamShowMessageBoxUIEx`'s real signature resolved; stack-argument access convention still unconfirmed (r221)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211/r212/r218/r219/r220. `ctest`/pytest unaffected:
203/203 pytest, 10/10 ctest (last verified at r217, unchanged since no
source was touched).

## Progress since r220: the real 9-argument signature and struct layout are now resolved

Methodically numbered every stack-relative address in
`Function_821F5BA8` (`XamShowMessageBoxUIEx`'s wrapper), instead of the
ad hoc register tracking r220 used. This resolves the real, documented
9-parameter signature completely against this XEX's own real call site
(`0x821f5c30`):

```
INT XamShowMessageBoxUIEx(
    DWORD dwUserIndex,       /* r3 = 0xff */
    LPCWSTR wszTitle,        /* r4 = 0 (NULL) */
    LPCWSTR wszText,         /* r5 = wrapper's own incoming param1 */
    DWORD cButtons,          /* r6 = 1 */
    LPCWSTR *ppwszButtons,   /* r7 = &(local 1-element array holding
                                    the wrapper's own incoming param2,
                                    built at 0x821f5bb8) */
    DWORD dwFocusButton,     /* r8 = 0 */
    DWORD dwFlags,           /* r9 = 1 */
    DWORD *pMessageBoxResult,/* r10 = wrapper's own incoming param3 */
    PXOVERLAPPED pOverlapped /* stack-passed 9th arg, value = a local
                                buffer at the wrapper's own r1+0x68 */
);
```

The `pOverlapped` buffer (`r1+0x68`) is zero-initialized (`Internal`
at `+0x68`, `InternalHigh` at `+0x6c`, both explicitly stored as `0`)
before the call — standard practice for a fresh `OVERLAPPED`. The
generic completion-wait helper r220 traced (`Function_821F50F8`) is
called with a pointer to `pOverlapped+8` (`r1+0x70`), confirming the
XAM convention of repurposing the unused `Offset`/`OffsetHigh` fields
(the real `+8`/`+0xc` positions of a standard `OVERLAPPED`) to carry
the async pending-status and result, rather than the standard
`Internal`/`InternalHigh` fields. Whether the wait-helper is invoked
depends on the initial call's return: exactly `997`
(`ERROR_IO_PENDING`) triggers it; anything else skips straight to
reading the final button-pressed result at `pOverlapped+0x14`
(`r1+0x7c`) — a third position within the same structure (past a
standard 20-byte `OVERLAPPED`, in the `XOVERLAPPED_EXTENSION` region),
confirming the message-box result is written into the caller's own
overlapped buffer, not returned through a separate channel, when the
call completes synchronously.

## Why this still isn't implemented

A safe fix now requires writing the "button 0 pressed" default into
`pOverlapped+0x14` and returning a non-`997` status. Doing that requires
reading the real 9th (stack-passed) argument from within this project's
native stub — i.e., resolving `pOverlapped` itself from the generated
`PPCContext`. Standard PowerPC ABI reasoning suggests the value sits at
`ctx.r1.u32 + 0x54` (matching the caller's own `stw r5,0x54(r1)` just
before the call), but this project has **no existing `render_body` case
that reads a stack-passed argument beyond `r10`** to independently
confirm that convention specifically for how this project's generated
code hands off to a native stub (as opposed to how one recompiled PPC
function calls another). Given this project's own evidence discipline —
measure the instrument before trusting it — implementing on an
ABI-reasoned-but-unverified offset here would be exactly the kind of
untested assumption that class of caution exists to catch. Left
unimplemented.

## Next

1. Before implementing, find (or create) one clean, independently
   confirmable case of a stack-passed argument being read from
   `PPCContext` in this project's own generated code or an existing
   fixed import, to validate the `ctx.r1.u32 + 0x54` convention before
   relying on it here.
2. Once confirmed, the fix itself is now fully specified: return a
   non-`997` status, write `pMessageBoxResult` (`ctx.r10.u32`, if
   nonzero) and `pOverlapped+0x14` both to a default "button 0" value.
3. This same synchronous-completion approach and struct convention would
   likely generalize to `XamUserReadProfileSettings`'s own async
   contract (r219) once validated here.
4. Continue the broader offline-import sweep per r218's bucket catalog
   in the meantime.
5. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
