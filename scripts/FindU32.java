import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindU32 extends GhidraScript {
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
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: FindU32 <value>");
        }
        long expected = Long.decode(args[0]) & 0xffffffffL;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.getSize() < 4) {
                continue;
            }
            Address current = block.getStart();
            Address last = block.getEnd().subtract(3);
            while (current.compareTo(last) <= 0) {
                if (readU32(current) == expected) {
                    println(current.toString());
                }
                current = current.add(4);
            }
        }
    }
}
