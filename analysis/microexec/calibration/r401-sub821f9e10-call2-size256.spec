# r401 discriminator, second of the pair -- see r401-sub821f9e10-call1-size1.spec
# for the full rationale. Pre-state captured LIVE at hit #587 (r4=0 r5=256),
# the very next call after call1, with no free between (confirmed by
# r398/r401's live trace). Live run's POSTCALL returned_r3=0x10082ab0 --
# the SAME address as call1, which is the double-issue bug under test.

function 0x821F9E10
case R401Call2Size256@heap+0x0

region heap 0x10000000 file:/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/recompilation/ace-combat-6-retail/artifacts/retail-us-native-r401-microexec-discriminator/heap2_call2_size256.bin
region stack 0x8fe00000 zero:0x200000

sp 0x8feffac0
gpr r3 heap
gpr r4 0x0
gpr r5 0x100

steps 4000000

stub 0x823D007C RtlEnterCriticalSection no-op
stub 0x823D008C RtlLeaveCriticalSection no-op

capture gpr:r3
