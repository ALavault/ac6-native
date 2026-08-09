function 0x8209CB70
case sincos-bridged:1.0
region out   0xB4000000 poison:0x40
region stack 0xC0000000 zero:0x1000
sp 0xC0000E00
gpr r3 out
gpr r4 out+0x10
gpr r0 0
fpr f1 f:1.0
vmx on
alias on
