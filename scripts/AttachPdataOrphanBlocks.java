// Attach the instruction blocks that sit inside a linker-recorded function
// extent but outside the function's body.
//
// After S0, every .pdata entry has a function and most bodies are complete. The
// remaining defect is different from truncation: the body's address set has
// holes. Ghidra disassembled the bytes - they are instructions - but never
// attached them to the function, typically because the only flow into them is a
// computed branch it did not resolve. An address in such a hole answers "no
// function contains this", which is why call sites inside perfectly well-formed
// functions look orphaned.
//
// The rule is the same one S0 used, and it comes from the linker rather than
// from inference: if an instruction lies inside a function's recorded extent
// and no other function owns it, it belongs to that function.
//
// Policy:
//   - only addresses already disassembled are attached; nothing is disassembled
//     here, and no byte is decoded that was not decoded before;
//   - an address owned by another function is never taken;
//   - a recorded extent that overlaps the next entry is skipped entirely;
//   - the entry point and the existing body are always preserved.
//
// Byte-qualified against the .pdata bytes before any change. Idempotent.
//
// usage: AttachPdataOrphanBlocks.java audit          (read-only, safe)
//        AttachPdataOrphanBlocks.java apply          (writes; run WITHOUT -readOnly)
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;

public class AttachPdataOrphanBlocks extends GhidraScript {

    private static final long PDATA_START = 0x82079e00L;
    private static final int PDATA_BYTES = 65968;
    private static final String PDATA_SHA256 =
            "740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034";

    private record Entry(long begin, long length) {}

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        boolean apply = args.length > 0 && args[0].equalsIgnoreCase("apply");

        byte[] raw = new byte[PDATA_BYTES];
        currentProgram.getMemory().getBytes(toAddr(PDATA_START), raw);
        String digest = sha256(raw);
        println("AC6_PDATA_SHA256 " + digest);
        if (!digest.equals(PDATA_SHA256)) {
            printerr("pdata bytes are not the qualified ones; refusing");
            return;
        }

        List<Entry> table = new ArrayList<>();
        for (int offset = 0; offset + 8 <= raw.length; offset += 8) {
            long begin = readU32(raw, offset);
            long packed = readU32(raw, offset + 4);
            if (begin == 0 && packed == 0) {
                continue;
            }
            table.add(new Entry(begin, ((packed >>> 8) & 0x3FFFFF) * 4));
        }
        println("AC6_PDATA_ENTRIES " + table.size());

        int touched = 0;
        int skippedOverlap = 0;
        long attachedBytes = 0;
        long ownedElsewhere = 0;
        for (int index = 0; index < table.size() && !monitor.isCancelled(); index++) {
            Entry entry = table.get(index);
            long end = entry.begin() + entry.length();
            if (index + 1 < table.size() && table.get(index + 1).begin() < end) {
                skippedOverlap++;
                continue;
            }
            Address begin = toAddr(entry.begin());
            Function function = currentProgram.getFunctionManager().getFunctionAt(begin);
            if (function == null || entry.length() == 0) {
                continue;
            }

            AddressSet orphans = new AddressSet();
            Address cursor = begin;
            while (cursor.getOffset() < end) {
                Instruction instruction =
                        currentProgram.getListing().getInstructionAt(cursor);
                if (instruction == null) {
                    cursor = cursor.add(4);
                    continue;
                }
                Address max = instruction.getMaxAddress();
                if (!function.getBody().contains(cursor)) {
                    Function owner = currentProgram.getFunctionManager()
                            .getFunctionContaining(cursor);
                    if (owner == null) {
                        orphans.addRange(cursor, max);
                    } else if (!owner.equals(function)) {
                        ownedElsewhere += max.subtract(cursor) + 1;
                    }
                }
                cursor = max.add(1);
            }
            if (orphans.isEmpty()) {
                continue;
            }
            touched++;
            attachedBytes += orphans.getNumAddresses();
            println(String.format("AC6_ORPHANS function=%s recorded=0x%X body=%d orphan=%d",
                    function.getEntryPoint(), entry.length(),
                    function.getBody().getNumAddresses(), orphans.getNumAddresses()));
            if (apply) {
                AddressSet body = new AddressSet(function.getBody());
                body.add(orphans);
                function.setBody(body);
            }
        }
        println(String.format(
                "AC6_ATTACH mode=%s functions=%d attached_bytes=%d owned_elsewhere=%d overlapping_entries_skipped=%d",
                apply ? "apply" : "audit", touched, attachedBytes, ownedElsewhere,
                skippedOverlap));
    }

    private long readU32(byte[] bytes, int offset) {
        return ((long) (bytes[offset] & 0xFF) << 24)
                | ((long) (bytes[offset + 1] & 0xFF) << 16)
                | ((long) (bytes[offset + 2] & 0xFF) << 8)
                | (long) (bytes[offset + 3] & 0xFF);
    }

    private String sha256(byte[] bytes) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        StringBuilder text = new StringBuilder();
        for (byte value : digest.digest(bytes)) {
            text.append(String.format("%02x", value));
        }
        return text.toString();
    }
}
