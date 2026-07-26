// Read-only assertions for the AC6 PAL 0x821F0AC8 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821F0AC8Boundary extends GhidraScript {
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
        println("AC6_821F0AC8_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821F0AC8_NO_REFERENCES=" + address);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_821F0AC8_FUNCTION_ENTRY=" + function.getEntryPoint());
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function != null) {
            throw new IllegalStateException(address + " unexpectedly starts function " +
                function.getEntryPoint());
        }
        assertions++;
        println("AC6_821F0AC8_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The preceding function has already restored its frame and returned.
        require(0x821f0a80L, "addi", "r1,r1,0x230");
        require(0x821f0a84L, "b", "0x82382f28");
        require(0x821f0a88L, "stw", "r4,0x4090(r3)");
        require(0x821f0a8cL, "blr", "blr");

        // The real entry owns the frame and initializes both loop cursors.
        require(0x821f0a90L, "mfspr", "r12,LR");
        require(0x821f0a94L, "bl", "0x82382ee4");
        require(0x821f0a98L, "stwu", "r1,-0xb0(r1)");
        require(0x821f0a9cL, "or", "r31,r3,r3");
        require(0x821f0aa0L, "lis", "r11,-0x7d97");
        require(0x821f0aa4L, "li", "r30,0x0");
        require(0x821f0aa8L, "addi", "r28,r11,0x1300");
        require(0x821f0aacL, "or", "r29,r30,r30");
        require(0x821f0ab0L, "lwz", "r24,0x350c(r31)");
        require(0x821f0ab4L, "lwz", "r23,0x5708(r31)");

        // First table loop. 0x821F0AC8 only derives an indexed field offset.
        require(0x821f0ab8L, "rlwinm", "r11,r29,0x1e,0x2,0x1f");
        require(0x821f0abcL, "lwz", "r10,0x4(r28)");
        require(0x821f0ac0L, "add", "r9,r29,r31");
        require(0x821f0ac4L, "addi", "r8,r11,0x10");
        require(0x821f0ac8L, "addi", "r11,r11,0x89");
        requireNoReferences(0x821f0ac8L);
        require(0x821f0accL, "rlwinm", "r8,r8,0x2,0x0,0x1d");

        // Preserve neighboring configured 0x821F0AD0; it is not changed here.
        require(0x821f0ad0L, "rlwinm", "r11,r11,0x2,0x0,0x1d");
        require(0x821f0ad4L, "or", "r3,r31,r31");
        require(0x821f0ad8L, "stwx", "r10,r8,r31");
        require(0x821f0adcL, "lwz", "r10,0x0(r28)");
        require(0x821f0ae0L, "stwx", "r10,r11,r31");
        require(0x821f0ae4L, "lwz", "r4,0x8(r28)");
        require(0x821f0ae8L, "lwz", "r11,0x40(r9)");
        require(0x821f0aecL, "mtspr", "CTR,r11");
        require(0x821f0af0L, "bctrl", "bctrl");
        require(0x821f0af4L, "addi", "r29,r29,0x4");
        require(0x821f0af8L, "addi", "r28,r28,0xc");
        require(0x821f0afcL, "cmplwi", "cr6,r29,0x194");
        require(0x821f0b00L, "blt", "cr6,0x821f0ab8");

        // Ghidra recognizes the real entry but none of the loop addresses as a
        // separate function start.
        requireFunctionEntry(0x821f0a90L);
        requireNoFunctionEntry(0x821f0ab8L);
        requireNoFunctionEntry(0x821f0ac8L);
        requireNoFunctionEntry(0x821f0ad0L);
        requireNoFunctionEntry(0x821f0b00L);

        // Execution continues in the same frame into a second bounded loop.
        require(0x821f0b04L, "lis", "r11,-0x7d97");
        require(0x821f0b14L, "or", "r29,r30,r30");
        require(0x821f0b1cL, "rlwinm", "r11,r29,0x1e,0x2,0x1f");
        require(0x821f0b58L, "bctrl", "bctrl");
        require(0x821f0b64L, "cmplwi", "cr6,r29,0x50");
        require(0x821f0b68L, "blt", "cr6,0x821f0b1c");
        require(0x821f0b9cL, "blt", "cr6,0x821f0b14");
        requireNoFunctionEntry(0x821f0b9cL);

        println("AC6_821F0AC8_ASSERTIONS=" + assertions);
    }
}
