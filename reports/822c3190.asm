822c3190 lwz r11,0x0(r3)
822c3194 lwz r10,0x2c(r3)
822c3198 rlwinm r11,r11,0x0,0x1c,0x1c
822c319c cmplwi cr6,r11,0x0
822c31a0 rlwinm r11,r4,0x6,0x0,0x19
822c31a4 add r3,r11,r10
822c31a8 blr
