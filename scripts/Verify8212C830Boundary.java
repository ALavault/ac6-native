// Read-only assertions for the AC6 PAL 0x8212C7F8 object-reset boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify8212C830Boundary extends GhidraScript {
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
        println("AC6_8212C830_CONTRACT=" + address + " " + instruction);
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
        println("AC6_8212C830_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The leaf entry seeds the common zero/-1 values and all first-loop
        // state from the caller-provided object pointer r3.
        require(0x8212c7f8L, "li", "r11,0x0");
        require(0x8212c7fcL, "li", "r8,-0x1");
        require(0x8212c800L, "addi", "r10,r3,0x2008");
        require(0x8212c804L, "li", "r9,0x800");
        require(0x8212c808L, "stw", "r11,0x4(r3)");
        require(0x8212c818L, "stw", "r8,0x5010(r3)");
        require(0x8212c81cL, "stw", "r8,0x5014(r3)");

        // The first counted loop decrements r9, clears paired arrays through
        // r10, and compares the same counter. The configured split is only the
        // compare immediately before the back edge and has no incoming edge.
        require(0x8212c820L, "subi", "r9,r9,0x1");
        require(0x8212c824L, "stw", "r11,-0x2000(r10)");
        require(0x8212c828L, "stw", "r11,0x0(r10)");
        require(0x8212c82cL, "addi", "r10,r10,0x4");
        require(0x8212c830L, "cmplwi", "cr6,r9,0x0");
        requireNoReferences(0x8212c830L);
        require(0x8212c834L, "bne", "cr6,0x8212c820");

        // A second loop and a CTR loop continue to consume r3/r11 initialized
        // by this leaf; the function terminates at 0x8212C888.
        require(0x8212c838L, "addi", "r10,r3,0x480c");
        require(0x8212c83cL, "li", "r9,0x200");
        require(0x8212c840L, "subi", "r9,r9,0x1");
        require(0x8212c844L, "stw", "r11,-0x800(r10)");
        require(0x8212c848L, "stw", "r11,0x0(r10)");
        require(0x8212c850L, "cmplwi", "cr6,r9,0x0");
        require(0x8212c854L, "bne", "cr6,0x8212c840");
        require(0x8212c858L, "stb", "r11,0x501c(r3)");
        require(0x8212c864L, "li", "r9,0x10");
        require(0x8212c878L, "mtspr", "CTR,r9");
        require(0x8212c87cL, "stw", "r11,0x0(r10)");
        require(0x8212c884L, "bdnz", "0x8212c87c");
        require(0x8212c888L, "blr", "blr");

        // The next independent saved-frame function begins at 0x8212C890.
        require(0x8212c890L, "mfspr", "r12,LR");
        require(0x8212c894L, "bl", "0x82382efc");
        require(0x8212c898L, "stwu", "r1,-0x70(r1)");

        println("AC6_8212C830_ASSERTIONS=" + assertions);
    }
}
