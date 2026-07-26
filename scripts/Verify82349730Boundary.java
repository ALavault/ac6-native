// Read-only assertions for the AC6 PAL 0x82349730 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82349730Boundary extends GhidraScript {
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
        println("AC6_82349730_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82349730_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The preceding leaf is complete before the candidate's function.
        require(0x823496c0L, "lis", "r11,-0x7dff");
        require(0x823496c4L, "addi", "r11,r11,0x2080");
        require(0x823496c8L, "stw", "r11,0x0(r3)");
        require(0x823496ccL, "blr", "blr");

        // The real entry owns validation, early returns and loop state.
        require(0x823496d0L, "cmplwi", "cr6,r6,0x1");
        require(0x823496d4L, "beq", "cr6,0x823496ec");
        require(0x823496d8L, "cmplwi", "cr6,r6,0x4");
        require(0x823496dcL, "beq", "cr6,0x823496ec");
        require(0x823496e0L, "lis", "r3,-0x7ff9");
        require(0x823496e8L, "blr", "blr");
        require(0x823496ecL, "addic.", "r11,r3,0x80");
        require(0x823496f0L, "beq", "0x8234974c");
        require(0x823496f8L, "stw", "r8,0x10(r11)");
        require(0x82349700L, "li", "r8,0x0");
        require(0x82349704L, "addi", "r6,r10,0x2080");
        require(0x8234970cL, "addi", "r10,r11,0x20");
        require(0x82349714L, "stw", "r6,0x0(r11)");
        require(0x82349718L, "li", "r6,0x1");

        // The loop head and body consume only entry-owned state.
        require(0x8234971cL, "stw", "r8,0x0(r10)");
        require(0x82349720L, "subic.", "r6,r6,0x1");
        require(0x82349724L, "stw", "r8,0x4(r10)");
        require(0x82349728L, "stw", "r8,0x11c(r10)");
        require(0x8234972cL, "addi", "r10,r10,0x130");

        // This configured address is only the backedge and has no xref.
        require(0x82349730L, "bge", "0x8234971c");
        requireNoReferences(0x82349730L);

        // Fallthrough completes the same leaf, including its null-output path.
        require(0x82349734L, "lis", "r10,-0x7dff");
        require(0x82349738L, "stw", "r8,0x280(r11)");
        require(0x8234973cL, "stw", "r8,0x284(r11)");
        require(0x82349744L, "stw", "r10,0x0(r11)");
        require(0x82349748L, "b", "0x82349750");
        require(0x8234974cL, "li", "r11,0x0");
        require(0x82349750L, "cmplwi", "cr6,r9,0x0");
        require(0x82349754L, "beq", "cr6,0x8234975c");
        require(0x82349758L, "stw", "r11,0x0(r9)");
        require(0x8234975cL, "li", "r3,0x0");
        require(0x82349760L, "blr", "blr");

        // A separate framed function starts at the next aligned prologue.
        require(0x82349768L, "mfspr", "r12,LR");
        require(0x8234976cL, "stw", "r12,-0x8(r1)");
        require(0x82349770L, "std", "r31,-0x10(r1)");
        require(0x82349774L, "stwu", "r1,-0x60(r1)");

        println("AC6_82349730_ASSERTIONS=" + assertions);
    }
}
