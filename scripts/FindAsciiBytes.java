// Finds exact ASCII byte sequences in every initialized memory block.
// Read-only helper for locating message ids and other binary literals.
// @category AC6.Evidence

import java.nio.charset.StandardCharsets;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindAsciiBytes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException("usage: FindAsciiBytes <text>...");
        }
        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        for (String value : args) {
            byte[] needle = value.getBytes(StandardCharsets.US_ASCII);
            int hits = 0;
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                if (!block.isInitialized()) continue;
                Address cursor = block.getStart();
                while (cursor != null && cursor.compareTo(block.getEnd()) <= 0) {
                    Address found = currentProgram.getMemory().findBytes(
                        cursor, block.getEnd(), needle, null, true, monitor);
                    if (found == null) break;
                    println("text=" + value + " address=" + found +
                        " block=" + block.getName());
                    hits++;
                    cursor = found.next();
                }
            }
            println("summary text=" + value + " hits=" + hits);
        }
    }
}
