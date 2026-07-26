// Read-only assertions for the AC6 PAL 0x821CA538 constructor-loop boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821CA570Boundary extends GhidraScript {
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
        println("AC6_821CA570_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821CA570_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the frame and seeds every register required by
        // the three-element constructor loop.
        require(0x821ca538L, "mfspr", "r12,LR");
        require(0x821ca53cL, "bl", "0x82382efc");
        require(0x821ca540L, "stwu", "r1,-0x70(r1)");
        require(0x821ca544L, "lis", "r11,-0x7dfa");
        require(0x821ca548L, "or", "r29,r3,r3");
        require(0x821ca54cL, "addi", "r11,r11,0x79a8");
        require(0x821ca550L, "addi", "r30,r29,0x4");
        require(0x821ca554L, "li", "r31,0x3");
        require(0x821ca558L, "stw", "r11,0x0(r29)");

        // The loop body consumes r30, then updates r31/r30 and produces cr6.
        require(0x821ca55cL, "or", "r3,r30,r30");
        require(0x821ca560L, "bl", "0x821ca718");
        require(0x821ca564L, "subi", "r31,r31,0x1");
        require(0x821ca568L, "addi", "r30,r30,0x390");
        require(0x821ca56cL, "cmpwi", "cr6,r31,0x0");

        // The configured address is the conditional branch instruction itself,
        // not a call target or prologue. It consumes the immediately preceding
        // cr6 and has no independent incoming reference.
        require(0x821ca570L, "bge", "cr6,0x821ca55c");
        requireNoReferences(0x821ca570L);

        // Fallthrough continues with the entry-owned object/frame and returns
        // through the matching restore helper.
        require(0x821ca574L, "or", "r3,r29,r29");
        require(0x821ca578L, "bl", "0x821ca5e8");
        require(0x821ca57cL, "or", "r3,r29,r29");
        require(0x821ca580L, "addi", "r1,r1,0x70");
        require(0x821ca584L, "b", "0x82382f4c");

        // A distinct saved-frame function starts at 0x821CA588.
        require(0x821ca588L, "mfspr", "r12,LR");
        require(0x821ca58cL, "bl", "0x82382efc");
        require(0x821ca590L, "stwu", "r1,-0x70(r1)");

        println("AC6_821CA570_ASSERTIONS=" + assertions);
    }
}
