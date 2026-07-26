// Read-only assertions for the AC6 PAL 0x822CEAC0 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822CEAC0Boundary extends GhidraScript {
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
        println("AC6_822CEAC0_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822CEAC0_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the saved-register frame and the object/count
        // state consumed throughout the dynamic-table construction loops.
        require(0x822ce6c8L, "mfspr", "r12,LR");
        require(0x822ce6ccL, "bl", "0x82382edc");
        require(0x822ce6d0L, "stwu", "r1,-0x100(r1)");
        require(0x822ce6d8L, "or", "r31,r3,r3");
        require(0x822ce6dcL, "or", "r29,r4,r4");
        require(0x822ce6e0L, "or", "r30,r5,r5");
        require(0x822ce6f4L, "or", "r27,r22,r22");
        require(0x822ce750L, "addi", "r26,r31,0x52c");

        // Entry-owned state and lookup bases feed the outer loop and select
        // the alternate branch containing 0x822CEAC0.
        require(0x822ce958L, "lwz", "r11,0x60(r31)");
        require(0x822ce960L, "slw", "r29,r30,r11");
        require(0x822ce970L, "subi", "r25,r11,0x58d0");
        require(0x822ce97cL, "subi", "r24,r11,0x5910");
        require(0x822ce980L, "lhz", "r10,-0x240(r26)");
        require(0x822ce9b8L, "bgt", "cr6,0x822cea24");

        // The alternate path derives r7/r3/r4/r8 before reaching the candidate.
        // 0x822CEAC0 consumes r11 produced by the immediately preceding add;
        // it has no independent prologue or incoming control-flow reference.
        require(0x822cea24L, "subf", "r4,r11,r10");
        require(0x822cea2cL, "srw", "r3,r9,r11");
        require(0x822cea48L, "lhzx", "r7,r6,r31");
        require(0x822ceaa0L, "slw", "r5,r30,r10");
        require(0x822ceaa4L, "stb", "r4,0x51(r1)");
        require(0x822ceaacL, "slw", "r8,r30,r4");
        require(0x822ceab0L, "cmplwi", "cr6,r5,0x0");
        require(0x822ceab4L, "beq", "cr6,0x822ceb0c");
        require(0x822ceab8L, "add", "r11,r7,r3");
        require(0x822ceabcL, "rlwinm", "r10,r8,0x1,0x0,0x1e");
        require(0x822ceac0L, "addi", "r11,r11,0xa92");
        requireNoReferences(0x822ceac0L);
        require(0x822ceac4L, "add", "r10,r8,r10");
        require(0x822ceac8L, "rlwinm", "r9,r11,0x1,0x0,0x1e");
        require(0x822cead8L, "add", "r9,r11,r31");

        // The copy loop uses the real entry's stack scratch, then rejoins the
        // outer r27/r26 loop and returns through its matching restore helper.
        require(0x822ceadcL, "addi", "r11,r1,0x50");
        require(0x822ceae0L, "or", "r10,r9,r9");
        require(0x822ceb08L, "blt", "cr6,0x822ceadc");
        require(0x822ceb0cL, "subi", "r27,r27,0x1");
        require(0x822ceb10L, "addi", "r26,r26,0x2");
        require(0x822ceb18L, "bne", "cr6,0x822ce980");
        require(0x822ceb20L, "addi", "r1,r1,0x100");
        require(0x822ceb24L, "b", "0x82382f2c");

        println("AC6_822CEAC0_ASSERTIONS=" + assertions);
    }
}
