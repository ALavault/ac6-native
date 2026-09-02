# AC6 retail NTSC-U/J — documentation only: overlapped-completion protocol partially traced, not implemented (r220)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211/r212/r218/r219. `ctest`/pytest unaffected: 203/203
pytest, 10/10 ctest (last verified at r217, unchanged since no source
was touched).

## What was investigated

Following r219's correction (the real blocker for `XamShowMessageBoxUIEx`
and `XamUserReadProfileSettings` is each import's own async-completion
contract, not `XamGetExecutionId`), this cycle traced the generic
completion-wait helper `Function_821F50F8` that `XamShowMessageBoxUIEx`'s
caller invokes when the initial call returns `997`
(`ERROR_IO_PENDING`). Confirmed real evidence: the helper reads
`*pOverlapped` (offset `+0`) and compares it against `0x3e5` (997) — a
pending check — and, once no longer pending, reads offset `+4` as the
real completion result, matching the standard Win32/Xbox 360
`OVERLAPPED`-style layout (`Internal` at `+0`, `InternalHigh` at `+4`).
This confirms the general shape: a synchronous "complete immediately"
implementation (this project's own established pattern for async APIs
with no real underlying device, e.g. r124/r126's `NtReadFile`) is
architecturally possible here in principle, by writing a non-pending
status directly into the caller's overlapped structure instead of
returning `997`.

## Why this was not implemented

Reconstructing exactly *which* stack address holds the real
`pOverlapped` structure at the point `XamShowMessageBoxUIEx` itself is
called required tracking several nested stack-relative addresses
(`&r1+0xcc`, `&r1+0x68`, `&r1+0x70`, `&r1+0x60` all appear as candidate
pointers passed to related calls within a few instructions of each
other) and the exact real parameter count/order of the specific XDK
signature this build uses is not confirmed at that level of precision.
Getting the offset wrong here would mean writing a fabricated "button
pressed" or "settings" value into the wrong stack slot — corrupting
unrelated local state rather than the intended one. Given the real risk
of a wrong-offset write, this cycle stopped short of implementing rather
than guess an offset this project's own evidence discipline requires be
confirmed, not assumed.

## Next

1. A future attempt at this should systematically number every
   stack-relative address used across `Function_821F5BA8` (the
   `XamShowMessageBoxUIEx` wrapper) and `Function_821F50F8` (the
   completion-wait helper) before writing any fix, rather than tracking
   registers ad hoc across several `Read`/disassembly passes as this
   cycle did.
2. If solved, this same synchronous-completion approach would likely
   also unblock `XamUserReadProfileSettings` (r219), since both follow
   the same `997`/overlapped-wait contract.
3. Continue the broader offline-import sweep per r218's bucket catalog
   in the meantime.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
