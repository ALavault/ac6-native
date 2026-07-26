// Read-only assertions for the AC6 PAL 0x82265D20 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82265D20Boundary extends GhidraScript {
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
        println("AC6_82265D20_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82265D20_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The prior function has an explicit restore and tail return.
        require(0x82265ce0L, "cmplwi", "cr6,r11,0x0");
        require(0x82265ce4L, "beq", "cr6,0x82265cf0");
        require(0x82265ce8L, "bl", "0x82380070");
        require(0x82265cf0L, "addi", "r1,r1,0x70");
        require(0x82265cf4L, "b", "0x82382f4c");

        // The real leaf entry initializes every state item used by the loop.
        require(0x82265cf8L, "lis", "r11,-0x7dff");
        require(0x82265cfcL, "addi", "r10,r3,0x1008");
        require(0x82265d00L, "subi", "r9,r11,0x7f00");
        require(0x82265d04L, "lis", "r11,-0x7e00");
        require(0x82265d08L, "stw", "r9,0x0(r3)");
        require(0x82265d0cL, "li", "r9,0x0");
        require(0x82265d10L, "lfs", "f0,0x82c(r11)");
        require(0x82265d14L, "li", "r11,0x271");
        require(0x82265d18L, "stfs", "f0,0x238c(r3)");

        // The true loop head decrements the entry-owned counter. The configured
        // address is only the following zero-store through the entry-owned r10.
        require(0x82265d1cL, "subi", "r11,r11,0x1");
        require(0x82265d20L, "stw", "r9,0x0(r10)");
        requireNoReferences(0x82265d20L);
        require(0x82265d24L, "addi", "r10,r10,0x8");
        require(0x82265d28L, "cmplwi", "cr6,r11,0x0");
        require(0x82265d2cL, "bne", "cr6,0x82265d1c");
        require(0x82265d30L, "blr", "blr");

        // The following aligned leaf is independent and writes another vtable.
        require(0x82265d38L, "lis", "r11,-0x7dfb");
        require(0x82265d3cL, "addi", "r11,r11,0x4db0");
        require(0x82265d40L, "stw", "r11,0x0(r3)");
        require(0x82265d44L, "blr", "blr");

        println("AC6_82265D20_ASSERTIONS=" + assertions);
    }
}
