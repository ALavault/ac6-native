821d0fd0 mfspr r12,LR
821d0fd4 stw r12,-0x8(r1)
821d0fd8 std r31,-0x10(r1)
821d0fdc stwu r1,-0x60(r1)
821d0fe0 lwz r11,0x0(r3)
821d0fe4 lwz r31,0x18(r3)
821d0fe8 lwz r11,0x28(r11)
821d0fec mtspr CTR,r11
821d0ff0 bctrl
821d0ff4 or r4,r31,r31
821d0ff8 lwz r3,0x18(r3)
821d0ffc bl 0x82222f20
821d1000 addi r1,r1,0x60
821d1004 lwz r12,-0x8(r1)
821d1008 mtspr LR,r12
821d100c ld r31,-0x10(r1)
821d1010 blr
