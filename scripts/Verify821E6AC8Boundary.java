// Read-only assertions for the AC6 PAL 0x821E6AC8 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821E6AC8Boundary extends GhidraScript {
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
        println("AC6_821E6AC8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821E6AC8_NO_REFERENCES=" + address);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_821E6AC8_FUNCTION_ENTRY=" + function.getEntryPoint());
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function != null) {
            throw new IllegalStateException(address + " unexpectedly starts function " +
                function.getEntryPoint());
        }
        assertions++;
        println("AC6_821E6AC8_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The sole frame owner and object state precede the delay loop.
        require(0x821e6a88L, "mfspr", "r12,LR");
        require(0x821e6a8cL, "bl", "0x82382efc");
        require(0x821e6a90L, "stwu", "r1,-0x80(r1)");
        require(0x821e6a94L, "or", "r31,r3,r3");
        require(0x821e6a98L, "li", "r11,0x4");
        require(0x821e6a9cL, "lwz", "r29,0x0(r31)");
        require(0x821e6aa0L, "stw", "r11,0x50(r1)");

        // The loop head is eight explicit no-op register moves. The configured
        // 0x821E6AC8 split consumes the counter loaded immediately before it.
        require(0x821e6aa4L, "or", "r31,r31,r31");
        require(0x821e6aa8L, "or", "r31,r31,r31");
        require(0x821e6aacL, "or", "r31,r31,r31");
        require(0x821e6ab0L, "or", "r31,r31,r31");
        require(0x821e6ab4L, "or", "r31,r31,r31");
        require(0x821e6ab8L, "or", "r31,r31,r31");
        require(0x821e6abcL, "or", "r31,r31,r31");
        require(0x821e6ac0L, "or", "r31,r31,r31");
        require(0x821e6ac4L, "lwz", "r11,0x50(r1)");
        require(0x821e6ac8L, "subi", "r11,r11,0x1");
        requireNoReferences(0x821e6ac8L);
        require(0x821e6accL, "stw", "r11,0x50(r1)");
        require(0x821e6ad0L, "lwz", "r11,0x50(r1)");
        require(0x821e6ad4L, "cmplwi", "cr6,r11,0x0");
        require(0x821e6ad8L, "bne", "cr6,0x821e6aa4");

        requireFunctionEntry(0x821e6a88L);
        requireNoFunctionEntry(0x821e6aa4L);
        requireNoFunctionEntry(0x821e6ac8L);
        requireNoFunctionEntry(0x821e6ad8L);

        // Execution continues in the same frame and reaches one shared
        // epilogue; there is no prologue or return at the configured split.
        require(0x821e6adcL, "lbz", "r11,0x2abd(r29)");
        require(0x821e6ae0L, "rlwinm.", "r11,r11,0x0,0x1e,0x1e");
        require(0x821e6ae4L, "bne", "0x821e6b50");
        require(0x821e6af0L, "lwz", "r10,0x8(r31)");
        require(0x821e6b00L, "beq", "cr6,0x821e6b10");
        require(0x821e6b10L, "bl", "0x82390d50");
        require(0x821e6b3cL, "bge", "cr6,0x821e6b48");
        require(0x821e6b44L, "b", "0x821e6b54");
        require(0x821e6b4cL, "bl", "0x821efab0");
        require(0x821e6b50L, "li", "r3,0x0");
        require(0x821e6b54L, "addi", "r1,r1,0x80");
        require(0x821e6b58L, "b", "0x82382f4c");
        requireNoFunctionEntry(0x821e6b50L);
        requireNoFunctionEntry(0x821e6b54L);

        println("AC6_821E6AC8_ASSERTIONS=" + assertions);
    }
}
