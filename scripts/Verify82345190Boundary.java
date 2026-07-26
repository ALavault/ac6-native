// Read-only assertions for the AC6 PAL 0x82345190 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82345190Boundary extends GhidraScript {
    private int assertions;

    private void require(long value, String mnemonic, String fragment) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            Disassembler.getDisassembler(currentProgram, monitor, null).disassemble(address, null);
            instruction = currentProgram.getListing().getInstructionAt(address);
        }
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                !instruction.toString().contains(fragment)) {
            throw new IllegalStateException(address + " expected " + mnemonic +
                " containing " + fragment + " but found " +
                (instruction == null ? "<none>" : instruction));
        }
        assertions++;
        println("AC6_82345190_CONTRACT=" + address + " " + instruction);
    }

    private void requireNoReferences(long value) {
        Address address = toAddr(value);
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(address);
        if (references.hasNext()) {
            throw new IllegalStateException(address + " has an incoming reference: " +
                references.next());
        }
        assertions++;
        println("AC6_82345190_NO_REFERENCES=" + address);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        if (currentProgram.getFunctionManager().getFunctionAt(address) == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_82345190_FUNCTION_ENTRY=" + address);
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        if (currentProgram.getFunctionManager().getFunctionAt(address) != null) {
            throw new IllegalStateException(address + " unexpectedly starts a function");
        }
        assertions++;
        println("AC6_82345190_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The sole ABI entry establishes the 0x120-byte frame and all outer
        // loop registers consumed at and after the configured split.
        require(0x82345100L, "mfspr", "r12,LR");
        require(0x82345104L, "bl", "0x82382ee0");
        require(0x82345108L, "stwu", "r1,-0x120(r1)");
        require(0x82345118L, "or", "r25,r3,r3");
        require(0x82345124L, "or", "r31,r25,r25");
        require(0x82345128L, "li", "r26,0x3");
        require(0x82345134L, "or", "r29,r11,r11");
        require(0x82345138L, "li", "r27,0x6");
        require(0x8234513cL, "addi", "r30,r22,0x2");
        require(0x82345140L, "li", "r28,0x8");

        // The first nested loop reaches 0x82345190 by straight-line flow. The
        // branch consumes CR0 set immediately before it and has no ABI setup.
        require(0x82345144L, "lhz", "r9,0x0(r30)");
        require(0x82345148L, "addi", "r11,r1,0x50");
        require(0x82345150L, "cmplwi", "r9,0x0");
        require(0x82345154L, "beq", "0x82345184");
        require(0x8234515cL, "lwz", "r8,0x0(r10)");
        require(0x82345180L, "bne", "0x8234515c");
        require(0x82345184L, "lhz", "r9,-0x2(r29)");
        require(0x82345188L, "lwz", "r10,0x0(r29)");
        require(0x8234518cL, "cmplwi", "r9,0x0");
        require(0x82345190L, "beq", "0x823451d4");
        requireNoReferences(0x82345190L);
        require(0x82345194L, "lwz", "r8,0x0(r10)");
        require(0x823451c4L, "subic.", "r9,r9,0x1");
        require(0x823451d0L, "bne", "0x82345194");

        // The split shares the same stack scratch area, declaration sources,
        // nested loop counters, output cursor and one final epilogue.
        require(0x823451d4L, "lhz", "r10,-0x4(r29)");
        require(0x823451dcL, "addi", "r3,r1,0x50");
        require(0x82345200L, "bl", "0x821de898");
        require(0x82345204L, "stw", "r3,0x0(r31)");
        require(0x82345208L, "subic.", "r28,r28,0x1");
        require(0x82345214L, "bne", "0x82345144");
        require(0x82345218L, "subic.", "r27,r27,0x1");
        require(0x82345220L, "bne", "0x8234513c");
        require(0x82345224L, "subic.", "r26,r26,0x1");
        require(0x8234522cL, "bne", "0x82345134");
        require(0x82345328L, "addi", "r1,r1,0x120");
        require(0x8234532cL, "b", "0x82382f30");

        requireFunctionEntry(0x82345100L);
        requireNoFunctionEntry(0x82345190L);
        requireNoFunctionEntry(0x82345144L);
        requireNoFunctionEntry(0x82345214L);
        requireNoFunctionEntry(0x82345328L);

        println("AC6_82345190_ASSERTIONS=" + assertions);
    }
}
