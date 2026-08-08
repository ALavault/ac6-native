// Reproduce the SweepMsvcRtti scan and classify every rejection.
//
// The 1,619 rejects of cycle 1115 were counted by one predicate and never
// examined. This script re-runs the identical scan and, for each candidate the
// reject predicate accepted but describe() refused, records which step refused
// it and enough context to tell coincidence from a lost class.
//
// Read-only. Never writes to the database.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class Ac6RttiRejects extends GhidraScript {

    private long minimum = Long.MAX_VALUE;
    private long maximum = Long.MIN_VALUE;

    @Override
    public void run() throws Exception {
        PrintWriter out = new PrintWriter(getScriptArgs()[0], "UTF-8");
        List<MemoryBlock> blocks = new ArrayList<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || !block.isRead()) continue;
            blocks.add(block);
            minimum = Math.min(minimum, block.getStart().getOffset());
            maximum = Math.max(maximum, block.getEnd().getOffset());
        }
        out.println("# image range " + Long.toHexString(minimum) + ".." + Long.toHexString(maximum));
        int emitted = 0, rejected = 0, scanned = 0, inRange = 0;
        for (MemoryBlock block : blocks) {
            if (block.isExecute()) continue;
            long start = block.getStart().getOffset();
            long end = block.getEnd().getOffset();
            for (long at = start; at + 4 <= end && !monitor.isCancelled(); at += 4) {
                scanned++;
                Long candidate = readU32(at);
                if (candidate == null || candidate < minimum || candidate > maximum) continue;
                inRange++;
                if (accepts(candidate, at + 4)) { emitted++; continue; }
                if (!looksLikeLocator(candidate)) continue;
                rejected++;
                out.println(String.format("REJECT\t%08x\t%08x\t%s\t%s\t%s\t%s",
                        at, candidate, reason(candidate, at + 4), block.getName(),
                        nameOrDash(candidate), slotDescription(at + 4)));
            }
        }
        out.println(String.format("SUMMARY scanned=%d in_range=%d vtables=%d rejected=%d",
                scanned, inRange, emitted, rejected));
        out.close();
        println("WROTE vtables=" + emitted + " rejected=" + rejected);
    }

    // The exact steps of SweepMsvcRtti.describe(), in its order.
    private String reason(long locator, long vtable) {
        Long signature = readU32(locator);
        if (signature == null || signature > 1) return "signature";
        Long typeDescriptor = readU32(locator + 0x0C);
        Long hierarchy = readU32(locator + 0x10);
        if (typeDescriptor == null || hierarchy == null) return "unreadable";
        if (typeDescriptor < minimum || typeDescriptor > maximum) return "type_out_of_range";
        if (hierarchy < minimum || hierarchy > maximum) return "hierarchy_out_of_range";
        String name = readName(typeDescriptor);
        if (name == null) return "name_unreadable";
        if (!name.startsWith(".?A")) return "name_not_rtti";
        Long slot = readU32(vtable);
        if (slot == null) return "slot_unreadable";
        if (!isExecutable(slot)) return "slot_not_code";
        return "accepted";
    }

    private boolean accepts(long locator, long vtable) {
        return "accepted".equals(reason(locator, vtable));
    }

    private String nameOrDash(long locator) {
        Long typeDescriptor = readU32(locator + 0x0C);
        if (typeDescriptor == null || typeDescriptor < minimum || typeDescriptor > maximum) return "-";
        String name = readName(typeDescriptor);
        return name == null ? "-" : name.replace('\t', ' ');
    }

    private String slotDescription(long vtable) {
        Long slot = readU32(vtable);
        if (slot == null) return "-";
        return String.format("%08x", slot);
    }

    private boolean looksLikeLocator(long locator) {
        Long signature = readU32(locator);
        Long descriptor = readU32(locator + 0x0C);
        return signature != null && signature <= 1 && descriptor != null
                && descriptor >= minimum && descriptor <= maximum;
    }

    private boolean isExecutable(long address) {
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (block.isExecute() && address >= block.getStart().getOffset()
                    && address <= block.getEnd().getOffset()) return true;
        }
        return false;
    }

    private String readName(long typeDescriptor) {
        StringBuilder text = new StringBuilder();
        for (int index = 0; index < 200; index++) {
            try {
                byte value = currentProgram.getMemory().getByte(toAddr(typeDescriptor + 8 + index));
                if (value == 0) return text.length() == 0 ? null : text.toString();
                text.append((char) (value & 0xFF));
            } catch (Exception failure) { return null; }
        }
        return null;
    }

    private Long readU32(long address) {
        try {
            byte[] bytes = new byte[4];
            currentProgram.getMemory().getBytes(toAddr(address), bytes);
            return ((long) (bytes[0] & 0xFF) << 24) | ((long) (bytes[1] & 0xFF) << 16)
                    | ((long) (bytes[2] & 0xFF) << 8) | (long) (bytes[3] & 0xFF);
        } catch (Exception failure) { return null; }
    }
}
