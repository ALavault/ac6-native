// Scan an exact data-pointer target and test surrounding words for an MSVC
// vtable/Complete Object Locator.  This is deliberately read-only: a target
// function pointer in a data table is not promoted to a vtable without a
// valid locator and type descriptor.
//
// usage: ScanPointerRtti OUT TARGET [backWords=64] [forwardWords=64]
// @category AC6.Evidence

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ScanPointerRtti extends GhidraScript {
    private PrintWriter out;

    private long readU32(long value) throws Exception {
        byte[] bytes = new byte[4];
        currentProgram.getMemory().getBytes(toAddr(value & 0xffffffffL), bytes);
        return ((long)(bytes[0] & 0xff) << 24) |
               ((long)(bytes[1] & 0xff) << 16) |
               ((long)(bytes[2] & 0xff) << 8) |
               (long)(bytes[3] & 0xff);
    }

    private boolean readable(long value) {
        try {
            return currentProgram.getMemory().contains(toAddr(value & 0xffffffffL));
        } catch (Exception failure) {
            return false;
        }
    }

    private boolean validFunctionPointer(long value) {
        if (!readable(value)) return false;
        Address address = toAddr(value & 0xffffffffL);
        Function function = getFunctionAt(address);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        return function != null || instruction != null;
    }

    private String readAscii(long value, int limit) throws Exception {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < limit; ++i) {
            int byteValue = currentProgram.getMemory().getByte(toAddr(value + i)) & 0xff;
            if (byteValue == 0) break;
            if (byteValue < 0x20 || byteValue > 0x7e) return "";
            text.append((char)byteValue);
        }
        return text.toString();
    }

    private boolean validLocator(long locator) {
        try {
            if (!readable(locator) || !readable(locator + 0x14)) return false;
            long signature = readU32(locator);
            long offset = readU32(locator + 4);
            long typeDescriptor = readU32(locator + 0x0c);
            long hierarchy = readU32(locator + 0x10);
            if (signature != 0 && signature != 1) return false;
            if (offset > 0x10000000L) return false;
            if (!readable(typeDescriptor) || !readable(hierarchy)) return false;
            String name = readAscii(typeDescriptor + 8, 128);
            return name.startsWith(".?A");
        } catch (Exception failure) {
            return false;
        }
    }

    private void printRttiCandidates(long hit, int backWords) {
        int found = 0;
        for (int slot = 0; slot <= backWords; ++slot) {
            long vtable = hit - (long)slot * 4;
            long locatorAddress = vtable - 4;
            if (!readable(locatorAddress)) continue;
            try {
                long locator = readU32(locatorAddress);
                if (!validLocator(locator)) continue;
                long typeDescriptor = readU32(locator + 0x0c);
                String name = readAscii(typeDescriptor + 8, 128);
                out.printf("RTTI_CANDIDATE hit=0x%08X vtable=0x%08X slot=%d col=0x%08X type=0x%08X name=%s%n",
                           hit, vtable, slot, locator, typeDescriptor, name);
                found++;
            } catch (Exception ignored) {
                // An unreadable candidate is not evidence of a vtable.
            }
        }
        if (found == 0) out.printf("RTTI_CANDIDATE hit=0x%08X none%n", hit);
    }

    private void printWindow(long hit, int backWords, int forwardWords) {
        long start = hit - (long)backWords * 4;
        long end = hit + (long)forwardWords * 4;
        out.printf("WINDOW hit=0x%08X start=0x%08X end=0x%08X%n", hit, start, end);
        long runStart = -1;
        long runEnd = -1;
        for (long p = start; p <= end; p += 4) {
            try {
                long value = readU32(p);
                boolean function = validFunctionPointer(value);
                if (function && runStart < 0) runStart = p;
                if (!function && runStart >= 0) {
                    runEnd = p - 4;
                    out.printf("FUNCTION_RUN 0x%08X..0x%08X words=%d%n",
                               runStart, runEnd, (runEnd - runStart) / 4 + 1);
                    runStart = -1;
                }
                if (p == hit || (p >= hit - 8 && p <= hit + 8)) {
                    out.printf("  word=0x%08X value=0x%08X function=%s%n",
                               p, value, function ? "yes" : "no");
                }
            } catch (Exception ignored) {
                if (runStart >= 0) {
                    runEnd = p - 4;
                    out.printf("FUNCTION_RUN 0x%08X..0x%08X words=%d%n",
                               runStart, runEnd, (runEnd - runStart) / 4 + 1);
                    runStart = -1;
                }
            }
        }
        if (runStart >= 0) {
            runEnd = end;
            out.printf("FUNCTION_RUN 0x%08X..0x%08X words=%d%n",
                       runStart, runEnd, (runEnd - runStart) / 4 + 1);
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length > 4) {
            throw new IllegalArgumentException(
                "usage: ScanPointerRtti OUT TARGET [backWords=64] [forwardWords=64]");
        }
        out = new PrintWriter(args[0], "UTF-8");
        long target = Long.decode(args[1]) & 0xffffffffL;
        int backWords = args.length >= 3 ? Integer.parseUnsignedInt(args[2]) : 64;
        int forwardWords = args.length >= 4 ? Integer.parseUnsignedInt(args[3]) : 64;
        int hits = 0;
        out.printf("TARGET 0x%08X%n", target);
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || !block.isRead() || block.isExecute()) continue;
            Address p = block.getStart();
            Address last = block.getEnd().subtract(3);
            while (p.compareTo(last) <= 0) {
                long value;
                try {
                    value = readU32(p.getOffset());
                } catch (Exception failure) {
                    p = p.add(4);
                    continue;
                }
                if (value == target) {
                    long hit = p.getOffset();
                    hits++;
                    out.printf("HIT 0x%08X block=%s%n", hit, block.getName());
                    ReferenceIterator references =
                        currentProgram.getReferenceManager().getReferencesTo(p);
                    int refs = 0;
                    while (references.hasNext()) {
                        Reference reference = references.next();
                        Function function = getFunctionContaining(reference.getFromAddress());
                        out.printf("  XREF from=0x%08X type=%s in=%s%n",
                                   reference.getFromAddress().getOffset(),
                                   reference.getReferenceType(),
                                   function == null ? "none" : function.getName());
                        refs++;
                    }
                    out.printf("  xrefs=%d%n", refs);
                    printRttiCandidates(hit, backWords);
                    printWindow(hit, backWords, forwardWords);
                }
                p = p.add(4);
            }
        }
        out.printf("SUMMARY hits=%d target=0x%08X%n", hits, target);
        out.close();
        println("WROTE " + args[0] + " hits=" + hits);
    }
}
