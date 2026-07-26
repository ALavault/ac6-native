// Read-only assertions for the AC6 PAL 0x8234CA20 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify8234CA20Boundary extends GhidraScript {
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
        println("AC6_8234CA20_CONTRACT=" + address + " " + instruction);
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
        println("AC6_8234CA20_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The preceding compact leaf ends before this initializer begins.
        require(0x8234c9c0L, "cmplwi", "cr6,r4,0x0");
        require(0x8234c9c4L, "beqlr", "cr6");
        require(0x8234c9c8L, "lwz", "r11,0x4(r3)");
        require(0x8234c9d0L, "stw", "r4,0x4(r3)");
        require(0x8234c9d4L, "blr", "blr");

        // The real leaf entry owns every loop-carried value and early return.
        require(0x8234c9d8L, "or", "r10,r6,r6");
        require(0x8234c9dcL, "stw", "r4,0x8(r3)");
        require(0x8234c9e0L, "stw", "r5,0xc(r3)");
        require(0x8234c9e4L, "cmplwi", "cr6,r10,0x0");
        require(0x8234c9e8L, "stw", "r10,0x0(r3)");
        require(0x8234c9ecL, "beqlr", "cr6");
        require(0x8234c9f0L, "subic.", "r9,r5,0x1");
        require(0x8234c9f4L, "stw", "r10,0x4(r3)");
        require(0x8234c9f8L, "li", "r11,0x0");
        require(0x8234c9fcL, "beqlr", "beqlr");

        // The loop starts at 0x8234CA00 and updates the same linked record.
        require(0x8234ca00L, "lwz", "r9,0x8(r3)");
        require(0x8234ca04L, "addi", "r11,r11,0x1");
        require(0x8234ca08L, "add", "r9,r10,r9");
        require(0x8234ca0cL, "stw", "r9,0x0(r10)");
        require(0x8234ca10L, "or", "r10,r9,r9");
        require(0x8234ca14L, "lwz", "r9,0xc(r3)");
        require(0x8234ca18L, "subi", "r9,r9,0x1");
        require(0x8234ca1cL, "cmplw", "cr6,r11,r9");

        // This configured address is only the backedge and has no xref.
        require(0x8234ca20L, "blt", "cr6,0x8234ca00");
        requireNoReferences(0x8234ca20L);
        require(0x8234ca24L, "blr", "blr");

        // A separate framed function begins immediately afterwards.
        require(0x8234ca28L, "mfspr", "r12,LR");
        require(0x8234ca2cL, "stw", "r12,-0x8(r1)");
        require(0x8234ca30L, "std", "r31,-0x10(r1)");
        require(0x8234ca34L, "stwu", "r1,-0x60(r1)");

        println("AC6_8234CA20_ASSERTIONS=" + assertions);
    }
}
