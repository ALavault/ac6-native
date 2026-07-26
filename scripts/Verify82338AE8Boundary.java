// Read-only assertions for the AC6 PAL 0x82338AB8 loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82338AE8Boundary extends GhidraScript {
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
        println("AC6_82338AE8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82338AE8_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry initializes the loop count and stores the element count.
        require(0x82338ab8L, "li", "r10,0x0");
        require(0x82338abcL, "stw", "r5,0x30(r3)");
        require(0x82338ac0L, "cmpwi", "cr6,r5,0x0");
        require(0x82338ac4L, "ble", "cr6,0x82338aec");
        require(0x82338ac8L, "addi", "r11,r4,0xc");

        // The loop head and body consume state prepared at the real entry.
        require(0x82338accL, "subi", "r9,r11,0x18");
        require(0x82338ad0L, "stw", "r11,-0x8(r11)");
        require(0x82338ad4L, "addi", "r10,r10,0x1");
        require(0x82338ad8L, "stw", "r9,-0xc(r11)");
        require(0x82338adcL, "addi", "r11,r11,0xc");
        require(0x82338ae0L, "lwz", "r9,0x30(r3)");
        require(0x82338ae4L, "cmpw", "cr6,r10,r9");

        // The configured split is only the loop back-edge. It consumes CR6 from
        // the preceding instruction and has no independent incoming reference.
        require(0x82338ae8L, "blt", "cr6,0x82338acc");
        requireNoReferences(0x82338ae8L);

        // Fallthrough remains in the same leaf function until its return.
        require(0x82338aecL, "addi", "r11,r3,0x18");
        require(0x82338b4cL, "stw", "r3,0x4(r11)");
        require(0x82338b50L, "blr", "blr");

        // The next independent function has a normal save prologue.
        require(0x82338b58L, "mfspr", "r12,LR");
        require(0x82338b5cL, "bl", "0x82382efc");

        println("AC6_82338AE8_ASSERTIONS=" + assertions);
    }
}
