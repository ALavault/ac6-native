// Export byte-qualified NTSC-U/J route-synchronization hook boundaries.
//
// The linker .pdata extent is authoritative here: Ghidra's loader initially
// marks Xenon save-register helper subentries no-return, which can truncate a
// displayed function body. This script is read-only and does not repair or
// otherwise mutate the canonical US project.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.security.MessageDigest;

public class QualifyUsRouteSync extends GhidraScript {
    private static final String PROJECT = "ac6-us";
    private static final String PROGRAM = "default.xex";
    private static final String LANGUAGE = "PowerPC:BE:64:Xenon";
    private static final String XEX_SHA256 =
        "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";
    private static final long PDATA_START = 0x82079e00L;
    private static final int PDATA_BYTES = 0xff18;
    private static final long[] HOOKS = {
        0x821c3800L, 0x821c5268L, 0x821c5708L, 0x8218f4f0L,
    };

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    private byte[] read(long start, int length) throws Exception {
        byte[] bytes = new byte[length];
        Address address = toAddr(start);
        for (int index = 0; index < length; ++index) {
            bytes[index] = currentProgram.getMemory().getByte(address.add(index));
        }
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

    @Override
    protected void run() throws Exception {
        qualifyIdentity();
        byte[] pdata = read(PDATA_START, PDATA_BYTES);
        println("AC6_US_ROUTE_SYNC_IDENTITY project=" + PROJECT + " program=" + PROGRAM
            + " language=" + LANGUAGE + " xex_sha256=" + XEX_SHA256);
        println(String.format(
            "AC6_US_PDATA start=0x%08X length=%d sha256=%s",
            PDATA_START, PDATA_BYTES, sha256(pdata)));

        for (long hook : HOOKS) {
            long length = 0;
            for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
                if (beU32(pdata, offset) == hook) {
                    long packed = beU32(pdata, offset + 4);
                    length = ((packed >>> 8) & 0x3fffffL) * 4;
                    break;
                }
            }
            if (length <= 0 || length > Integer.MAX_VALUE) {
                throw new AssertionError(String.format(
                    "missing/invalid .pdata extent for 0x%08X", hook));
            }
            Function function = getFunctionAt(toAddr(hook));
            if (function == null) {
                throw new AssertionError(String.format(
                    "no function at 0x%08X", hook));
            }
            byte[] extent = read(hook, (int) length);
            byte[] entry = read(hook, 32);
            println(String.format(
                "AC6_US_ROUTE_SYNC_HOOK address=0x%08X end_exclusive=0x%08X "
                + "length=%d extent_sha256=%s entry_bytes=%s ghidra_body=%s",
                hook, hook + length, length, sha256(extent), hex(entry),
                function.getBody()));
        }
        println("AC6_US_ROUTE_SYNC_PASS hooks=4");
    }
}
