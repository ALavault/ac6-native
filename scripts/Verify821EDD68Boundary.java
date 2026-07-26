// Read-only assertions for the AC6 PAL 0x821EDD68 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821EDD68Boundary extends GhidraScript {
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
        println("AC6_821EDD68_CONTRACT=" + address + " " + instruction);
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
        println("AC6_821EDD68_NO_REFERENCES=" + address);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_821EDD68_FUNCTION_ENTRY=" + function.getEntryPoint());
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function != null) {
            throw new IllegalStateException(address + " unexpectedly starts function " +
                function.getEntryPoint());
        }
        assertions++;
        println("AC6_821EDD68_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The sole function entry derives the cache-line span and both loop
        // counters before the configured address is reached.
        require(0x821edd28L, "lis", "r11,0x7ef");
        require(0x821edd2cL, "subis", "r10,r3,0x7f10");
        require(0x821edd30L, "ori", "r11,r11,0xffff");
        require(0x821edd34L, "cmplw", "cr6,r10,r11");
        require(0x821edd38L, "blelr", "cr6");
        require(0x821edd3cL, "addi", "r10,r4,0x7f");
        require(0x821edd40L, "rlwinm", "r11,r3,0x0,0x0,0x18");
        require(0x821edd44L, "rlwinm", "r10,r10,0x0,0x0,0x18");
        require(0x821edd48L, "subf", "r10,r11,r10");
        require(0x821edd4cL, "srawi", "r10,r10,0x7");
        require(0x821edd50L, "addze", "r10,r10");
        require(0x821edd54L, "rlwinm.", "r9,r10,0x1d,0x3,0x1f");
        require(0x821edd58L, "rlwinm", "r10,r10,0x0,0x1d,0x1f");
        require(0x821edd5cL, "beq", "0x821edda8");

        // 0x821EDD60 is the eight-line cache-flush loop head. The configured
        // 0x821EDD68 split is its second dcbf and depends on r8=128 loaded at
        // 0x821EDD64; it has neither an incoming reference nor its own entry.
        require(0x821edd60L, "dcbf", "0,r11");
        require(0x821edd64L, "li", "r8,0x80");
        require(0x821edd68L, "dcbf", "r8,r11");
        requireNoReferences(0x821edd68L);
        require(0x821edd6cL, "li", "r8,0x100");
        require(0x821edd70L, "dcbf", "r8,r11");
        require(0x821edd74L, "li", "r8,0x180");
        require(0x821edd78L, "dcbf", "r8,r11");
        require(0x821edd7cL, "li", "r8,0x200");
        require(0x821edd80L, "dcbf", "r8,r11");
        require(0x821edd84L, "li", "r8,0x280");
        require(0x821edd88L, "dcbf", "r8,r11");
        require(0x821edd8cL, "li", "r8,0x300");
        require(0x821edd90L, "dcbf", "r8,r11");
        require(0x821edd94L, "li", "r8,0x380");
        require(0x821edd98L, "dcbf", "r8,r11");
        require(0x821edd9cL, "subic.", "r9,r9,0x1");
        require(0x821edda0L, "addi", "r11,r11,0x400");
        require(0x821edda4L, "bne", "0x821edd60");

        requireFunctionEntry(0x821edd28L);
        requireNoFunctionEntry(0x821edd60L);
        requireNoFunctionEntry(0x821edd68L);
        requireNoFunctionEntry(0x821edda4L);

        // The residual-line loop and the synchronization/return remain part
        // of the same leaf function; there is no prologue or return at 0x68.
        require(0x821edda8L, "cmplwi", "cr6,r10,0x0");
        require(0x821eddacL, "beq", "cr6,0x821eddc0");
        require(0x821eddb0L, "dcbf", "0,r11");
        require(0x821eddb4L, "subic.", "r10,r10,0x1");
        require(0x821eddb8L, "addi", "r11,r11,0x80");
        require(0x821eddbcL, "bne", "0x821eddb0");
        require(0x821eddc0L, "sync", "0x0");
        require(0x821eddc4L, "blr", "blr");
        requireNoFunctionEntry(0x821edda8L);
        requireNoFunctionEntry(0x821eddb0L);
        requireNoFunctionEntry(0x821eddc0L);

        println("AC6_821EDD68_ASSERTIONS=" + assertions);
    }
}
