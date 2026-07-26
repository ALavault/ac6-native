// Read-only assertions for the AC6 PAL 0x822CE7B0 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822CE7B0Boundary extends GhidraScript {
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
        println("AC6_822CE7B0_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822CE7B0_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real function entry owns the saved-register frame, argument state
        // and both stack-resident u16 work arrays used by this loop.
        require(0x822ce6c8L, "mfspr", "r12,LR");
        require(0x822ce6ccL, "bl", "0x82382edc");
        require(0x822ce6d0L, "stwu", "r1,-0x100(r1)");
        require(0x822ce6d8L, "or", "r31,r3,r3");
        require(0x822ce6dcL, "or", "r29,r4,r4");
        require(0x822ce6e0L, "or", "r30,r5,r5");
        require(0x822ce6ecL, "sth", "r22,0x80(r1)");
        require(0x822ce6f0L, "addi", "r3,r1,0x82");
        require(0x822ce704L, "sth", "r22,0x60(r1)");
        require(0x822ce70cL, "addi", "r3,r1,0x62");

        // The entry clears its object storage, then derives the loop count and
        // object cursor from the original arguments and object pointer.
        require(0x822ce71cL, "stw", "r30,0x60(r31)");
        require(0x822ce728L, "addi", "r3,r31,0x2ec");
        require(0x822ce74cL, "bl", "0x823835d0");
        require(0x822ce750L, "addi", "r26,r31,0x52c");
        require(0x822ce784L, "rlwinm", "r5,r29,0x0,0x10,0x1f");
        require(0x822ce788L, "cmplwi", "cr6,r5,0x0");
        require(0x822ce78cL, "beq", "cr6,0x822ce7e0");
        require(0x822ce790L, "addi", "r7,r31,0x2ac");
        require(0x822ce794L, "or", "r6,r5,r5");

        // The loop loads an object u16, indexes the entry-owned stack array,
        // increments that element, then stores it at 0x822CE7B0. The store has
        // no independent prologue or incoming reference.
        require(0x822ce798L, "lhz", "r11,0x0(r7)");
        require(0x822ce79cL, "addi", "r9,r1,0x80");
        require(0x822ce7a0L, "rlwinm", "r10,r11,0x1,0x0,0x1f");
        require(0x822ce7a4L, "cmplw", "cr6,r30,r11");
        require(0x822ce7a8L, "lhzx", "r8,r10,r9");
        require(0x822ce7acL, "addi", "r8,r8,0x1");
        require(0x822ce7b0L, "sthx", "r8,r10,r9");
        requireNoReferences(0x822ce7b0L);
        require(0x822ce7b4L, "ble", "cr6,0x822ce7bc");

        // The body updates entry-owned extrema and cursors, then returns to the
        // load at 0x822CE798. All exits use the original 0x100-byte frame.
        require(0x822ce7bcL, "cmplw", "cr6,r28,r11");
        require(0x822ce7c8L, "subi", "r6,r6,0x1");
        require(0x822ce7ccL, "addi", "r7,r7,0x2");
        require(0x822ce7d0L, "cmplwi", "cr6,r6,0x0");
        require(0x822ce7d4L, "bne", "cr6,0x822ce798");
        require(0x822ce7e4L, "addi", "r1,r1,0x100");
        require(0x822ce7e8L, "b", "0x82382f2c");
        require(0x822ceb20L, "addi", "r1,r1,0x100");
        require(0x822ceb24L, "b", "0x82382f2c");

        println("AC6_822CE7B0_ASSERTIONS=" + assertions);
    }
}
