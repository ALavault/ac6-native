# Same case, but the fourth matrix row at +0xC0 is DEFINED (zero) instead of
# poison. The caller writes only three rows; if the callees read a fourth, they
# have been reading 0xCD bytes as floats in one pass and 0x00 in the other.
function 0x822A1E80
case Rotation-definedtail@halfpi
region object 0xB4000000 poison:0xC0
region tail   0xB40000C0 bytes:00000000000000000000000000000000
region stack  0xC0000000 zero:0x1000
sp 0xC0000E00
gpr r3 object
gpr r0 0
fpr f1 f:1.5707963
fpr f2 f:0.0
fpr f3 f:0.0
vmx on
