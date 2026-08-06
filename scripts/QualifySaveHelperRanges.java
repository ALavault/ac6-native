// Print byte-qualified ranges for the save/file-selector helpers.
// This is read-only evidence collection; it never creates or edits functions.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.security.MessageDigest;

public class QualifySaveHelperRanges extends GhidraScript {
    private static final String XEX_SHA256 =
            "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private String digest(Address start, Address end) throws Exception {
        long length = end.subtract(start) + 4;
        if (length <= 0 || length > Integer.MAX_VALUE) {
            throw new IllegalArgumentException("invalid range " + start + ".." + end);
        }
        byte[] bytes = new byte[(int) length];
        for (int i = 0; i < bytes.length; ++i) {
            bytes[i] = currentProgram.getMemory().getByte(start.add(i));
        }
        byte[] hash = MessageDigest.getInstance("SHA-256").digest(bytes);
        StringBuilder out = new StringBuilder(64);
        for (byte value : hash) {
            out.append(String.format("%02x", value & 0xff));
        }
        return out.toString();
    }

    private void report(String startText, String endText, String label) throws Exception {
        Address start = toAddr(startText);
        Address end = toAddr(endText);
        Function function = getFunctionAt(start);
        println(label + " " + start + ".." + end +
                " length=" + (end.subtract(start) + 4) +
                " sha256=" + digest(start, end));
        if (function == null) {
            println("  function=<none>");
        } else {
            println("  function=" + function.getEntryPoint() + " " + function.getName() +
                    " body=" + function.getBody());
        }
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        println("program=" + currentProgram.getName() + " xex_sha256=" + sha);
        report("0x821C3BE8", "0x821C402C", "FileSelector_821C3BE8");
        report("0x821C4FA0", "0x821C5254", "FileCreateDialog_821C4FA0");
        report("0x821C5258", "0x821C56F4", "FileCreateTask_821C5258");
        report("0x821C56F8", "0x821C59AC", "FileCreateState_821C56F8");
    }
}
