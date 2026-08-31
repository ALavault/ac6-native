// Finds an exact, word-aligned, program-endian u32 sequence in initialized
// memory. This is a read-only candidate finder; callers must qualify the
// program identity and the consumers of every match separately.
//
// Usage: FindU32Sequence.java <value>...
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class FindU32Sequence extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments.length == 0 || arguments.length > 4096) {
            throw new IllegalArgumentException(
                "usage: FindU32Sequence <value>... (maximum 4096 values)");
        }

        int[] expected = new int[arguments.length];
        for (int index = 0; index < arguments.length; ++index) {
            expected[index] = (int) (Long.decode(arguments[index]) & 0xffffffffL);
        }

        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        int matches = 0;
        long byteLength = (long) expected.length * 4L;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.getSize() < byteLength) {
                continue;
            }
            Address current = block.getStart();
            Address last = block.getEnd().subtract(byteLength - 1L);
            while (current.compareTo(last) <= 0) {
                boolean equal = true;
                for (int index = 0; index < expected.length; ++index) {
                    if (currentProgram.getMemory().getInt(
                            current.add((long) index * 4L)) != expected[index]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    println("AC6_U32_SEQUENCE start=" + current
                        + " words=" + expected.length);
                    for (int index = 0; index < expected.length; ++index) {
                        Address target = current.add((long) index * 4L);
                        ReferenceIterator references = currentProgram
                            .getReferenceManager().getReferencesTo(target);
                        while (references.hasNext()) {
                            Reference reference = references.next();
                            Address from = reference.getFromAddress();
                            Function function = currentProgram.getFunctionManager()
                                .getFunctionContaining(from);
                            Instruction instruction = currentProgram.getListing()
                                .getInstructionAt(from);
                            println("AC6_U32_SEQUENCE_REF target=" + target
                                + " from=" + from
                                + " function=" + (function == null
                                    ? "none" : function.getEntryPoint())
                                + " type=" + reference.getReferenceType()
                                + " instruction=" + (instruction == null
                                    ? "none" : instruction));
                        }
                    }
                    ++matches;
                }
                current = current.add(4L);
            }
        }
        println("AC6_U32_SEQUENCE_TOTAL " + matches);
    }
}
