# Positive control for canonical PAL 0x82281198. One active tunnel has flag
# bit 1 set and a synthetic vtable whose +0x12C slot points to the retail leaf
# 0x82266390. The leaf itself executes and returns one; no call is stubbed and
# no instruction semantics are supplied.

function 0x82281198
case camera-mode2-tunnel-query-hit

region tunnel_manager 0xB4000000 bytes:00000000B500000000000001
region tunnel_head    0xB5000000 bytes:B6000000
region tunnel_body    0xB5000004 zero:0x114
region tunnel_flags   0xB5000118 bytes:00000002
region vtable_prefix  0xB6000000 zero:0x12C
region vtable_query   0xB600012C bytes:82266390
region query          0xB7000000 bytes:3F0000003F800000400000003F800000
region stack          0xC0000000 zero:0x4000

sp 0xC0003000
gpr r3 tunnel_manager
gpr r4 query

capture gpr:r3
