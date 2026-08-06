// Dump a bounded sequence of program-endian 32-bit words from the current
// Ghidra program. Intended for headless, read-only evidence queries.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpQualifiedWords extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            printerr("usage: DumpQualifiedWords.java <start-address> <word-count>");
            return;
        }

        Address start = toAddr(Long.decode(args[0]));
        int count = Integer.parseUnsignedInt(args[1]);
        if (count <= 0 || count > 4096) {
            printerr("word-count must be in 1..4096");
            return;
        }

        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        println("start=" + start + " count=" + count);
        for (int index = 0; index < count; ++index) {
            Address address = start.add((long) index * 4L);
            int value = currentProgram.getMemory().getInt(address);
            printf("%s %08X%n", address, value);
        }
    }
}
