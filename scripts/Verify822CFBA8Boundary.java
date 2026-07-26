// Read-only assertions for the AC6 PAL 0x822CFBA8 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify822CFBA8Boundary extends GhidraScript {
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
        println("AC6_822CFBA8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_822CFBA8_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the frame, object pointer and both loop states.
        require(0x822cfb18L, "mfspr", "r12,LR");
        require(0x822cfb1cL, "bl", "0x82382edc");
        require(0x822cfb20L, "stwu", "r1,-0xb0(r1)");
        require(0x822cfb24L, "or", "r31,r3,r3");
        require(0x822cfb3cL, "stw", "r3,0x8(r31)");
        require(0x822cfb48L, "bl", "0x822c8fd0");
        require(0x822cfb64L, "stw", "r3,0x40(r31)");
        require(0x822cfb68L, "bl", "0x822cfc98");
        require(0x822cfb74L, "addi", "r30,r31,0x524");
        require(0x822cfb7cL, "li", "r21,0xf");
        require(0x822cfb88L, "li", "r27,0x1");
        require(0x822cfb8cL, "addi", "r26,r11,0x4bf8");
        require(0x822cfb98L, "subi", "r25,r11,0x589c");
        require(0x822cfba0L, "subi", "r24,r11,0x58a8");

        // This configured split only completes the lis/subi constant load.
        // It has no incoming reference and cannot be a callable entry.
        require(0x822cfba4L, "lis", "r11,-0x7dff");
        require(0x822cfba8L, "subi", "r23,r11,0x58b0");
        requireNoReferences(0x822cfba8L);

        // The true outer-loop target consumes the entry-owned cursor and r23.
        require(0x822cfbacL, "addi", "r3,r30,0x1");
        require(0x822cfbb0L, "or", "r4,r23,r23");
        require(0x822cfbb4L, "bl", "0x822cfac8");
        require(0x822cfbb8L, "addi", "r3,r30,0x22");
        require(0x822cfbc4L, "bl", "0x822cfac8");
        require(0x822cfbd8L, "addi", "r29,r30,0x63");
        require(0x822cfbdcL, "li", "r28,0x20");

        // The nested loop uses the same frame and state, then returns to the
        // outer target. The following configured split remains out of scope.
        require(0x822cfbe4L, "addi", "r3,r29,0x1");
        require(0x822cfbe8L, "or", "r4,r26,r26");
        require(0x822cfbecL, "bl", "0x822cfac8");
        require(0x822cfbf0L, "subi", "r28,r28,0x1");
        require(0x822cfbf8L, "addi", "r29,r29,0x21");
        require(0x822cfc00L, "bne", "cr6,0x822cfbe4");
        require(0x822cfc04L, "subi", "r21,r21,0x1");
        require(0x822cfc08L, "addi", "r30,r30,0x483");
        require(0x822cfc10L, "bne", "cr6,0x822cfbac");
        require(0x822cfc18L, "addi", "r1,r1,0xb0");
        require(0x822cfc1cL, "b", "0x82382f2c");

        // A separate real prologue follows the complete function.
        require(0x822cfc20L, "mfspr", "r12,LR");
        require(0x822cfc24L, "stw", "r12,-0x8(r1)");
        require(0x822cfc28L, "std", "r31,-0x10(r1)");
        require(0x822cfc2cL, "stwu", "r1,-0x60(r1)");

        println("AC6_822CFBA8_ASSERTIONS=" + assertions);
    }
}
