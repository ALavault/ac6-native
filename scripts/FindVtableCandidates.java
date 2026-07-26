import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.MemoryBlock;

public class FindVtableCandidates extends GhidraScript {
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
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || block.isExecute()) {
                continue;
            }
            Address current = block.getStart();
            Address last = block.getEnd().subtract(0x28);
            while (current.compareTo(last) <= 0) {
                boolean candidate = true;
                long[] pointers = new long[10];
                for (int slot = 0; slot < pointers.length; ++slot) {
                    pointers[slot] = readU32(current.add(slot * 4L));
                    Address target = toAddr(pointers[slot]);
                    if (currentProgram.getFunctionManager().getFunctionAt(target) == null) {
                        candidate = false;
                        break;
                    }
                }
                if (candidate) {
                    println(current + String.format(
                        " slot10=%08x slot18=%08x slot1c=%08x slot24=%08x",
                        pointers[4], pointers[6], pointers[7], pointers[9]));
                }
                current = current.add(4);
            }
        }
    }
}
