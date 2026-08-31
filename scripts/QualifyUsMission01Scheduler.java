// Read-only byte qualification for the US Mission 01 script scheduler.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.security.MessageDigest;

public class QualifyUsMission01Scheduler extends GhidraScript {
    private static final String PROJECT = "ac6-us";
    private static final String PROGRAM = "default.xex";
    private static final String LANGUAGE = "PowerPC:BE:64:Xenon";
    private static final String XEX_SHA256 =
        "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";
    private static final long PDATA_START = 0x82079e00L;
    private static final int PDATA_BYTES = 0xff18;
    private static final long[] HOOKS = {
        0x82267160L, // step advance
        0x82267258L, // counter write helper
        0x822ed310L, // signal/object handler
    };

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    private byte[] read(long start, int length) throws Exception {
        byte[] bytes = new byte[length];
        currentProgram.getMemory().getBytes(toAddr(start), bytes);
        return bytes;
    }

    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format("%02x", value & 0xff));
        }
        return result.toString();
    }

    private static String sha256(byte[] bytes) throws Exception {
        return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private void qualifyIdentity() {
        if (!PROJECT.equals(state.getProject().getName())) {
            throw new AssertionError("wrong project: " + state.getProject().getName());
        }
        if (!PROGRAM.equals(currentProgram.getName())) {
            throw new AssertionError("wrong program: " + currentProgram.getName());
        }
        if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new AssertionError("wrong XEX: " + currentProgram.getExecutableSHA256());
        }
        String language = currentProgram.getLanguageID().toString();
        if (!LANGUAGE.equals(language) && !(LANGUAGE + ":default").equals(language)) {
            throw new AssertionError("wrong language: " + language);
        }
    }

    private long extent(byte[] pdata, long entry) {
        for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
            if (beU32(pdata, offset) == entry) {
                return ((beU32(pdata, offset + 4) >>> 8) & 0x3fffffL) * 4;
            }
        }
        return 0;
    }

    @Override
    protected void run() throws Exception {
        qualifyIdentity();
        byte[] pdata = read(PDATA_START, PDATA_BYTES);
        println("AC6_US_MISSION01_SCHEDULER_IDENTITY project=" + PROJECT
            + " program=" + PROGRAM + " language=" + LANGUAGE
            + " xex_sha256=" + XEX_SHA256);
        for (long hook : HOOKS) {
            long length = extent(pdata, hook);
            if (length <= 0 || length > 0x10000L) {
                throw new AssertionError(String.format(
                    "missing/invalid .pdata extent for 0x%08X", hook));
            }
            Function function = getFunctionAt(toAddr(hook));
            if (function == null) {
                throw new AssertionError(String.format(
                    "no function at 0x%08X", hook));
            }
            byte[] extentBytes = read(hook, (int) length);
            println(String.format(
                "AC6_US_MISSION01_SCHEDULER_HOOK address=0x%08X "
                + "end_exclusive=0x%08X length=%d extent_sha256=%s "
                + "entry_bytes=%s ghidra_body=%s",
                hook, hook + length, length, sha256(extentBytes),
                hex(read(hook, 32)), function.getBody()));
        }
        println("AC6_US_MISSION01_SCHEDULER_PASS hooks=" + HOOKS.length);
    }
}
