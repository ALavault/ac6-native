// Compares two bounded program-endian u32 tables by index.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class CompareQualifiedWordTables extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: CompareQualifiedWordTables <start-a> <start-b> <count>");
        }
        Address startA = toAddr(Long.decode(args[0]));
        Address startB = toAddr(Long.decode(args[1]));
        int count = Integer.parseUnsignedInt(args[2]);
        if (count < 1 || count > 4096) {
            throw new IllegalArgumentException("count must be in 1..4096");
        }

        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        println("a=" + startA + " b=" + startB + " count=" + count);
        int equal = 0;
        int different = 0;
        for (int index = 0; index < count; ++index) {
            Address addressA = startA.add((long)index * 4L);
            Address addressB = startB.add((long)index * 4L);
            int valueA = currentProgram.getMemory().getInt(addressA);
            int valueB = currentProgram.getMemory().getInt(addressB);
            if (valueA == valueB) {
                equal++;
            } else {
                different++;
                printf("index=%d offset=0x%X %s=%08X %s=%08X%n",
                    index, index * 4, addressA, valueA, addressB, valueB);
            }
        }
        println("SUMMARY equal=" + equal + " different=" + different);
    }
}
