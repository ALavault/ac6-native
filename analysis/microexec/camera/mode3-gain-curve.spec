# Positive control for the four-coefficient scalar curve called by canonical
# PAL 0x82262A28 at 0x82262B3C. No global state or substituted semantics.

function 0x8225D660
case camera-mode3-gain-curve-quarter

region coefficients 0xB5000000 bytes:3F800000400000004080000041000000
region stack        0xC0000000 zero:0x1000

sp 0xC0000E00
gpr r5 coefficients
fpr f1 f:0.25

capture fpr:f1
