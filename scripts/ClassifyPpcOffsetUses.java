// Classify bounded PPC indexed-memory uses after a split lis/addi|ori offset
// materialization. This is a read-only candidate finder; register liveness
// beyond the bounded first use still requires inspection.
// Usage: ClassifyPpcOffsetUses.java <offset> [pair-window=64] [use-window=12]
// @category AC6.Evidence

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class ClassifyPpcOffsetUses extends GhidraScript {
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

    private static String indexedMnemonic(int xo) {
        switch (xo) {
            case 23: return "lwzx";
            case 87: return "lbzx";
            case 151: return "stwx";
            case 215: return "stbx";
            case 279: return "lhzx";
            case 343: return "lhax";
            case 407: return "sthx";
            default: return null;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1 || args.length > 3) {
            throw new IllegalArgumentException(
                "usage: ClassifyPpcOffsetUses <offset> [pair-window=64] [use-window=12]");
        }
        long target = Long.decode(args[0]) & 0xffffffffL;
        int pairWindow = args.length >= 2 ? Integer.parseUnsignedInt(args[1]) : 64;
        int useWindow = args.length == 3 ? Integer.parseUnsignedInt(args[2]) : 12;
        if (pairWindow < 1 || pairWindow > 128 || useWindow < 1 || useWindow > 64) {
            throw new IllegalArgumentException("windows out of range");
        }

        Set<String> reported = new HashSet<>();
        long hits = 0;
        for (Instruction highInstruction : currentProgram.getListing().getInstructions(true)) {
            byte[] highBytes = highInstruction.getBytes();
            if (highBytes.length != 4) continue;
            long highWord = u32(highBytes);
            if ((highWord >>> 26) != 15 || ((highWord >>> 16) & 31) != 0) continue;

            int highRegister = (int)((highWord >>> 21) & 31);
            long high = (((long)signed16(highWord)) << 16) & 0xffffffffL;
            Address lowCursor = highInstruction.getMaxAddress().add(1);
            for (int pairDistance = 1; pairDistance <= pairWindow; ++pairDistance) {
                Instruction lowInstruction = currentProgram.getListing().getInstructionAt(lowCursor);
                if (lowInstruction == null || lowInstruction.getBytes().length != 4) break;
                long lowWord = u32(lowInstruction.getBytes());
                int opcode = (int)(lowWord >>> 26);
                boolean addi = opcode == 14 && ((lowWord >>> 16) & 31) == highRegister;
                boolean ori = opcode == 24 && ((lowWord >>> 21) & 31) == highRegister;
                long value = addi
                    ? (high + signed16(lowWord)) & 0xffffffffL
                    : ori ? (high | (lowWord & 0xffffL)) & 0xffffffffL : 0;
                if ((addi || ori) && value == target) {
                    int offsetRegister = addi
                        ? (int)((lowWord >>> 21) & 31)
                        : (int)((lowWord >>> 16) & 31);
                    Address useCursor = lowInstruction.getMaxAddress().add(1);
                    for (int useDistance = 1; useDistance <= useWindow; ++useDistance) {
                        Instruction useInstruction = currentProgram.getListing().getInstructionAt(useCursor);
                        if (useInstruction == null || useInstruction.getBytes().length != 4) break;
                        long useWord = u32(useInstruction.getBytes());
                        if ((useWord >>> 26) == 31) {
                            String mnemonic = indexedMnemonic((int)((useWord >>> 1) & 0x3ff));
                            int baseRegister = (int)((useWord >>> 16) & 31);
                            int indexRegister = (int)((useWord >>> 11) & 31);
                            if (mnemonic != null &&
                                (baseRegister == offsetRegister || indexRegister == offsetRegister)) {
                                String key = lowInstruction.getAddress() + ":" + useInstruction.getAddress();
                                if (reported.add(key)) {
                                    Function owner = currentProgram.getFunctionManager()
                                        .getFunctionContaining(useInstruction.getAddress());
                                    println(String.format(
                                        "low=%s pair_distance=%d use=%s use_distance=%d %s " +
                                        "offset=r%d base=r%d index=r%d owner=%s",
                                        lowInstruction.getAddress(), pairDistance,
                                        useInstruction.getAddress(), useDistance, mnemonic,
                                        offsetRegister, baseRegister, indexRegister,
                                        owner == null ? "<no-function>" :
                                            owner.getEntryPoint() + " " + owner.getName()));
                                    hits++;
                                }
                                break;
                            }
                        }
                        useCursor = useInstruction.getMaxAddress().add(1);
                    }
                }
                lowCursor = lowInstruction.getMaxAddress().add(1);
            }
        }
        println("SUMMARY target=0x" + Long.toHexString(target) + " hits=" + hits);
    }
}
