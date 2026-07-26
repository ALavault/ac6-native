// Read-only assertions for the AC6 PAL texture-descriptor bind boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyTextureDescriptorBind extends GhidraScript {
    private void require(long value, String mnemonic, String fragment)
            throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null)
                .disassemble(address, null);
            instruction = currentProgram.getListing().getInstructionAt(address);
        }
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                !instruction.toString().contains(fragment)) {
            throw new IllegalStateException(address + " expected " + mnemonic +
                " containing " + fragment + " but found " +
                (instruction == null ? "<none>" : instruction));
        }
        println("AC6_TEXTURE_BIND=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // TextureContext vtable method passes object+0x1c as the descriptor.
        require(0x8234fdc4L, "addi", "r5,r11,0x1c");
        require(0x8234fdccL, "bl", "821e1088");

        // Entry contract: r4 is the stage, r5 the descriptor, r6 the dirty bit.
        require(0x821e1094L, "addi", "r11,r4,0xc3e");
        require(0x821e10a0L, "cmplwi", "r5,0x0");
        require(0x821e10b0L, "addi", "r11,r4,0x30");

        // Six descriptor words are copied into the per-stage fetch state.
        require(0x821e10acL, "lwz", "r8,0x20(r5)");
        require(0x821e10b4L, "lwz", "r7,0x30(r5)");
        require(0x821e10c0L, "lwz", "r9,0x2c(r5)");
        require(0x821e10c8L, "lwz", "r26,0x1c(r5)");
        require(0x821e10d0L, "lwz", "r25,0x28(r5)");
        require(0x821e10d4L, "lwz", "r24,0x24(r5)");
        require(0x821e110cL, "stw", "r24,0x8(r11)");
        require(0x821e1138L, "stw", "r10,0x10(r11)");
        require(0x821e113cL, "stw", "r26,0x0(r11)");
        require(0x821e1140L, "stw", "r30,0x4(r11)");
        require(0x821e1144L, "stw", "r25,0xc(r11)");
        require(0x821e1148L, "stw", "r8,0x14(r11)");

        // The device records both the dirty bit and the bound descriptor pointer.
        require(0x821e1178L, "ld", "r8,0x18(r31)");
        require(0x821e1180L, "or", "r8,r8,r6");
        require(0x821e1188L, "std", "r8,0x18(r31)");
        require(0x821e1190L, "stwx", "r5,r28,r31");

        // 0x821e10c8 is inside this routine, after r4 was changed at 0x821e10b8.
        require(0x821e10b8L, "add", "r4,r31,r4");
    }
}
