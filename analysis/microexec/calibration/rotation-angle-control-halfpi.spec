# The same case as rotation-822a1e80.spec, with the four asserted vector
# semantics enabled. Cycle 1295.
#
# The closure 0x822A1E80 -> {0x820A9B30, 0x820A99F8, 0x82211828} is 271
# instructions and needs exactly four operations the SLEIGH module leaves
# unimplemented: vmrghw, vmrglw, lvlx, vrlimi128. Four, not the image's 70.

function 0x822A1E80
case Rotation@node+0x0

region object 0xB4000000 poison:0x200
region stack  0xC0000000 zero:0x1000

sp  0xC0000E00
gpr r3 object

fpr f1 f:1.5707963
fpr f2 f:0.0
fpr f3 f:0.0

vmx on

capture gpr:r3 fpr:f1
