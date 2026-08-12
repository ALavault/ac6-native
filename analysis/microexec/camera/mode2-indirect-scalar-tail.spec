# Positive control for the scalar non-mode-3 tail of canonical PAL
# 0x82262508.  Execution starts after the VMX128 direction construction at
# 0x8226283C and stops at the function epilogue.  f28=2 and f31=-1 are bounded
# by manager+0x360=4 and manager+0x364=2, producing +0.5 and -0.5.
function 0x8226283C
case camera-mode2-indirect-scalar-tail
steps 128

region root          0xB4000000 bytes:B4100000
region table         0xB4100000 bytes:B5000000
region manager350    0xB5000350 bytes:3F8000000000000000000000000000004080000040000000
region first_output  0xB6000000 poison:0x4
region second_output 0xB6000010 poison:0x4
region stack         0xC0000000 zero:0x1000

sp 0xC0000F00
gpr r24 second_output
gpr r25 first_output
gpr r26 0xB5000310
gpr r27 0
gpr r29 0xB3FFB14C
gpr r31 0
fpr f28 f:2.0
fpr f30 f:0.0
fpr f31 f:-1.0

stub 0x82262A10 bounded scalar tail return
