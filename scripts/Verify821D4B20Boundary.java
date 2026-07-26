// Read-only assertions for the AC6 PAL 0x821D4B20 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821D4B20Boundary extends GhidraScript {
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
        println("AC6_821D4B20_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821D4B20_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the frame and global-object state used by every
        // iteration of the four-element construction loop.
        require(0x821d4ae0L, "mfspr", "r12,LR");
        require(0x821d4ae4L, "bl", "0x82382ef0");
        require(0x821d4ae8L, "stwu", "r1,-0x90(r1)");
        require(0x821d4af4L, "addi", "r28,r11,0x6250");
        require(0x821d4b00L, "addi", "r3,r28,0x20c");
        require(0x821d4b04L, "stw", "r11,0x200(r28)");
        require(0x821d4b08L, "stw", "r11,0x204(r28)");
        require(0x821d4b0cL, "stw", "r11,0x208(r28)");
        require(0x821d4b10L, "bl", "0x823d6abc");

        // The configured address is the last instruction of loop setup. It
        // consumes the high half loaded at 0x821D4B14 and follows the counters
        // initialized by the entry; no branch or call targets it.
        require(0x821d4b14L, "lis", "r11,-0x7dfb");
        require(0x821d4b18L, "li", "r27,0x0");
        require(0x821d4b1cL, "li", "r31,0x0");
        require(0x821d4b20L, "addi", "r26,r11,0x42a8");
        requireNoReferences(0x821d4b20L);

        // The loop head immediately overwrites r11 but consumes the retained
        // r26 table base, r27 index and r31 byte cursor set above.
        require(0x821d4b24L, "lis", "r11,-0x7de3");
        require(0x821d4b30L, "or", "r6,r27,r27");
        require(0x821d4b34L, "addi", "r5,r11,0x4bd0");
        require(0x821d4b40L, "bl", "0x821f5990");
        require(0x821d4b44L, "addi", "r11,r28,0x238");
        require(0x821d4b4cL, "add", "r29,r31,r11");
        require(0x821d4b60L, "stw", "r30,-0x10(r29)");
        require(0x821d4b7cL, "stw", "r11,0x0(r29)");
        require(0x821d4b90L, "stw", "r11,0x10(r29)");
        require(0x821d4b9cL, "lwzx", "r4,r31,r26");

        // The entry-owned cursors advance and branch to 0x821D4B24. The
        // fallthrough still uses r28 and returns through the matching restore.
        require(0x821d4bacL, "addi", "r31,r31,0x4");
        require(0x821d4bb0L, "addi", "r27,r27,0x1");
        require(0x821d4bb4L, "cmplwi", "cr6,r31,0x10");
        require(0x821d4bb8L, "blt", "cr6,0x821d4b24");
        require(0x821d4bbcL, "lwz", "r3,0x238(r28)");
        require(0x821d4bc4L, "or", "r3,r28,r28");
        require(0x821d4bc8L, "addi", "r1,r1,0x90");
        require(0x821d4bccL, "b", "0x82382f40");

        println("AC6_821D4B20_ASSERTIONS=" + assertions);
    }
}
