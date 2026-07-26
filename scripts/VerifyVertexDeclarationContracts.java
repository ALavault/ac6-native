// Read-only assertions for AC6 PAL index-buffer binding and declaration creation.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyVertexDeclarationContracts extends GhidraScript {
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
        println("AC6_INDEX_BUFFER_CONTRACT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        // Historical filename retained for command compatibility. 0x821DD188
        // is the index-buffer bind, not a vertex-declaration bind. It preserves
        // device/new index buffer in r31/r29 and always publishes r29 at +0x308C.
        require(0x821dd188L, "mfspr", "r12,LR");
        require(0x821dd194L, "or", "r31,r3,r3");
        require(0x821dd198L, "or", "r29,r4,r4");
        require(0x821dd19cL, "lwz", "r30,0x308c(r31)");
        require(0x821dd1a4L, "beq", "0x821dd20c");
        require(0x821dd1b8L, "b", "0x821dd20c");
        require(0x821dd1c8L, "beq", "0x821dd20c");
        require(0x821dd20cL, "stw", "r29,0x308c(r31)");

        // A non-null caller binds the third 0x24-byte buffer object immediately
        // after two vertex streams. Reset callers pass null.
        require(0x82138164L, "li", "r8,0x1");
        require(0x8213817cL, "bl", "0x821dd068");
        require(0x82138180L, "li", "r8,0x1");
        require(0x82138198L, "bl", "0x821dd068");
        require(0x8213819cL, "addi", "r4,r30,0x48");
        require(0x821381a4L, "bl", "0x821dd188");
        require(0x821e6e48L, "li", "r4,0x0");
        require(0x821e6e60L, "bl", "0x821dd188");
        require(0x8233e500L, "li", "r4,0x0");
        require(0x8233e508L, "bl", "0x821dd188");

        // The indexed shared draw consumes device+0x308C as a buffer object,
        // reading its resource word and byte-size field to construct the packet.
        require(0x821df500L, "lwz", "r22,0x308c(r31)");
        require(0x821df574L, "lwz", "r11,0x0(r22)");
        require(0x821df588L, "lwz", "r8,0x18(r22)");

        // 0x821DE7D0 belongs to a creation helper: r3 is declaration source,
        // r4 is an allocated output object. It is not a device-state bind.
        require(0x821de7b8L, "or", "r29,r3,r3");
        require(0x821de7bcL, "or", "r31,r4,r4");
        require(0x821de7d0L, "lhz", "r11,0x0(r29)");
        require(0x821de8d8L, "addi", "r3,r11,0x38");
        require(0x821de8e0L, "bl", "0x821d7458");
        require(0x821de8f4L, "or", "r4,r31,r31");
        require(0x821de8f8L, "or", "r3,r30,r30");
        require(0x821de8fcL, "bl", "0x821de7a8");

        println("AC6_INDEX_BUFFER_ASSERTIONS=" + assertions);
    }
}
