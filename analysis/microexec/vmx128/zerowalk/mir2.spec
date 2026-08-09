# The module-implemented control, already known to mirror.
function 0x8209CC44
case bridge:module-mirrored
steps 1
region stack 0xC0000000 zero:0x1000
sp 0xC0000E00
vec vs45 0f0e0d0c0b0a09080706050403020100
vmx on
alias on
capture vec:vs37 vec:vr5
