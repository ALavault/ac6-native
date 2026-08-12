# Guard control for canonical PAL 0x82281198. The tunnel vtable points to the
# positive retail leaf, but tunnel+0x118 bit 1 is clear, so the callback must not
# execute. The bounded retail global-state route then returns zero. No call is
# stubbed and no instruction semantics are supplied.

function 0x82281198
case camera-mode2-tunnel-query-inactive

region global_root_pointer 0x826E4EB4 bytes:B1000000
region global_root_prefix  0xB1000000 zero:0x37030
region global_state_slot   0xB1037030 bytes:822663A8
region tunnel_manager      0xB4000000 bytes:00000000B500000000000001
region tunnel_head         0xB5000000 bytes:B6000000
region tunnel_body         0xB5000004 zero:0x114
region tunnel_flags        0xB5000118 bytes:00000000
region vtable_prefix       0xB6000000 zero:0x12C
region vtable_query        0xB600012C bytes:82266390
region query               0xB7000000 bytes:3F0000003F800000400000003F800000
region stack               0xC0000000 zero:0x4000

sp 0xC0003000
gpr r3 tunnel_manager
gpr r4 query

capture gpr:r3
