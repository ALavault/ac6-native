// Read-only assertions for the AC6 PAL 0x82106358 interpolation boundary.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82106520Boundary extends GhidraScript {
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
        println("AC6_82106520_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82106520_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The real entry owns the saved-register frame and seeds the table walk.
        require(0x82106358L, "mfspr", "r12,LR");
        require(0x8210635cL, "bl", "0x82382ee4");
        require(0x82106368L, "li", "r28,0x0");
        require(0x8210636cL, "li", "r8,0x1");
        require(0x82106370L, "or", "r27,r28,r28");
        require(0x82106390L, "addi", "r29,r10,0x4a54");
        require(0x821063ecL, "lwz", "r7,-0x18(r29)");
        require(0x821063f4L, "cmpwi", "cr6,r7,0x0");
        require(0x821063f8L, "beq", "cr6,0x8210657c");

        // The outer-loop head consumes the entry-owned r7/r8/r27/r28/r29 state.
        require(0x82106400L, "subi", "r31,r8,0x1");
        require(0x82106408L, "cmpwi", "cr6,r7,0x4");
        require(0x8210640cL, "blt", "cr6,0x821064f4");

        // The configured split consumes a stack scratch value and f0 prepared by
        // the immediately preceding block. It has no independent incoming edge.
        require(0x82106514L, "std", "r9,-0x68(r1)");
        require(0x82106518L, "add", "r8,r10,r8");
        require(0x8210651cL, "addi", "r9,r4,0x1");
        require(0x82106520L, "lfd", "f13,-0x68(r1)");
        requireNoReferences(0x82106520L);
        require(0x82106524L, "fcfid", "f13,f13");
        require(0x82106528L, "frsp", "f13,f13");
        require(0x8210652cL, "fdivs", "f0,f0,f13");

        // The inner interpolation loop and the outer table walk share the same
        // state and return through the entry-owned restore helper.
        require(0x82106530L, "extsw", "r5,r9");
        require(0x82106560L, "bne", "cr6,0x82106530");
        require(0x82106564L, "addi", "r27,r27,0x1");
        require(0x82106570L, "lwzx", "r7,r30,r10");
        require(0x82106574L, "cmpwi", "cr6,r7,0x0");
        require(0x82106578L, "bne", "cr6,0x82106400");
        require(0x8210657cL, "b", "0x82382f34");

        // A new independent saved-frame function begins at 0x82106580.
        require(0x82106580L, "mfspr", "r12,LR");
        require(0x82106584L, "stw", "r12,-0x8(r1)");
        require(0x82106590L, "stwu", "r1,-0x70(r1)");

        println("AC6_82106520_ASSERTIONS=" + assertions);
    }
}
