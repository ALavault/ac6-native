// Create the function starts the linker itself recorded.
//
// The PE .pdata section is the Microsoft runtime-function table: 8246 entries of
// (BeginAddress, PackedData) where PackedData carries the prologue length in
// bits 0-7 and the function length in words in bits 8-29. It is monotonic, has
// zero overlaps, and covers 3,137,892 of the 3,506,168 bytes of the qualified
// code range (89.5 percent). It is the linker's own boundary evidence, read out
// of the program, not an inferred or generated list.
//
// Policy. This pass is strictly additive:
//   - a function that already exists at a .pdata start is left untouched,
//     including its body; Ghidra's own boundary keeps priority;
//   - a start with no function gets its range disassembled and a function
//     created, with the body derived from flow rather than forced from the
//     table;
//   - nothing is deleted, renamed or resized.
//
// Byte-qualified against the loaded image before any change. Run WITHOUT
// -readOnly. Idempotent.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.security.MessageDigest;

public class CreatePdataFunctionStarts extends GhidraScript {

    private static final String QUALIFIED_XEX_SHA256 =
        "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

    // PE section .pdata: VA 0x82079E00, 65968 bytes, 8246 eight-byte entries.
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
            throw new IllegalStateException(".pdata SHA-256 mismatch: expected "
                + PDATA_SHA256 + ", got " + actual);
        }
        println("AC6_QUALIFIED_PDATA " + start + " bytes=" + PDATA_BYTES
            + " sha256=" + actual);
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

        int entries = 0;
        int outOfRange = 0;
        int alreadyPresent = 0;
        int disassembled = 0;
        int created = 0;
        int failed = 0;

        for (int offset = 0; offset + 8 <= pdata.length; offset += 8) {
            if (monitor.isCancelled()) {
                break;
            }
            long begin = beU32(pdata, offset);
            long packed = beU32(pdata, offset + 4);
            if (begin == 0) {
                continue;
            }
            entries++;
            long words = (packed >>> 8) & 0x3fffffL;
            long length = words * 4;
            if (begin < CODE_MIN || begin > CODE_MAX || length <= 0) {
                outOfRange++;
                continue;
            }

            Address entry = toAddr(begin);
            if (getFunctionAt(entry) != null) {
                alreadyPresent++;
                continue;
            }

            // Seed every still-undefined word of the recorded range. Creating a
            // function over undefined bytes yields a plausible name and an empty
            // body, which is exactly the state this pass exists to remove.
            long last = Math.min(begin + length, CODE_MAX + 1);
            boolean seeded = false;
            for (long cursor = begin; cursor < last; cursor += 4) {
                Address address = toAddr(cursor);
                if (getInstructionAt(address) == null) {
                    disassembler.disassemble(address, null);
                    seeded = true;
                }
            }
            if (seeded) {
                disassembled++;
            }

            if (getInstructionAt(entry) == null) {
                failed++;
                println("AC6_PDATA_UNDISASSEMBLABLE " + entry);
                continue;
            }
            // Body from flow, not forced from the table: the table is the start
            // evidence, Ghidra's control flow stays the boundary authority.
            Function function = createFunction(entry, null);
            if (function == null) {
                failed++;
                println("AC6_PDATA_CREATE_FAILED " + entry
                    + " pdata_length=" + length);
                continue;
            }
            created++;
        }

        println(String.format(
            "AC6_PDATA_SUMMARY entries=%d out_of_range=%d already_present=%d"
            + " disassembly_seeded=%d created=%d failed=%d",
            entries, outOfRange, alreadyPresent, disassembled, created, failed));
    }
}
