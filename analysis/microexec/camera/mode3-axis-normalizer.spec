# Positive control for the scalar mode-3 axis normalizer at canonical PAL
# 0x8225C680. The two input floats are the first and second axis slots used by
# 0x82262A28. The global pointer chain selects the default 1.25 radius scale.
# No call is stubbed and no instruction semantics are substituted.

function 0x8225C680
case camera-mode3-normalizer-axis-x

region constant_zero       0x8200082C bytes:00000000
region constant_one        0x82001348 bytes:3F800000
region default_scale       0x8206A030 bytes:3FA00000
region global_root_pointer 0x826E4EB4 bytes:B4000000
region manager_slot        0xB402F9A0 bytes:B5000000
region alternate_scale     0xB50004A8 bytes:00
region axes                0xB6000000 bytes:3F80000000000000
region stack               0xC0000000 zero:0x2000

sp 0xC0001E00
gpr r3 axes
gpr r4 axes+4

dump axes
