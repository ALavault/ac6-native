# 0x8209CC44 is `vspltw v5,v13,0x2`. Cycle 1297 read its p-code writing vs37.
# If the AltiVec file and the VMX128 file are the SAME 128 registers, as they are
# on hardware, then vs37 and vr5 are one storage and capturing either shows the
# splat. Seed v13 as vs45, capture BOTH.
function 0x8209CC44
case alias:vspltw-vs37-vs-vr5
steps 1
region stack 0xC0000000 zero:0x1000
sp 0xC0000E00
vec vs45 0f0e0d0c0b0a09080706050403020100
vmx on
capture vec:vs37 vec:vr5
