// A null model for the RTTI sweep's reject count.
//
// The claim to be tested is that the 1,619 rejects are coincidences of a loose
// pre-filter rather than near-miss vtables. If so, perturbing the candidate
// pointer by a constant - which destroys every genuine locator relationship
// while leaving the data's statistics alone - must collapse the accepted count
// to almost nothing and leave the reject count roughly where it was.
//
// Read-only. @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class Ac6RttiNullModel extends GhidraScript {
    private long minimum = Long.MAX_VALUE, maximum = Long.MIN_VALUE;

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        List<MemoryBlock> blocks = new ArrayList<>();
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isInitialized() || !b.isRead()) continue;
            blocks.add(b);
            minimum = Math.min(minimum, b.getStart().getOffset());
            maximum = Math.max(maximum, b.getEnd().getOffset());
        }
        long[] shifts = {0, 0x10, 0x40, 0x100, 0x400, 0x1000, 0x10000};
        for (long shift : shifts) {
            int accepted = 0, rejected = 0;
            for (MemoryBlock block : blocks) {
                if (block.isExecute()) continue;
                long start = block.getStart().getOffset(), end = block.getEnd().getOffset();
                for (long at = start; at + 4 <= end && !monitor.isCancelled(); at += 4) {
                    Long word = readU32(at);
                    if (word == null || word < minimum || word > maximum) continue;
                    long candidate = word + shift;
                    if (candidate < minimum || candidate > maximum) continue;
                    if (accepts(candidate, at + 4)) { accepted++; continue; }
                    if (looksLikeLocator(candidate)) rejected++;
                }
            }
            out.println(String.format("SHIFT %#x accepted=%d rejected=%d", shift, accepted, rejected));
            out.flush();
        }
        out.close();
        println("WROTE");
    }

    private boolean accepts(long locator, long vtable) {
        Long sig = readU32(locator);
        if (sig == null || sig > 1) return false;
        Long type = readU32(locator + 0x0C), hier = readU32(locator + 0x10);
        if (type == null || hier == null) return false;
        if (type < minimum || type > maximum || hier < minimum || hier > maximum) return false;
        String name = readName(type);
        if (name == null || !name.startsWith(".?A")) return false;
        Long slot = readU32(vtable);
        return slot != null && isExecutable(slot);
    }

    private boolean looksLikeLocator(long locator) {
        Long sig = readU32(locator), desc = readU32(locator + 0x0C);
        return sig != null && sig <= 1 && desc != null && desc >= minimum && desc <= maximum;
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

    private Long readU32(long address) {
        try {
            byte[] b = new byte[4];
            currentProgram.getMemory().getBytes(toAddr(address), b);
            return ((long) (b[0] & 0xFF) << 24) | ((long) (b[1] & 0xFF) << 16)
                    | ((long) (b[2] & 0xFF) << 8) | (long) (b[3] & 0xFF);
        } catch (Exception e) { return null; }
    }
}
