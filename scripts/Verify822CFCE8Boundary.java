// Read-only assertions for the AC6 PAL 0x822CFCA8 constructor-loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822CFCE8Boundary extends GhidraScript {
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
        println("AC6_822CFCE8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822CFCE8_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The true entry owns the saved-register frame, object pointer, base
        // initialization and first embedded-object construction.
        require(0x822cfca8L, "mfspr", "r12,LR");
        require(0x822cfcacL, "bl", "0x82382ef4");
        require(0x822cfcb0L, "stwu", "r1,-0x80(r1)");
        require(0x822cfcb4L, "or", "r31,r3,r3");
        require(0x822cfcb8L, "bl", "0x8232ab20");
        require(0x822cfcc0L, "addi", "r28,r31,0x70");
        require(0x822cfcccL, "stw", "r11,0x0(r31)");
        require(0x822cfcd0L, "bl", "0x8228c1a0");

        // The entry seeds the seven-element loop. The configured split is only
        // the decrement after the repeated constructor call; it consumes r30
        // and has no independent incoming edge.
        require(0x822cfcd4L, "addi", "r27,r31,0xf4");
        require(0x822cfcd8L, "li", "r30,0x7");
        require(0x822cfcdcL, "or", "r29,r27,r27");
        require(0x822cfce0L, "or", "r3,r29,r29");
        require(0x822cfce4L, "bl", "0x8228b680");
        require(0x822cfce8L, "subi", "r30,r30,0x1");
        requireNoReferences(0x822cfce8L);
        require(0x822cfcecL, "addi", "r29,r29,0xc60");
        require(0x822cfcf0L, "cmpwi", "cr6,r30,0x0");
        require(0x822cfcf4L, "bge", "cr6,0x822cfce0");

        // Post-loop stores publish the embedded-object pointers from r27/r28
        // and r31, then the entry-owned frame returns through its helper.
        require(0x822cfcf8L, "addi", "r11,r31,0xd54");
        require(0x822cfcfcL, "stw", "r28,0x4(r31)");
        require(0x822cfd04L, "stw", "r27,0xc(r31)");
        require(0x822cfd14L, "stw", "r11,0x18(r31)");
        require(0x822cfd28L, "or", "r3,r31,r31");
        require(0x822cfd38L, "stw", "r11,0x60(r31)");
        require(0x822cfd3cL, "addi", "r1,r1,0x80");
        require(0x822cfd40L, "b", "0x82382f44");

        // A new independent saved-frame function starts at 0x822CFD48.
        require(0x822cfd48L, "mfspr", "r12,LR");
        require(0x822cfd4cL, "stw", "r12,-0x8(r1)");
        require(0x822cfd58L, "stwu", "r1,-0x70(r1)");

        println("AC6_822CFCE8_ASSERTIONS=" + assertions);
    }
}
