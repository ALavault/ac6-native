// Read-only assertions for the AC6 PAL 0x823D2088 table-initialization boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify823D20B0Boundary extends GhidraScript {
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
        println("AC6_823D20B0_CONTRACT=" + address + " " + instruction);
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
        println("AC6_823D20B0_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the saved-register frame and derives all three
        // table cursors consumed by the following loop.
        require(0x823d2088L, "mfspr", "r12,LR");
        require(0x823d208cL, "stw", "r12,-0x8(r1)");
        require(0x823d2090L, "std", "r31,-0x10(r1)");
        require(0x823d2094L, "stwu", "r1,-0x60(r1)");
        require(0x823d2098L, "lis", "r11,-0x7d63");
        require(0x823d209cL, "subi", "r31,r11,0x7068");
        require(0x823d20a0L, "addi", "r10,r31,0x2a04");
        require(0x823d20a4L, "addi", "r11,r31,0xe04");

        // The loop head produces r9 and advances r11. The configured split is
        // merely its store body: it consumes r9/r10 and has no incoming edge.
        require(0x823d20a8L, "or", "r9,r11,r11");
        require(0x823d20acL, "addi", "r11,r11,0x100");
        require(0x823d20b0L, "stw", "r9,0x0(r10)");
        requireNoReferences(0x823d20b0L);
        require(0x823d20b4L, "addi", "r9,r31,0x2004");
        require(0x823d20b8L, "addi", "r10,r10,0x4");
        require(0x823d20bcL, "cmpw", "cr6,r11,r9");
        require(0x823d20c0L, "blt", "cr6,0x823d20a8");

        // The post-loop zeroing and restore tail still consume the entry-owned
        // r31 frame; 0x823D2108 starts a distinct saved-frame function.
        require(0x823d20c4L, "addi", "r11,r31,0x2004");
        require(0x823d20d0L, "addi", "r3,r31,0xe04");
        require(0x823d20d4L, "stw", "r11,0x2a08(r31)");
        require(0x823d20dcL, "stw", "r11,0xe00(r31)");
        require(0x823d20e0L, "bl", "0x823835d0");
        require(0x823d20f0L, "bl", "0x823835d0");
        require(0x823d20f4L, "addi", "r1,r1,0x60");
        require(0x823d2100L, "ld", "r31,-0x10(r1)");
        require(0x823d2104L, "blr", "blr");
        require(0x823d2108L, "mfspr", "r12,LR");
        require(0x823d2110L, "stwu", "r1,-0x60(r1)");

        println("AC6_823D20B0_ASSERTIONS=" + assertions);
    }
}
