// Locates the closest .pdata function start at or below each code address.
// Xbox 360 .pdata records are two program-endian u32 words: entry and unwind.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindQualifiedPdataFloor extends GhidraScript {
    private long readU32(Address address) throws Exception {
        return Integer.toUnsignedLong(currentProgram.getMemory().getInt(address));
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindQualifiedPdataFloor <code-address>...");
        }
        MemoryBlock pdata = currentProgram.getMemory().getBlock(".pdata");
        if (pdata == null || !pdata.isInitialized()) {
            throw new IllegalStateException("initialized .pdata block not found");
        }
        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        for (String argument : args) {
            long target = Long.decode(argument) & 0xffffffffL;
            long bestEntry = 0;
            long bestUnwind = 0;
            Address bestRecord = null;
            Address cursor = pdata.getStart();
            Address last = pdata.getEnd().subtract(7);
            while (cursor.compareTo(last) <= 0) {
                long entry = readU32(cursor);
                if (entry <= target && entry > bestEntry &&
                        currentProgram.getMemory().getBlock(toAddr(entry)) != null &&
                        currentProgram.getMemory().getBlock(toAddr(entry)).isExecute()) {
                    bestEntry = entry;
                    bestUnwind = readU32(cursor.add(4));
                    bestRecord = cursor;
                }
                cursor = cursor.add(8);
            }
            if (bestRecord == null) {
                println(String.format("target=0x%08X no-floor", target));
            } else {
                println(String.format(
                    "target=0x%08X record=%s entry=0x%08X delta=0x%X unwind=0x%08X",
                    target, bestRecord, bestEntry, target - bestEntry, bestUnwind));
            }
        }
    }
}
