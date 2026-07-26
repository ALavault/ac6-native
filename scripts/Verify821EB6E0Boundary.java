// Read-only assertions for the AC6 PAL 0x821EB6E0 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821EB6E0Boundary extends GhidraScript {
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
        println("AC6_821EB6E0_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821EB6E0_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The preceding framed function exits before this function begins.
        require(0x821eb68cL, "or", "r3,r15,r15");
        require(0x821eb690L, "addi", "r1,r1,0x1b0");
        require(0x821eb698L, "bl", "0x8238445c");
        require(0x821eb69cL, "b", "0x82382f10");

        // The real entry owns frame setup and all loop-carried registers.
        require(0x821eb6a0L, "mfspr", "r12,LR");
        require(0x821eb6a4L, "stw", "r12,-0x8(r1)");
        require(0x821eb6a8L, "std", "r30,-0x18(r1)");
        require(0x821eb6acL, "std", "r31,-0x10(r1)");
        require(0x821eb6b0L, "stwu", "r1,-0x70(r1)");
        require(0x821eb6b4L, "or", "r31,r3,r3");
        require(0x821eb6b8L, "or", "r3,r5,r5");
        require(0x821eb6bcL, "or", "r30,r4,r4");
        require(0x821eb6c0L, "bl", "0x821eb3e8");
        require(0x821eb6c4L, "or", "r10,r30,r30");
        require(0x821eb6c8L, "addi", "r11,r31,0x200");
        require(0x821eb6ccL, "subf", "r8,r31,r30");
        require(0x821eb6d0L, "li", "r9,0x100");

        // The loop begins before the configured split.
        require(0x821eb6d4L, "lhz", "r7,0x0(r10)");
        require(0x821eb6d8L, "subic.", "r9,r9,0x1");
        require(0x821eb6dcL, "rlwinm", "r7,r7,0x1b,0x5,0x1e");

        // 0x821EB6E0 is a mid-body indexed load, not an entry.
        require(0x821eb6e0L, "lhzx", "r7,r7,r3");
        requireNoReferences(0x821eb6e0L);
        require(0x821eb6e4L, "rlwinm", "r7,r7,0x6,0x0,0x1f");
        require(0x821eb6e8L, "sth", "r7,-0x200(r11)");
        require(0x821eb6ecL, "lhzx", "r7,r8,r11");
        require(0x821eb700L, "lhz", "r7,0x400(r10)");
        require(0x821eb704L, "addi", "r10,r10,0x2");
        require(0x821eb714L, "sth", "r7,0x200(r11)");
        require(0x821eb718L, "addi", "r11,r11,0x2");
        require(0x821eb71cL, "bne", "0x821eb6d4");

        // The same function restores its frame and returns.
        require(0x821eb720L, "addi", "r1,r1,0x70");
        require(0x821eb724L, "lwz", "r12,-0x8(r1)");
        require(0x821eb728L, "mtspr", "LR,r12");
        require(0x821eb72cL, "ld", "r30,-0x18(r1)");
        require(0x821eb730L, "ld", "r31,-0x10(r1)");
        require(0x821eb734L, "blr", "blr");

        // A separate framed function begins at 0x821EB738.
        require(0x821eb738L, "mfspr", "r12,LR");
        require(0x821eb73cL, "stw", "r12,-0x8(r1)");
        require(0x821eb740L, "std", "r30,-0x18(r1)");
        require(0x821eb744L, "std", "r31,-0x10(r1)");
        require(0x821eb748L, "stwu", "r1,-0x70(r1)");

        println("AC6_821EB6E0_ASSERTIONS=" + assertions);
    }
}
