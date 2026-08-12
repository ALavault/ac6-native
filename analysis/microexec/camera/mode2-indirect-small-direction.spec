# Positive control for the scalar end of the direction producer in canonical
# PAL 0x82262508. Execution starts after the VMX/VMX128 matrix work. All three
# components are below the retail 2^-16 threshold, so the estimate/refinement
# normalisation is skipped. The retail asin and guarded atan2 helpers execute;
# the exact step bound stops immediately before the next basic block.

function 0x82262738
case camera-mode2-indirect-small-direction
steps 134

region stack_pre  0xC0000000 zero:0xF50
region output     0xC0000F50 poison:0x10
region direction  0xC0000F60 bytes:3680000036000000B680000000000000
region stack_tail 0xC0000F70 zero:0x90

sp 0xC0000F00
gpr r10 output
gpr r11 direction

dump output
capture fpr:f28 fpr:f31
