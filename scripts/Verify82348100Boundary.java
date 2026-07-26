// Read-only assertions for the AC6 PAL 0x82348100 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify82348100Boundary extends GhidraScript {
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
        println("AC6_82348100_CONTRACT=" + address + " " + instruction);
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
        println("AC6_82348100_NO_REFERENCES=" + address);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_82348100_FUNCTION_ENTRY=" + function.getEntryPoint());
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function != null) {
            throw new IllegalStateException(address + " unexpectedly starts function " +
                function.getEntryPoint());
        }
        assertions++;
        println("AC6_82348100_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The sole frame owner and argument captures precede every candidate label.
        require(0x82348098L, "mfspr", "r12,LR");
        require(0x8234809cL, "bl", "0x82382ef4");
        require(0x823480a0L, "stwu", "r1,-0x80(r1)");
        require(0x823480a4L, "or", "r29,r3,r3");
        require(0x823480a8L, "or", "r30,r5,r5");
        require(0x823480acL, "cmplwi", "cr6,r4,0x0");

        // Initial allocation establishes r28/r27 before entering the shared tail.
        require(0x823480c8L, "rlwinm", "r28,r11,0x0,0x0,0x19");
        require(0x823480e4L, "or", "r4,r28,r28");
        require(0x823480e8L, "or", "r27,r28,r28");
        require(0x823480ecL, "b", "0x8234814c");

        // 0x823480F0 is the internal retry target. 0x82348100 consumes the
        // quotient inputs loaded and checked immediately before it.
        require(0x823480f0L, "cmplwi", "cr6,r30,0x40");
        require(0x823480f4L, "ble", "cr6,0x8234817c");
        require(0x823480f8L, "lwz", "r11,0x10(r31)");
        require(0x823480fcL, "twllei", "r30,0x0");
        require(0x82348100L, "divwu", "r10,r11,r30");
        requireNoReferences(0x82348100L);
        require(0x82348104L, "mullw", "r10,r10,r30");
        require(0x82348108L, "subf.", "r10,r10,r11");
        require(0x8234810cL, "beq", "0x8234817c");
        require(0x82348114L, "lwz", "r9,0x14(r31)");
        require(0x82348120L, "or", "r4,r31,r31");
        require(0x82348128L, "or", "r3,r29,r29");
        require(0x82348134L, "add", "r11,r5,r28");
        require(0x82348138L, "cmplw", "cr6,r11,r9");
        require(0x8234813cL, "ble", "cr6,0x82348168");
        require(0x82348140L, "bl", "0x82347d90");
        require(0x82348144L, "add", "r27,r27,r30");
        require(0x82348148L, "or", "r4,r27,r27");
        require(0x8234814cL, "or", "r3,r29,r29");
        require(0x82348150L, "bl", "0x82347f50");
        require(0x82348154L, "or.", "r31,r3,r3");
        require(0x82348158L, "bne", "0x823480f0");

        requireFunctionEntry(0x82348098L);
        requireNoFunctionEntry(0x823480f0L);
        requireNoFunctionEntry(0x82348100L);
        requireNoFunctionEntry(0x82348158L);

        // Both retry exits continue in the original 0x80-byte frame, which is
        // restored only at the shared epilogue.
        require(0x82348168L, "li", "r6,0x1");
        require(0x8234817cL, "lwz", "r11,0x14(r31)");
        require(0x823481ccL, "stw", "r31,0xe08(r29)");
        require(0x823481d0L, "lwz", "r3,0x10(r31)");
        require(0x823481d4L, "b", "0x82348160");
        require(0x82348160L, "addi", "r1,r1,0x80");
        require(0x82348164L, "b", "0x82382f44");
        requireNoFunctionEntry(0x8234817cL);
        requireNoFunctionEntry(0x823481d4L);

        println("AC6_82348100_ASSERTIONS=" + assertions);
    }
}
