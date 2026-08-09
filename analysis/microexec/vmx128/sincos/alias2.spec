# The converse: seed the VMX128 name, read the AltiVec name.
function 0x8209CC44
case alias:seed-vr13-read-vs37
steps 1
region stack 0xC0000000 zero:0x1000
sp 0xC0000E00
vec vr13 0f0e0d0c0b0a09080706050403020100
vmx on
capture vec:vs37 vec:vr5 vec:vs45
