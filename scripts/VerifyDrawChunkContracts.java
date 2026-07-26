// Read-only assertions for AC6 PAL draw chunk register recovery.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyDrawChunkContracts extends GhidraScript {
    private int assertions;

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
        assertions++;
        println("AC6_DRAW_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Primitive draw reaches its chunk before r3/r4/r5 are repurposed.
        require(0x821dea14L, "or", "r31,r3,r3");
        require(0x821dea18L, "or", "r24,r5,r5");
        require(0x821dea48L, "ld", "r11,0x10(r31)");
        require(0x821dea68L, "ld", "r4,0x0(r31)");

        // Indexed draw must use saved r31/r25/r21/r22 at 0x821DEF18.
        require(0x821deee4L, "or", "r31,r3,r3");
        require(0x821deee8L, "or", "r25,r4,r4");
        require(0x821deeecL, "or", "r21,r5,r5");
        require(0x821deef0L, "or", "r22,r6,r6");
        require(0x821deef8L, "ld", "r4,0x0(r31)");
        require(0x821def14L, "addi", "r6,r31,0x780");
        require(0x821def18L, "li", "r5,0x4000");

        // Shared indexed draw must use r31/r16/r15/r19/r17.
        require(0x821df2ccL, "or", "r31,r3,r3");
        require(0x821df2d0L, "or", "r16,r4,r4");
        require(0x821df2d4L, "or", "r15,r5,r5");
        require(0x821df2d8L, "or", "r19,r6,r6");
        require(0x821df2dcL, "or", "r17,r7,r7");
        require(0x821df2e0L, "ld", "r4,0x0(r31)");
        require(0x821df300L, "addi", "r6,r31,0x780");

        println("AC6_DRAW_ASSERTIONS=" + assertions);
    }
}
