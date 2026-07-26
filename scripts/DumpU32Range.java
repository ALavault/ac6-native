// Dump big-endian 32-bit values from a program address range.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpU32Range extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            printerr("usage: DumpU32Range.java START END");
            return;
        }
        Address start = toAddr(args[0]);
        Address end = toAddr(args[1]);
        for (Address cursor = start; cursor.compareTo(end) < 0; cursor = cursor.add(4)) {
            int value = currentProgram.getMemory().getInt(cursor, true);
            println(String.format("%s 0x%08x", cursor, value));
        }
    }
}
