// Read-only assertions for the AC6 PAL 0x821DE7E8 configured split.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class Verify821DE7E8Boundary extends GhidraScript {
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
        println("AC6_821DE7E8_CONTRACT=" + address + " " + instruction);
    }

    private void requireOnlyReference(long sourceValue, long targetValue) {
        Address source = toAddr(sourceValue);
        Address target = toAddr(targetValue);
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(target);
        int count = 0;
        while (references.hasNext()) {
            Reference reference = references.next();
            if (!reference.getFromAddress().equals(source)) {
                throw new IllegalStateException(target + " unexpected reference from " +
                    reference.getFromAddress());
            }
            count++;
        }
        if (count != 1) {
            throw new IllegalStateException(target + " expected exactly one reference from " +
                source + " but found " + count);
        }
        assertions++;
        println("AC6_821DE7E8_ONLY_REFERENCE=" + source + " -> " + target);
    }

    private void requireReference(long sourceValue, long targetValue) {
        Address source = toAddr(sourceValue);
        Address target = toAddr(targetValue);
        for (Reference reference :
                currentProgram.getReferenceManager().getReferencesFrom(source)) {
            if (reference.getToAddress().equals(target)) {
                assertions++;
                println("AC6_821DE7E8_REFERENCE=" + source + " -> " + target);
                return;
            }
        }
        throw new IllegalStateException(source + " has no reference to " + target);
    }

    private void requireFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function == null) {
            throw new IllegalStateException(address + " expected function entry");
        }
        assertions++;
        println("AC6_821DE7E8_FUNCTION_ENTRY=" + function.getEntryPoint());
    }

    private void requireNoFunctionEntry(long value) {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(address);
        if (function != null) {
            throw new IllegalStateException(address + " unexpectedly starts function " +
                function.getEntryPoint());
        }
        assertions++;
        println("AC6_821DE7E8_NO_FUNCTION_ENTRY=" + address);
    }

    @Override
    public void run() throws Exception {
        // The sole ABI entry establishes the frame and loop registers consumed
        // by 0x821DE7E8. The configured split has no self-contained prologue.
        require(0x821de7a8L, "mfspr", "r12,LR");
        require(0x821de7acL, "bl", "0x82382ef4");
        require(0x821de7b0L, "stwu", "r1,-0x90(r1)");
        require(0x821de7b8L, "or", "r29,r3,r3");
        require(0x821de7bcL, "or", "r31,r4,r4");
        require(0x821de7c0L, "or", "r10,r29,r29");
        require(0x821de7c4L, "or", "r30,r27,r27");
        require(0x821de7c8L, "std", "r27,0x50(r1)");
        require(0x821de7ccL, "or", "r28,r27,r27");
        require(0x821de7d0L, "lhz", "r11,0x0(r29)");
        require(0x821de7d8L, "b", "0x821de800");

        // 0x821DE7E8 is merely the conditional fall-through target inside the
        // declaration scan. Its only incoming reference is from 0x821DE7E0.
        require(0x821de7dcL, "cmplw", "cr6,r28,r11");
        require(0x821de7e0L, "bgt", "cr6,0x821de7e8");
        requireOnlyReference(0x821de7e0L, 0x821de7e8L);
        require(0x821de7e4L, "or", "r28,r11,r11");
        require(0x821de7e8L, "addi", "r9,r1,0x50");
        require(0x821de7ecL, "li", "r8,0xff");
        require(0x821de7f0L, "addi", "r10,r10,0xc");
        require(0x821de7f4L, "addi", "r30,r30,0x1");
        require(0x821de7f8L, "stbx", "r8,r11,r9");
        require(0x821de7fcL, "lhz", "r11,0x0(r10)");
        require(0x821de800L, "cmplwi", "cr6,r11,0xff");
        require(0x821de804L, "bne", "cr6,0x821de7dc");
        requireReference(0x821de804L, 0x821de7dcL);

        // State computed across the split feeds one allocation/copy path and
        // one epilogue owned by 0x821DE7A8.
        require(0x821de808L, "mulli", "r11,r30,0xc");
        require(0x821de814L, "or", "r3,r31,r31");
        require(0x821de818L, "bl", "0x823835d0");
        require(0x821de824L, "stw", "r30,0x18(r31)");
        require(0x821de82cL, "stw", "r28,0x1c(r31)");
        require(0x821de838L, "cmplwi", "cr6,r30,0x0");
        require(0x821de858L, "beq", "cr6,0x821de88c");
        require(0x821de85cL, "or", "r11,r29,r29");
        require(0x821de860L, "addi", "r10,r31,0x34");
        require(0x821de888L, "bne", "0x821de864");
        requireReference(0x821de888L, 0x821de864L);
        require(0x821de88cL, "addi", "r1,r1,0x90");
        require(0x821de890L, "b", "0x82382f44");

        requireFunctionEntry(0x821de7a8L);
        requireNoFunctionEntry(0x821de7e8L);
        requireNoFunctionEntry(0x821de7dcL);
        requireNoFunctionEntry(0x821de804L);
        requireNoFunctionEntry(0x821de88cL);

        // Record but do not classify the neighboring configured address.
        require(0x821de8d8L, "addi", "r3,r11,0x38");

        println("AC6_821DE7E8_ASSERTIONS=" + assertions);
    }
}
