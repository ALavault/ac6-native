// Scan raw executable bytes for bounded PPC lis/addi|ori materializations.
// Unlike listing-based scripts, this covers valid code not assigned to a
// Ghidra function or instruction yet.
// Usage: FindRawPpcAddressMaterialization.java <address> [window=12]
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;

public class FindRawPpcAddressMaterialization extends GhidraScript {
    private static int signed16(int word) {
        return (short)word;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1 || args.length > 2) {
            throw new IllegalArgumentException(
                "usage: FindRawPpcAddressMaterialization <address> [window=12]");
        }
        long target = Long.decode(args[0]) & 0xffffffffL;
        int window = args.length == 2 ? Integer.parseUnsignedInt(args[1]) : 12;
        if (window < 1 || window > 64) {
            throw new IllegalArgumentException("window must be in 1..64");
        }

        Memory memory = currentProgram.getMemory();
        long hits = 0;
        for (long value = 0x82090000L; value < 0x823e7ff8L; value += 4) {
            Address highAddress = toAddr(value);
            int highWord;
            try {
                highWord = memory.getInt(highAddress);
            } catch (MemoryAccessException ignored) {
                continue;
            }
            if ((highWord >>> 26) != 15 || ((highWord >>> 16) & 31) != 0) {
                continue;
            }
            int highRegister = (highWord >>> 21) & 31;
            long high = ((long)signed16(highWord) << 16) & 0xffffffffL;
            for (int distance = 1; distance <= window; ++distance) {
                Address lowAddress = highAddress.add((long)distance * 4L);
                int lowWord;
                try {
                    lowWord = memory.getInt(lowAddress);
                } catch (MemoryAccessException ignored) {
                    break;
                }
                int opcode = lowWord >>> 26;
                boolean addi = opcode == 14 && ((lowWord >>> 16) & 31) == highRegister;
                boolean ori = opcode == 24 && ((lowWord >>> 21) & 31) == highRegister;
                if (!addi && !ori) continue;
                long materialized = addi
                    ? (high + signed16(lowWord)) & 0xffffffffL
                    : (high | (lowWord & 0xffffL)) & 0xffffffffL;
                if (materialized != target) continue;
                int resultRegister = addi
                    ? (lowWord >>> 21) & 31
                    : (lowWord >>> 16) & 31;
                println(String.format(
                    "high=%s low=%s distance=%d high=r%d result=r%d value=0x%08X",
                    highAddress, lowAddress, distance, highRegister,
                    resultRegister, materialized));
                hits++;
            }
        }
        println(String.format("SUMMARY target=0x%08X hits=%d", target, hits));
    }
}
