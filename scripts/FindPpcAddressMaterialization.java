// Finds simple PPC lis/addi or lis/ori address materializations.
// Read-only diagnostic; it does not create or modify Ghidra symbols.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class FindPpcAddressMaterialization extends GhidraScript {
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

    private static long materialize(long high, long low, boolean ori) {
        return ori
            ? ((high | (low & 0xffffL)) & 0xffffffffL)
            : ((high + signed16(low)) & 0xffffffffL);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindPpcAddressMaterialization <address>...");
        }
        long[] targets = new long[args.length];
        for (int i = 0; i < args.length; ++i) {
            targets[i] = Long.decode(args[i]) & 0xffffffffL;
        }

        for (Instruction first : currentProgram.getListing()
                .getInstructions(true)) {
            byte[] firstBytes = first.getBytes();
            if (firstBytes.length != 4) continue;
            long firstWord = u32(firstBytes);
            if ((firstWord >>> 26) != 15) continue; // lis/addis

            int destination = (int)((firstWord >>> 21) & 31);
            long high = (((long)signed16(firstWord)) << 16) & 0xffffffffL;
            Address nextAddress = first.getMaxAddress().add(1);
            Instruction second = currentProgram.getListing()
                .getInstructionAt(nextAddress);
            if (second == null) continue;
            byte[] secondBytes = second.getBytes();
            if (secondBytes.length != 4) continue;
            long secondWord = u32(secondBytes);
            int opcode = (int)(secondWord >>> 26);
            int secondDestination = (int)((secondWord >>> 21) & 31);
            int secondBase = (int)((secondWord >>> 16) & 31);
            if (secondBase != destination || secondDestination != destination) {
                continue;
            }

            boolean ori = opcode == 24; // ori rt,ra,UI
            boolean addi = opcode == 14; // addi rt,ra,SI
            if (!ori && !addi) continue;

            long target = materialize(high, secondWord & 0xffffL, ori);
            for (long wanted : targets) {
                if (target == wanted) {
                    println(first.getAddress() + " " + first + " ; " +
                        second.getAddress() + " " + second +
                        " => 0x" + Long.toHexString(target));
                    break;
                }
            }
        }
    }
}
