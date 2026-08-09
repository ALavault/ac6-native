function 0x822A1E80
case vr13:156
region object 0xB4000000 poison:0xC0
region tail   0xB40000C0 bytes:00000000000000000000000000000000
region stack  0xC0000000 zero:0x1000
sp 0xC0000E00
gpr r3 object
gpr r0 0
fpr f1 f:0.0
fpr f2 f:0.0
fpr f3 f:0.0
vmx on
alias on
override 0x820a9ab8 vpermwi128
override 0x820a9abc vpermwi128
override 0x820a9bec vpermwi128
override 0x820a9bf0 vpermwi128
override 0x822118e0 vpermwi128
override 0x822118e4 vpermwi128
steps 156
capture vec:vr13 vec:vr12 vec:vr0 vec:vs45 vec:vs44
