// Finds word-aligned occurrences of any supplied 32-bit values.  Kept small
// for correlating virtual-method bodies with nearby vtable/RTTI records.
// @category AC6

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindU32Set extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (getScriptArgs().length == 0) {
            throw new IllegalArgumentException("usage: FindU32Set <value>...");
        }
        Set<Long> expected = new HashSet<>();
        for (String argument : getScriptArgs()) expected.add(Long.decode(argument) & 0xffffffffL);
        byte[] bytes = new byte[4];
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.getSize() < 4) continue;
            Address current = block.getStart();
            Address last = block.getEnd().subtract(3);
            while (current.compareTo(last) <= 0) {
                currentProgram.getMemory().getBytes(current, bytes);
                long value = ((long)(bytes[0] & 0xff) << 24) |
                    ((long)(bytes[1] & 0xff) << 16) |
                    ((long)(bytes[2] & 0xff) << 8) | (long)(bytes[3] & 0xff);
                if (expected.contains(value)) println(current + " -> 0x" + Long.toHexString(value));
                current = current.add(4);
            }
        }
    }
}
