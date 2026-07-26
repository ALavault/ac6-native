// Read-only assertions for the AC6 PAL 0x820FA5E8 loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify820FA690Boundary extends GhidraScript {
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
        println("AC6_820FA690_CONTRACT=" + address + " " + instruction);
    }

    private void requireNoReferences(long value) {
        Address address = toAddr(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address);
        if (references.hasNext()) {
            throw new IllegalStateException(address + " has an incoming reference: " +
                references.next());
        }
        assertions++;
        println("AC6_820FA690_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the saved frame and initializes loop state.
        require(0x820fa5e8L, "mfspr", "r12,LR");
        require(0x820fa5ecL, "bl", "0x82382efc");
        require(0x820fa5f0L, "stfd", "f31,-0x28(r1)");
        require(0x820fa5f4L, "stwu", "r1,-0x80(r1)");
        require(0x820fa5fcL, "or", "r30,r3,r3");
        require(0x820fa600L, "li", "r29,0x2");
        require(0x820fa614L, "addi", "r31,r11,0x28");

        // The loop head uses the frame-owned r29/r31/f31 state.
        require(0x820fa670L, "lis", "r11,-0x7df7");
        require(0x820fa674L, "stfs", "f31,-0x8(r31)");
        require(0x820fa678L, "li", "r5,0x3");
        require(0x820fa680L, "addi", "r6,r11,0x6828");

        // The configured split is only argument setup for the first call. It
        // consumes r31 from the real entry and has no incoming reference.
        require(0x820fa690L, "addi", "r3,r31,0x8");
        requireNoReferences(0x820fa690L);
        require(0x820fa694L, "bl", "0x82090228");
        require(0x820fa698L, "lis", "r11,-0x7df7");
        require(0x820fa6a8L, "addi", "r3,r31,0x38");
        require(0x820fa6acL, "bl", "0x82090228");

        // Loop control and restore tail remain in the same frame.
        require(0x820fa6b0L, "subi", "r29,r29,0x1");
        require(0x820fa6bcL, "cmpwi", "cr6,r29,0x0");
        require(0x820fa6d8L, "addi", "r31,r31,0xc0");
        require(0x820fa6dcL, "bge", "cr6,0x820fa670");
        require(0x820fa6e4L, "addi", "r1,r1,0x80");
        require(0x820fa6e8L, "lfd", "f31,-0x28(r1)");
        require(0x820fa6ecL, "b", "0x82382f4c");

        // A new independent saved-frame function begins at 0x820FA6F0.
        require(0x820fa6f0L, "mfspr", "r12,LR");
        require(0x820fa6f4L, "bl", "0x82382ef8");

        println("AC6_820FA690_ASSERTIONS=" + assertions);
    }
}
