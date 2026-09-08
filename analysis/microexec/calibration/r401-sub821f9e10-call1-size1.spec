# r401 discriminator (codegen vs. host-stub) for the r399 allocator
# double-issue bug. Pre-state captured LIVE via gdb at the guest entry of
# sub_821F9E10 (r401_capture2.gdb, hit #586 of 587, r4=0 r5=1), the first
# of two back-to-back allocation requests that the running native binary
# returns the SAME address (0x10082ab0) for. Heap dump is the full
# [0x10000000, 0x10200000) guest range at that exact moment, read from the
# live inferior's memory (base_host + guest_addr), covering every address
# named across r369-r398 (sentinel 0x10000180, node 0x10082ac8, etc.).
#
# Compare this case's captured r3 against the live run's POSTCALL
# returned_r3=0x10082ab0 (r401_capture.log). Agreement means Ghidra's
# independent p-code interpretation of the SAME bytes reproduces the bug
# -> points at upstream state (host stub / initial memory), not codegen.
# Disagreement means the compiled C++ (XenonRecomp codegen) diverges from
# the retail instruction semantics Ghidra models -> points at codegen.

function 0x821F9E10
case R401Call1Size1@heap+0x0

region heap 0x10000000 file:/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/recompilation/ace-combat-6-retail/artifacts/retail-us-native-r401-microexec-discriminator/heap2_call1_size1.bin
region stack 0x8fe00000 zero:0x200000

sp 0x8feffac0
gpr r3 heap
gpr r4 0x0
gpr r5 0x1

steps 4000000

# RtlEnterCriticalSection/RtlLeaveCriticalSection are host-implemented
# (native-import-stubs.cpp, real std::recursive_mutex, no PPC bytes for
# EmulatorHelper to run) -- stub both as immediate no-op returns, matching
# the single-invocation logic under test, which does not depend on lock
# contention.
stub 0x823D007C RtlEnterCriticalSection no-op
stub 0x823D008C RtlLeaveCriticalSection no-op

capture gpr:r3

# Added after review: close off any risk of walking into an unstubbed
# import if a different bucket path is taken than expected, and dump the
# heap region's final bytes (file: regions are not write-detected by the
# poison mechanism, so this is the only way to see what actually changed).
stub 0x823D03FC KeGetCurrentProcessType no-op
stub 0x823D03EC KeBugCheckEx no-op
dump heap
