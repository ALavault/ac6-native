// Read-only assertions for the AC6 PAL 0x822EF758 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822EF758Boundary extends GhidraScript {
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
        println("AC6_822EF758_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822EF758_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the frame, object pointer and loop state.
        require(0x822ef6d8L, "mfspr", "r12,LR");
        require(0x822ef6dcL, "bl", "0x82382efc");
        require(0x822ef6e0L, "stwu", "r1,-0x70(r1)");
        require(0x822ef6e4L, "or", "r31,r3,r3");
        require(0x822ef6e8L, "li", "r10,0x53");
        require(0x822ef6ecL, "li", "r9,0x41");
        require(0x822ef6f0L, "li", "r8,0x56");
        require(0x822ef6f4L, "li", "r7,0x45");
        require(0x822ef6f8L, "lwz", "r11,0x9c(r31)");
        require(0x822ef700L, "li", "r5,0x0");

        // The setup remains one sequence even across the separately configured
        // 0x822EF708 address, which is outside this cycle's patch scope.
        require(0x822ef704L, "stw", "r10,0x4(r31)");
        require(0x822ef708L, "addi", "r3,r31,0x9c");
        require(0x822ef70cL, "stw", "r9,0x8(r31)");
        require(0x822ef710L, "stw", "r8,0xc(r31)");
        require(0x822ef714L, "stw", "r7,0x10(r31)");
        require(0x822ef718L, "lwz", "r11,0x4(r11)");
        require(0x822ef71cL, "stw", "r6,0x14(r31)");
        require(0x822ef720L, "stw", "r5,0x18(r31)");
        require(0x822ef724L, "mtspr", "CTR,r11");
        require(0x822ef728L, "bctrl", "bctrl");

        // The entry initializes all loop-carried registers before the true head.
        require(0x822ef72cL, "lis", "r11,0x0");
        require(0x822ef730L, "addi", "r31,r31,0xe4");
        require(0x822ef734L, "li", "r30,0x3");
        require(0x822ef738L, "ori", "r29,r11,0xa7e0");
        require(0x822ef73cL, "lwz", "r11,0x0(r31)");
        require(0x822ef740L, "or", "r3,r31,r31");
        require(0x822ef744L, "lwz", "r11,0x4(r11)");
        require(0x822ef748L, "mtspr", "CTR,r11");
        require(0x822ef74cL, "bctrl", "bctrl");
        require(0x822ef750L, "subi", "r30,r30,0x1");
        require(0x822ef754L, "add", "r31,r31,r29");

        // The configured address only compares the entry-owned counter before
        // the backedge. It has no incoming reference of its own.
        require(0x822ef758L, "cmplwi", "cr6,r30,0x0");
        requireNoReferences(0x822ef758L);
        require(0x822ef75cL, "bne", "cr6,0x822ef73c");
        require(0x822ef760L, "addi", "r1,r1,0x70");
        require(0x822ef764L, "b", "0x82382f4c");

        // A separate real prologue follows the completed loop function.
        require(0x822ef768L, "mfspr", "r12,LR");
        require(0x822ef76cL, "bl", "0x82382efc");
        require(0x822ef770L, "stwu", "r1,-0x70(r1)");
        require(0x822ef774L, "or", "r31,r3,r3");

        println("AC6_822EF758_ASSERTIONS=" + assertions);
    }
}
