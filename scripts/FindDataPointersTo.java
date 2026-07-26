// Finds exact big-endian 32-bit pointers in initialized non-executable memory.
// Read-only diagnostic; a hit is not promoted to a vtable or field without
// surrounding layout and receiver evidence.
// @category AC6

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindDataPointersTo extends GhidraScript {
    private long readU32(Address address) throws Exception {
        byte[] bytes = new byte[4];
        currentProgram.getMemory().getBytes(address, bytes);
        return ((long)(bytes[0] & 0xff) << 24) |
               ((long)(bytes[1] & 0xff) << 16) |
               ((long)(bytes[2] & 0xff) << 8) |
               (long)(bytes[3] & 0xff);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindDataPointersTo <address>...");
        }

        Set<Long> targets = new HashSet<>();
        for (String argument : args) {
            targets.add(Long.decode(argument) & 0xffffffffL);
        }

        long scannedWords = 0;
        long hits = 0;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.isExecute() || block.getSize() < 4) {
                continue;
            }
            Address current = block.getStart();
            Address last = block.getEnd().subtract(3);
            while (current.compareTo(last) <= 0) {
                scannedWords++;
                long value = readU32(current);
                if (targets.contains(value)) {
                    hits++;
                    println(String.format(
                        "%s -> 0x%08x block=%s",
                        current, value, block.getName()));
                }
                current = current.add(4);
            }
        }
        println(String.format(
            "SUMMARY targets=%d scanned_words=%d hits=%d",
            targets.size(), scannedWords, hits));
    }
}
