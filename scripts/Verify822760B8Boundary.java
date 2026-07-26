// Read-only assertions for the AC6 PAL 0x82276098 loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822760B8Boundary extends GhidraScript {
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
        println("AC6_822760B8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822760B8_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // Real entry and loop state established before the configured split.
        require(0x82276098L, "mfspr", "r12,LR");
        require(0x8227609cL, "bl", "0x82382efc");
        require(0x822760a0L, "stwu", "r1,-0x70(r1)");
        require(0x822760a4L, "or", "r29,r3,r3");
        require(0x822760a8L, "li", "r30,0x4");
        require(0x822760acL, "addi", "r31,r29,0x20");

        // 0x822760B8 consumes r11 and loop state prepared by fallthrough.
        require(0x822760b0L, "lis", "r11,-0x7df7");
        require(0x822760b4L, "li", "r5,0x4");
        require(0x822760b8L, "addi", "r6,r11,0x3560");
        require(0x822760bcL, "li", "r4,0x10");
        require(0x822760c0L, "or", "r3,r31,r31");
        require(0x822760c4L, "bl", "0x82090228");
        requireNoReferences(0x822760b8L);

        // The backward edge and restore tail belong to the same frame.
        require(0x822760e0L, "subi", "r30,r30,0x1");
        require(0x822760e4L, "addi", "r31,r31,0xe0");
        require(0x822760e8L, "cmpwi", "cr6,r30,0x0");
        require(0x822760ecL, "bge", "cr6,0x822760b0");
        require(0x822760f4L, "addi", "r1,r1,0x70");
        require(0x822760f8L, "b", "0x82382f4c");

        // A new independent prologue begins at 0x82276100.
        require(0x82276100L, "mfspr", "r12,LR");
        require(0x82276104L, "bl", "0x82382efc");

        println("AC6_822760B8_ASSERTIONS=" + assertions);
    }
}
