// Print big-endian 32-bit words from an address range for bounded XEX table inspection.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpDataWords extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: DumpDataWords <address> <word-count>");
        }
        Address address = toAddr(Long.decode(args[0]));
        int wordCount = Integer.decode(args[1]);
        for (int index = 0; index < wordCount; index++) {
            Address wordAddress = address.add((long) index * 4L);
            int value = currentProgram.getMemory().getInt(wordAddress);
            println(wordAddress + " 0x" + String.format("%08x", value));
        }
    }
}
