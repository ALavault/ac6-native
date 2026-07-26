// Read-only assertions for the AC6 PAL 0x822CE9A8 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822CE9A8Boundary extends GhidraScript {
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
        println("AC6_822CE9A8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822CE9A8_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real function entry owns the saved-register frame and initializes
        // the object, source and count state later consumed by the outer loop.
        require(0x822ce6c8L, "mfspr", "r12,LR");
        require(0x822ce6ccL, "bl", "0x82382edc");
        require(0x822ce6d0L, "stwu", "r1,-0x100(r1)");
        require(0x822ce6d8L, "or", "r31,r3,r3");
        require(0x822ce6dcL, "or", "r29,r4,r4");
        require(0x822ce6e0L, "or", "r30,r5,r5");
        require(0x822ce6f4L, "or", "r27,r22,r22");
        require(0x822ce750L, "addi", "r26,r31,0x52c");

        // The preceding passes derive r27 and the per-entry state.  The final
        // setup establishes constants and lookup-table bases for this loop.
        require(0x822ce874L, "addi", "r27,r27,0x1");
        require(0x822ce958L, "lwz", "r11,0x60(r31)");
        require(0x822ce95cL, "cmplwi", "cr6,r27,0x0");
        require(0x822ce960L, "slw", "r29,r30,r11");
        require(0x822ce964L, "beq", "cr6,0x822ceb1c");
        require(0x822ce96cL, "li", "r23,0x2");
        require(0x822ce970L, "subi", "r25,r11,0x58d0");
        require(0x822ce978L, "li", "r28,0x4");
        require(0x822ce97cL, "subi", "r24,r11,0x5910");

        // 0x822CE9A8 is a sequential store in the outer loop body.  It consumes
        // r10 loaded through r24 at 0x822CE9A0 and the entry-owned stack frame;
        // it is neither a prologue nor an independently referenced target.
        require(0x822ce980L, "lhz", "r10,-0x240(r26)");
        require(0x822ce990L, "stb", "r23,0x50(r1)");
        require(0x822ce9a0L, "lhzx", "r10,r10,r24");
        require(0x822ce9a4L, "stb", "r7,0x52(r1)");
        require(0x822ce9a8L, "sth", "r10,0x54(r1)");
        requireNoReferences(0x822ce9a8L);
        require(0x822ce9acL, "lhzx", "r10,r8,r31");
        require(0x822ce9b0L, "cmplw", "cr6,r10,r11");

        // The same entry-owned r27/r26 state advances and returns to 0x822CE980;
        // the zero-count exit restores the original 0x100-byte frame.
        require(0x822ceb0cL, "subi", "r27,r27,0x1");
        require(0x822ceb10L, "addi", "r26,r26,0x2");
        require(0x822ceb14L, "cmplwi", "cr6,r27,0x0");
        require(0x822ceb18L, "bne", "cr6,0x822ce980");
        require(0x822ceb1cL, "li", "r3,0x1");
        require(0x822ceb20L, "addi", "r1,r1,0x100");
        require(0x822ceb24L, "b", "0x82382f2c");

        println("AC6_822CE9A8_ASSERTIONS=" + assertions);
    }
}
