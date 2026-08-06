// Finds PPC lis/addi or lis/ori address materializations separated by a
// bounded number of instructions. The simple adjacent-pair scanner misses
// compiler schedules that prepare an index between the high and low halves.
// Read-only diagnostic; every hit must still be inspected in its function.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class FindPpcSplitAddressMaterialization extends GhidraScript {
    private static long u32(byte[] bytes) {
        return ((long)(bytes[0] & 0xff) << 24) |
               ((long)(bytes[1] & 0xff) << 16) |
               ((long)(bytes[2] & 0xff) << 8) |
               (long)(bytes[3] & 0xff);
    }

    private static int signed16(long value) {
        int result = (int)(value & 0xffffL);
        return result >= 0x8000 ? result - 0x10000 : result;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1 || args.length > 2) {
            throw new IllegalArgumentException(
                "usage: FindPpcSplitAddressMaterialization <address> [window=24]");
        }
        long target = Long.decode(args[0]) & 0xffffffffL;
        int window = args.length == 2 ? Integer.parseUnsignedInt(args[1]) : 24;
        if (window < 1 || window > 128) {
            throw new IllegalArgumentException("window must be in 1..128");
        }

        long hits = 0;
        for (Instruction first : currentProgram.getListing().getInstructions(true)) {
            byte[] firstBytes = first.getBytes();
            if (firstBytes.length != 4) continue;
            long firstWord = u32(firstBytes);
            if ((firstWord >>> 26) != 15 || ((firstWord >>> 16) & 31) != 0) {
                continue; // lis is addis rD,r0,SI
            }

            int highRegister = (int)((firstWord >>> 21) & 31);
            long high = (((long)signed16(firstWord)) << 16) & 0xffffffffL;
            Address cursor = first.getMaxAddress().add(1);
            for (int distance = 1; distance <= window; ++distance) {
                Instruction second = currentProgram.getListing().getInstructionAt(cursor);
                if (second == null || second.getBytes().length != 4) break;
                long secondWord = u32(second.getBytes());
                int opcode = (int)(secondWord >>> 26);
                boolean addi = opcode == 14 &&
                    ((secondWord >>> 16) & 31) == highRegister;
                boolean ori = opcode == 24 &&
                    ((secondWord >>> 21) & 31) == highRegister;
                long value = addi
                    ? (high + signed16(secondWord)) & 0xffffffffL
                    : ori ? (high | (secondWord & 0xffffL)) & 0xffffffffL : 0;
                if ((addi || ori) && value == target) {
                    hits++;
                    println(first.getAddress() + " " + first + " ; +" +
                        distance + " " + second.getAddress() + " " + second +
                        " => 0x" + Long.toHexString(value));
                }
                cursor = second.getMaxAddress().add(1);
            }
        }
        println("SUMMARY target=0x" + Long.toHexString(target) +
            " window=" + window + " hits=" + hits);
    }
}
