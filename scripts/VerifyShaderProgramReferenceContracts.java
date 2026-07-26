// Read-only assertions for the AC6 PAL guest shader-program reference layout.
// The pixel reference is singular. The vertex object exposes variants through
// an 8-byte-stride table; only variants 0 and 1 are observed in the qualified
// pre-draw path, and the current capture hook does not identify which one won.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyShaderProgramReferenceContracts extends GhidraScript {
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
        println("AC6_SHADER_PROGRAM_REFERENCE_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Compact/pixel object: descriptor = object + object[0x40], then
        // program = object[0x18] + descriptor[0x28], byte size = descriptor[0x2c].
        require(0x821ed36cL, "lwz", "r10,0x40(r29)");
        require(0x821ed370L, "lwz", "r9,0x18(r29)");
        require(0x821ed374L, "add", "r10,r10,r29");
        require(0x821ed378L, "lwz", "r10,0x28(r10)");
        require(0x821ed37cL, "add", "r10,r10,r9");
        require(0x821ed39cL, "lwz", "r10,0x40(r29)");
        require(0x821ed3a0L, "add", "r10,r10,r29");
        require(0x821ed3a4L, "lwz", "r10,0x2c(r10)");
        require(0x821ed3a8L, "rlwinm", "r10,r10,0x1e,0x2,0x1f");

        // Vertex object: variant table starts at object+0x380 and has an
        // eight-byte stride. The descriptor supplies +0x368/+0x36c while the
        // common program base lives at object+0x20.
        require(0x821ed7f0L, "addi", "r9,r15,0x70");
        require(0x821ed7f8L, "rlwinm", "r8,r9,0x3,0x0,0x1c");
        require(0x821ed808L, "lwz", "r7,0x20(r31)");
        require(0x821ed810L, "lwzx", "r9,r8,r31");
        require(0x821ed818L, "add", "r9,r9,r31");
        require(0x821ed81cL, "lwz", "r9,0x368(r9)");
        require(0x821ed820L, "add", "r9,r9,r7");
        require(0x821ed83cL, "lwzx", "r9,r8,r31");
        require(0x821ed840L, "add", "r9,r9,r31");
        require(0x821ed844L, "lwz", "r9,0x36c(r9)");
        require(0x821ed848L, "rlwinm", "r9,r9,0x1e,0x2,0x1f");

        // The only concrete variant values created by this function are 0
        // (r24 initial value) and 1; -1 means no upload.
        require(0x821ed1f4L, "or", "r15,r11,r11");
        require(0x821ed1fcL, "or", "r19,r11,r11");
        require(0x821ed22cL, "li", "r24,0x0");
        require(0x821ed268L, "li", "r24,0x1");
        require(0x821ed560L, "or", "r19,r24,r24");
        require(0x821ed5c8L, "li", "r15,0x1");
        require(0x821ed5d8L, "or", "r19,r24,r24");

        // All configured draw capture chunks execute before this compiler,
        // so current snapshots can preserve candidates but not the winner.
        require(0x821def18L, "li", "r5,0x4000");
        require(0x821def60L, "bl", "0x821ed1d0");
        require(0x821df300L, "addi", "r6,r31,0x780");
        require(0x821df34cL, "bl", "0x821ed1d0");

        println("AC6_SHADER_PROGRAM_REFERENCE_ASSERTIONS=" + assertions);
    }
}
