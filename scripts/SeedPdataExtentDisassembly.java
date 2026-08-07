// Disassemble every word the linker records as belonging to a function, then
// recompute the owning body.
//
// After the flow repairs, 1016 functions still stop on undefined bytes inside
// their own .pdata extent, losing 736,380 bytes. Flow alone cannot reach those
// words: the usual predecessor is a `bctr` whose jump table Ghidra has not
// recovered, so nothing branches into the code that follows. .pdata states the
// words are part of the function regardless of whether flow reaches them.
//
// The extent is the authority for "these bytes are code". It is not used as the
// authority for the body: bodies are still recomputed from flow, so a word the
// linker covers but no path reaches stays outside the body and remains visible
// as a shortfall rather than being silently absorbed.
//
// Byte-qualified against .pdata before any change. Run WITHOUT -readOnly.
// Idempotent.
// @category AC6

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import java.security.MessageDigest;

public class SeedPdataExtentDisassembly extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    private static final long PDATA_START = 0x82079e00L;
    private static final int  PDATA_BYTES = 65968;
    private static final String PDATA_SHA256 =
        "740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034";

    private static final long CODE_MIN = 0x82090000L;
    private static final long CODE_MAX = 0x823e7ff7L;

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
        println("AC6_QUALIFIED_PDATA " + start + " sha256=" + actual);
        return bytes;
    }

    private static long beU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xff) << 24)
             | ((long) (bytes[offset + 1] & 0xff) << 16)
             | ((long) (bytes[offset + 2] & 0xff) << 8)
             | (long) (bytes[offset + 3] & 0xff);
    }

    @Override
    protected void run() throws Exception {
        String sha = currentProgram.getExecutableSHA256();
        if (!QUALIFIED_XEX_SHA256.equalsIgnoreCase(sha)) {
            throw new IllegalStateException("unexpected XEX SHA-256: " + sha);
        }
        byte[] pdata = readQualifiedPdata();

        Disassembler disassembler =
            Disassembler.getDisassembler(currentProgram, monitor, null);

        int extents = 0;
        int extentsSeeded = 0;
        long wordsSeeded = 0;
        long wordsStillUndefined = 0;
        int grown = 0;

        for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
            if (monitor.isCancelled()) {
                break;
            }
            long begin = beU32(pdata, offset);
            long packed = beU32(pdata, offset + 4);
            if (begin < CODE_MIN || begin > CODE_MAX) {
                continue;
            }
            long length = ((packed >>> 8) & 0x3fffffL) * 4;
            if (length <= 0) {
                continue;
            }
            extents++;
            long end = Math.min(begin + length, CODE_MAX + 1);

            AddressSet seeds = new AddressSet();
            for (long cursor = begin; cursor < end; cursor += 4) {
                Address address = toAddr(cursor);
                if (getInstructionAt(address) == null) {
                    seeds.add(address);
                }
            }
            if (seeds.isEmpty()) {
                continue;
            }
            extentsSeeded++;
            for (Address seed : seeds.getAddresses(true)) {
                if (getInstructionAt(seed) == null) {
                    disassembler.disassemble(seed, null);
                    wordsSeeded++;
                }
            }
            for (Address seed : seeds.getAddresses(true)) {
                if (getInstructionAt(seed) == null) {
                    wordsStillUndefined++;
                }
            }

            Function function = getFunctionAt(toAddr(begin));
            if (function == null) {
                continue;
            }
            long before = function.getBody().getNumAddresses();
            if (CreateFunctionCmd.fixupFunctionBody(currentProgram, function, monitor)
                    && function.getBody().getNumAddresses() > before) {
                grown++;
            }
        }

        println(String.format(
            "AC6_PDATA_SEED extents=%d seeded=%d words_disassembled=%d"
            + " words_still_undefined=%d bodies_grown=%d",
            extents, extentsSeeded, wordsSeeded, wordsStillUndefined, grown));
    }
}
