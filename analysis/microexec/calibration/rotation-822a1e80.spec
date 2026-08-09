# The first non-parser shape put through the harness: an object as `this`, three
# float arguments, and writes scattered through the object rather than into a
# record and a buffer.
#
# 0x822A1E80 is the rotation writer MISSION01_LADDER.md names on the placement
# path. It is 40 instructions by .pdata, it takes f1/f2/f3, and it computes
# `r31 = r3 + 0x80` - so whatever it writes, it writes relative to +0x80.
#
# This case exists to measure the instrument, not to establish the function.

function 0x822A1E80
case Rotation@node+0x0

region object 0xB4000000 poison:0x200
region stack  0xC0000000 zero:0x1000

sp  0xC0000E00
gpr r3 object

# The three angles, distinct and exactly representable, so a written field can
# be traced back to the argument it came from rather than guessed.
fpr f1 f:0.25
fpr f2 f:0.5
fpr f3 f:0.75

capture gpr:r3 fpr:f1
