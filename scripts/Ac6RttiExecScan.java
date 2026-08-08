// The sweep skips executable blocks - "vtables live in read-only data". That is
// an assumption, and this measures it: run the identical chain over the
// executable blocks and count what it finds.
// Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;

import java.io.PrintWriter;

public class Ac6RttiExecScan extends GhidraScript {
    private long minimum = Long.MAX_VALUE, maximum = Long.MIN_VALUE;

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isInitialized() || !b.isRead()) continue;
            minimum = Math.min(minimum, b.getStart().getOffset());
            maximum = Math.max(maximum, b.getEnd().getOffset());
        }
        int found = 0, scanned = 0;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || !block.isRead() || !block.isExecute()) continue;
            long start = block.getStart().getOffset(), end = block.getEnd().getOffset();
            for (long at = start; at + 4 <= end && !monitor.isCancelled(); at += 4) {
                scanned++;
                Long word = readU32(at);
                if (word == null || word < minimum || word > maximum) continue;
                String name = chainName(word, at + 4);
                if (name == null) continue;
                found++;
                out.println(String.format("EXEC_VTABLE\t%08x\t%08x\t%s\t%s", at + 4, word,
                        block.getName(), name));
            }
        }
        out.println("EXEC_SUMMARY scanned=" + scanned + " found=" + found);
        out.close();
        println("WROTE exec_found=" + found);
    }

    private String chainName(long locator, long vtable) {
        Long sig = readU32(locator);
        if (sig == null || sig > 1) return null;
        Long type = readU32(locator + 0x0C), hier = readU32(locator + 0x10);
        if (type == null || hier == null) return null;
        if (type < minimum || type > maximum || hier < minimum || hier > maximum) return null;
        String name = readName(type);
        if (name == null || !name.startsWith(".?A")) return null;
        Long slot = readU32(vtable);
        if (slot == null || !isExecutable(slot)) return null;
        return name;
    }

    private boolean isExecutable(long address) {
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (b.isExecute() && address >= b.getStart().getOffset()
                    && address <= b.getEnd().getOffset()) return true;
        }
        return false;
    }

    private String readName(long type) {
        StringBuilder t = new StringBuilder();
        for (int i = 0; i < 200; i++) {
            try {
                byte v = currentProgram.getMemory().getByte(toAddr(type + 8 + i));
                if (v == 0) return t.length() == 0 ? null : t.toString();
                t.append((char) (v & 0xFF));
            } catch (Exception e) { return null; }
        }
        return null;
    }

    private Long readU32(long a) {
        try {
            byte[] b = new byte[4];
            currentProgram.getMemory().getBytes(toAddr(a), b);
            return ((long) (b[0] & 0xFF) << 24) | ((long) (b[1] & 0xFF) << 16)
                    | ((long) (b[2] & 0xFF) << 8) | (long) (b[3] & 0xFF);
        } catch (Exception e) { return null; }
    }
}
