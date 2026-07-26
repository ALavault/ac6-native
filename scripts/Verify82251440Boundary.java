// Read-only assertions for the AC6 PAL 0x82251440 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82251440Boundary extends GhidraScript {
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
        println("AC6_82251440_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82251440_NO_REFERENCES=" + address);
    }

    @Override
    public void run() throws Exception {
        // The prior function returns before this leaf initializer begins.
        require(0x82251374L, "addi", "r1,r1,0x70");
        require(0x82251378L, "b", "0x82382f4c");
        require(0x82251380L, "addi", "r1,r1,0x70");
        require(0x82251384L, "b", "0x82382f4c");

        // The real entry initializes all loop-carried pointers and constants.
        require(0x82251388L, "lis", "r11,-0x7e00");
        require(0x82251390L, "li", "r7,0x2");
        require(0x82251394L, "addi", "r9,r3,0x84");
        require(0x8225139cL, "lfs", "f0,0x82c(r11)");
        require(0x822513acL, "addi", "r10,r11,0x4e18");
        require(0x822513b0L, "li", "r11,0x0");
        require(0x822513b4L, "li", "r8,0x1");
        require(0x822513ccL, "addi", "r10,r9,0xc");
        require(0x822513e8L, "addi", "r7,r7,0x4e4c");

        // The true loop head and body consume only entry-owned state.
        require(0x822513ecL, "stw", "r5,0x0(r9)");
        require(0x822513f0L, "subi", "r8,r8,0x1");
        require(0x822513f8L, "addi", "r9,r9,0x44");
        require(0x82251400L, "cmpwi", "cr6,r8,0x0");
        require(0x8225140cL, "stw", "r6,-0x8(r10)");
        require(0x82251420L, "stw", "r7,0xc(r10)");
        require(0x8225143cL, "addi", "r10,r10,0x44");

        // This configured address is only the loop backedge. Its condition was
        // set earlier in the same body and it has no incoming reference.
        require(0x82251440L, "bge", "cr6,0x822513ec");
        requireNoReferences(0x82251440L);

        // Fallthrough continues the same leaf initializer and uses its values.
        require(0x82251444L, "lis", "r10,-0x7dfa");
        require(0x82251448L, "stfs", "f0,0x114(r3)");
        require(0x82251450L, "stw", "r11,0x10c(r3)");
        require(0x82251464L, "li", "r7,0xf");
        require(0x82251474L, "li", "r8,0xb");
        require(0x822514b4L, "li", "r10,0xd");
        require(0x822514e0L, "stw", "r7,0x6c(r3)");
        require(0x822514f4L, "stw", "r10,0x80(r3)");
        require(0x822514f8L, "blr", "blr");

        // A separate framed function begins at the next aligned prologue.
        require(0x82251500L, "mfspr", "r12,LR");
        require(0x82251504L, "stw", "r12,-0x8(r1)");
        require(0x82251508L, "std", "r31,-0x10(r1)");
        require(0x8225150cL, "stwu", "r1,-0x60(r1)");

        println("AC6_82251440_ASSERTIONS=" + assertions);
    }
}
