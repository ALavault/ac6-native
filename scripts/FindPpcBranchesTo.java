// Finds PPC relative/absolute branch encodings to exact targets without
// relying on Ghidra's function or reference database. Read-only.
// @category AC6

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class FindPpcBranchesTo extends GhidraScript {
    private static long u32(byte[] bytes) {
        return ((long)(bytes[0] & 0xff) << 24) |
               ((long)(bytes[1] & 0xff) << 16) |
               ((long)(bytes[2] & 0xff) << 8) |
               (long)(bytes[3] & 0xff);
    }

    private static long signExtend(long value, int bits) {
        long sign = 1L << (bits - 1);
        return (value ^ sign) - sign;
    }

    @Override
    protected void run() throws Exception {
        if (getScriptArgs().length == 0) {
            throw new IllegalArgumentException("usage: FindPpcBranchesTo <address>...");
        }
        Set<Long> targets = new HashSet<>();
        for (String argument : getScriptArgs()) targets.add(Long.decode(argument) & 0xffffffffL);

        for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
            byte[] bytes = instruction.getBytes();
            if (bytes.length != 4) continue;
            long word = u32(bytes);
            long opcode = word >>> 26;
            long destination;
            if (opcode == 18) { // b/bl: LI << 2, AA at bit 1
                long displacement = signExtend((word >>> 2) & 0x00ffffffL, 24) << 2;
                destination = ((word & 0x2L) != 0) ? displacement :
                    instruction.getAddress().getOffset() + displacement;
            } else if (opcode == 16) { // bc/bcl: BD << 2, AA at bit 1
                long displacement = signExtend((word >>> 2) & 0x3fffL, 14) << 2;
                destination = ((word & 0x2L) != 0) ? displacement :
                    instruction.getAddress().getOffset() + displacement;
            } else {
                continue;
            }
            destination &= 0xffffffffL;
            if (targets.contains(destination)) {
                println(instruction.getAddress() + " -> 0x" +
                    Long.toHexString(destination) + " " + instruction);
            }
        }
    }
}
