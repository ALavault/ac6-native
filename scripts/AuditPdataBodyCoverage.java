// Compare every linker-recorded function extent in .pdata against the body
// Ghidra actually holds, and classify what stops the flow at each shortfall.
//
// .pdata is the ground truth for extent: 8246 monotonic, non-overlapping
// entries covering 89.5 percent of the code range. Any function whose body is
// materially shorter than its recorded length is still truncated, and the
// instruction at the truncation point names the defect family responsible.
// Read-only. Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class AuditPdataBodyCoverage extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long PDATA_START = 0x82079e00L;
    private static final int  PDATA_BYTES = 65968;
    private static final String PDATA_SHA256 =
        "740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034";

    private static final long CODE_MIN = 0x82090000L;
    private static final long CODE_MAX = 0x823e7ff7L;

    /** A shortfall of one word is normal padding; anything more is truncation. */
    private static final long TOLERANCE = 4;

    private static class Shortfall {
        Address entry;
        long recorded;
        long actual;
        String stopMnemonic = "<undefined>";
        Address stopAt;
    }

    private byte[] readQualifiedPdata() throws Exception {
        Address start = toAddr(PDATA_START);
        byte[] bytes = new byte[PDATA_BYTES];
        for (int index = 0; index < bytes.length; ++index) {
            bytes[index] = currentProgram.getMemory().getByte(start.add(index));
        }
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder actual = new StringBuilder(64);
        for (byte value : digest) {
            actual.append(String.format("%02x", value & 0xff));
        }
        if (!PDATA_SHA256.equalsIgnoreCase(actual.toString())) {
            throw new IllegalStateException(".pdata SHA-256 mismatch: got " + actual);
        }
        return bytes;
    }

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    /**
     * Walks forward from the entry while the body still contains each word, and
     * returns the first address the body does not cover. That instruction, or
     * the one before it, is what stopped the flow.
     */
    private void classifyStop(Function function, long begin, long recorded,
            Shortfall shortfall) {
        for (long cursor = begin; cursor < begin + recorded; cursor += 4) {
            Address address = toAddr(cursor);
            if (function.getBody().contains(address)) {
                continue;
            }
            shortfall.stopAt = address;
            // A body that stops on a plain arithmetic instruction is not a flow
            // problem: something else already owns the next address. The usual
            // owner is a spurious function start left over from the truncation
            // era, which now blocks the real body from growing.
            Function blocker = getFunctionAt(address);
            if (blocker != null) {
                shortfall.stopMnemonic = "BLOCKED_BY_FUNCTION_START";
                return;
            }
            if (getInstructionAt(address) == null) {
                shortfall.stopMnemonic = "UNDEFINED_BYTES";
                return;
            }
            Address previous = toAddr(cursor - 4);
            Instruction last = getInstructionAt(previous);
            if (last != null) {
                String mnemonic = last.getMnemonicString();
                // Name the call target where there is one: the defect families
                // are usually one specific callee, as __savegprlr was.
                Address target = null;
                if (last.getFlowType().isCall() && last.getFlows().length > 0) {
                    target = last.getFlows()[0];
                }
                shortfall.stopMnemonic = target == null
                    ? mnemonic
                    : mnemonic + " -> " + target;
            }
            return;
        }
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        byte[] pdata = readQualifiedPdata();

        long recordedTotal = 0;
        long actualTotal = 0;
        int missing = 0;
        int exact = 0;
        List<Shortfall> shortfalls = new ArrayList<>();

        for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
            long begin = beU32(pdata, offset);
            long packed = beU32(pdata, offset + 4);
            if (begin < CODE_MIN || begin > CODE_MAX) {
                continue;
            }
            long recorded = ((packed >>> 8) & 0x3fffffL) * 4;
            if (recorded <= 0) {
                continue;
            }
            recordedTotal += recorded;

            Address entry = toAddr(begin);
            Function function = getFunctionAt(entry);
            if (function == null) {
                missing++;
                continue;
            }
            long actual = function.getBody().getNumAddresses();
            actualTotal += Math.min(actual, recorded);
            if (recorded - actual <= TOLERANCE) {
                exact++;
                continue;
            }
            Shortfall shortfall = new Shortfall();
            shortfall.entry = entry;
            shortfall.recorded = recorded;
            shortfall.actual = actual;
            classifyStop(function, begin, recorded, shortfall);
            shortfalls.add(shortfall);
        }

        println(String.format(
            "AC6_PDATA_BODY recorded_bytes=%d covered_bytes=%d percent=%.1f"
            + " no_function=%d complete=%d short=%d",
            recordedTotal, actualTotal, 100.0 * actualTotal / recordedTotal,
            missing, exact, shortfalls.size()));

        // Which defect family accounts for the remaining loss?
        Map<String, long[]> byFamily = new HashMap<>();
        for (Shortfall shortfall : shortfalls) {
            long[] tally = byFamily.computeIfAbsent(shortfall.stopMnemonic,
                key -> new long[2]);
            tally[0]++;
            tally[1] += shortfall.recorded - shortfall.actual;
        }
        List<Map.Entry<String, long[]>> families =
            new ArrayList<>(byFamily.entrySet());
        families.sort(Comparator.comparingLong(
            (Map.Entry<String, long[]> family) -> family.getValue()[1]).reversed());
        for (int index = 0; index < Math.min(20, families.size()); ++index) {
            Map.Entry<String, long[]> family = families.get(index);
            println(String.format("AC6_PDATA_STOP %-40s functions=%d lost_bytes=%d",
                family.getKey(), family.getValue()[0], family.getValue()[1]));
        }

        shortfalls.sort(Comparator.comparingLong(
            (Shortfall shortfall) -> shortfall.recorded - shortfall.actual).reversed());
        for (int index = 0; index < Math.min(15, shortfalls.size()); ++index) {
            Shortfall shortfall = shortfalls.get(index);
            println(String.format(
                "AC6_PDATA_WORST %s recorded=%d actual=%d stop_at=%s after=%s",
                shortfall.entry, shortfall.recorded, shortfall.actual,
                shortfall.stopAt, shortfall.stopMnemonic));
        }
    }
}
