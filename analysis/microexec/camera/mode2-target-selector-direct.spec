# Positive control for the scalar/direct target branch of canonical PAL
# 0x82262A28.  manager+0x190 == 33 and manager+0xDF4 == 2 make
# 0x8225BF60 return mode 2.  The player target supplies +0xE88/+0xE8C;
# manager+0x4A0 admits it and +0x4A8 selects the direct reads.

function 0x82262A28
case camera-mode2-direct-selector

region manager_base       0xB4000000 zero:0x4
region manager_mode       0xB4000190 bytes:00000021
region manager_identity   0xB400019C bytes:00000000
region manager_gain_360   0xB4000360 bytes:40000000
region manager_gain_364   0xB4000364 bytes:3F000000
region manager_rate_368   0xB4000368 bytes:3E800000
region manager_rotations  0xB40003A0 bytes:000000000000000000000000
region manager_gate_3c4   0xB40003C4 bytes:00000001
region manager_gate_4a0   0xB40004A0 bytes:01
region manager_gate_4a8   0xB40004A8 bytes:00
region manager_state_df0  0xB4000DF0 bytes:00000000
region manager_state_df4  0xB4000DF4 bytes:00000002
region target_base        0xB5000000 zero:0x4
region target_axes        0xB5000E88 bytes:3F0000003E800000
region stack              0xC0000000 zero:0x2000

sp 0xC0001E00
gpr r3 manager_base
gpr r5 target_base
fpr f1 f:1.0

dump manager_rotations
capture gpr:r3 fpr:f1
