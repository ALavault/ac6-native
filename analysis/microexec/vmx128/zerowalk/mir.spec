# 0x820998E8 is `vmrghw v6,v10,v8`, a CALLOTHER instruction whose output p-code
# varnode is vs38. Does `alias on` mirror it to vr6, the way it mirrors the
# module-implemented vspltw? If getResultObjects() reports nothing for a
# CALLOTHER, the bridge is blind to exactly the four operations the harness
# supplies itself.
function 0x820998E8
case bridge:callother-mirrored
steps 1
region stack 0xC0000000 zero:0x1000
sp 0xC0000E00
vec vs42 00112233445566778899aabbccddeeff
vec vs40 0f0e0d0c0b0a09080706050403020100
vmx on
alias on
capture vec:vs38 vec:vr6
