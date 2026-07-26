// Read-only assertions for the AC6 PAL 0x82275F60 loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82275F88Boundary extends GhidraScript {
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
        println("AC6_82275F88_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82275F88_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // Real entry and inherited loop frame.
        require(0x82275f60L, "mfspr", "r12,LR");
        require(0x82275f64L, "bl", "0x82382efc");
        require(0x82275f68L, "stwu", "r1,-0x70(r1)");
        require(0x82275f6cL, "or", "r29,r3,r3");
        require(0x82275f70L, "li", "r30,0x2");
        require(0x82275f74L, "addi", "r31,r29,0x10");

        // 0x82275F88 is ordinary fallthrough inside the first loop iteration.
        require(0x82275f78L, "lis", "r11,-0x7df7");
        require(0x82275f84L, "li", "r4,0x10");
        require(0x82275f88L, "or", "r3,r31,r31");
        require(0x82275f8cL, "bl", "0x82090228");
        requireNoReferences(0x82275f88L);

        // The backward edge needs the loop head in the same generated body.
        require(0x82275fa8L, "subi", "r30,r30,0x1");
        require(0x82275facL, "addi", "r31,r31,0xe0");
        require(0x82275fb0L, "cmpwi", "cr6,r30,0x0");
        require(0x82275fb4L, "bge", "cr6,0x82275f78");
        require(0x82275fbcL, "addi", "r1,r1,0x70");
        require(0x82275fc0L, "b", "0x82382f4c");

        // The next physical entry has its own save helper and is preserved.
        require(0x82275fc8L, "mfspr", "r12,LR");
        require(0x82275fccL, "bl", "0x82382efc");

        println("AC6_82275F88_ASSERTIONS=" + assertions);
    }
}
