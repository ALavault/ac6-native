# Negative control for canonical PAL 0x82281198. The active tunnel callback is
# the retail leaf 0x822663A8, which executes and returns zero. A bounded global
# fixture makes the later retail state query return zero through that same leaf,
# so the collision fallback is not entered. No call is stubbed.

function 0x82281198
case camera-mode2-tunnel-query-miss

region global_root_pointer 0x826E4EB4 bytes:B1000000
region global_root_prefix  0xB1000000 zero:0x37030
region global_state_slot   0xB1037030 bytes:822663A8
region tunnel_manager      0xB4000000 bytes:00000000B500000000000001
region tunnel_head         0xB5000000 bytes:B6000000
region tunnel_body         0xB5000004 zero:0x114
region tunnel_flags        0xB5000118 bytes:00000002
region vtable_prefix       0xB6000000 zero:0x12C
region vtable_query        0xB600012C bytes:822663A8
region query               0xB7000000 bytes:3F0000003F800000400000003F800000
region stack               0xC0000000 zero:0x4000

sp 0xC0003000
gpr r3 tunnel_manager
gpr r4 query

capture gpr:r3
