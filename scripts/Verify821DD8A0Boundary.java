// Read-only assertions for the AC6 PAL 0x821DD8A0 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821DD8A0Boundary extends GhidraScript {
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
        println("AC6_821DD8A0_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821DD8A0_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The preceding function restores its frame and returns.
        require(0x821dd848L, "addi", "r1,r1,0x70");
        require(0x821dd84cL, "lwz", "r12,-0x8(r1)");
        require(0x821dd850L, "mtspr", "LR,r12");
        require(0x821dd854L, "ld", "r30,-0x18(r1)");
        require(0x821dd858L, "ld", "r31,-0x10(r1)");
        require(0x821dd85cL, "blr", "blr");

        // The real entry owns the large frame and constructs the comparison.
        require(0x821dd860L, "mfspr", "r12,LR");
        require(0x821dd864L, "stw", "r12,-0x8(r1)");
        require(0x821dd868L, "std", "r30,-0x18(r1)");
        require(0x821dd86cL, "std", "r31,-0x10(r1)");
        require(0x821dd870L, "stwu", "r1,-0x670(r1)");
        require(0x821dd874L, "or", "r31,r3,r3");
        require(0x821dd878L, "or", "r4,r5,r5");
        require(0x821dd87cL, "li", "r5,0x0");
        require(0x821dd880L, "addi", "r3,r1,0x50");
        require(0x821dd884L, "bl", "0x821eb6a0");
        require(0x821dd888L, "addi", "r30,r31,0x3a8c");
        require(0x821dd88cL, "addi", "r11,r1,0x50");
        require(0x821dd890L, "or", "r10,r30,r30");
        require(0x821dd894L, "addi", "r9,r11,0x600");

        // Byte comparison loop; configured 0x821DD8A0 is its subtract.
        require(0x821dd898L, "lbz", "r8,0x0(r11)");
        require(0x821dd89cL, "lbz", "r7,0x0(r10)");
        require(0x821dd8a0L, "subf.", "r8,r7,r8");
        requireNoReferences(0x821dd8a0L);
        require(0x821dd8a4L, "bne", "0x821dd8b8");
        require(0x821dd8a8L, "addi", "r11,r11,0x1");
        require(0x821dd8acL, "addi", "r10,r10,0x1");
        require(0x821dd8b0L, "cmpw", "cr6,r11,r9");
        require(0x821dd8b4L, "bne", "cr6,0x821dd898");

        // The same function handles divergence and restores its frame.
        require(0x821dd8b8L, "cmpwi", "r8,0x0");
        require(0x821dd8bcL, "beq", "0x821dd90c");
        require(0x821dd8c0L, "lwz", "r3,0x56ec(r31)");
        require(0x821dd8d8L, "li", "r5,0x600");
        require(0x821dd8e0L, "bl", "0x821f37d0");
        require(0x821dd8e8L, "addi", "r4,r1,0x50");
        require(0x821dd8fcL, "bl", "0x82382f70");
        require(0x821dd90cL, "addi", "r1,r1,0x670");
        require(0x821dd910L, "lwz", "r12,-0x8(r1)");
        require(0x821dd914L, "mtspr", "LR,r12");
        require(0x821dd918L, "ld", "r30,-0x18(r1)");
        require(0x821dd91cL, "ld", "r31,-0x10(r1)");
        require(0x821dd920L, "blr", "blr");

        // A distinct framed sibling begins at 0x821DD928.
        require(0x821dd928L, "mfspr", "r12,LR");
        require(0x821dd92cL, "stw", "r12,-0x8(r1)");
        require(0x821dd930L, "std", "r30,-0x18(r1)");
        require(0x821dd934L, "std", "r31,-0x10(r1)");
        require(0x821dd938L, "stwu", "r1,-0x670(r1)");

        println("AC6_821DD8A0_ASSERTIONS=" + assertions);
    }
}
