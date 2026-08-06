// Find direct PPC D-form memory accesses using selected signed displacements.
// This is a read-only candidate finder; it does not infer object identity or
// register provenance.
// Usage: FindPpcDisplacementUses.java <offset> [offset ...]
// @category AC6.Evidence

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class FindPpcDisplacementUses extends GhidraScript {
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

    private static String mnemonic(int opcode) {
        switch (opcode) {
            case 32: return "lwz";
            case 34: return "lbz";
            case 40: return "lhz";
            case 42: return "lha";
            case 46: return "lmw";
            case 48: return "lfs";
            case 50: return "lfd";
            case 36: return "stw";
            case 38: return "stb";
            case 44: return "sth";
            case 47: return "stmw";
            case 52: return "stfs";
            case 54: return "stfd";
            default: return null;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindPpcDisplacementUses <offset> [offset ...]");
        }
        Set<Integer> targets = new HashSet<>();
        for (String arg : args) {
            long decoded = Long.decode(arg);
            if (decoded < Short.MIN_VALUE || decoded > 0xffffL) {
                throw new IllegalArgumentException("offset out of 16-bit range: " + arg);
            }
            targets.add(signed16(decoded));
        }

        long hits = 0;
        for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
            byte[] bytes = instruction.getBytes();
            if (bytes.length != 4) continue;
            long word = u32(bytes);
            String operation = mnemonic((int)(word >>> 26));
            int displacement = signed16(word);
            if (operation == null || !targets.contains(displacement)) continue;

            int valueRegister = (int)((word >>> 21) & 31);
            int baseRegister = (int)((word >>> 16) & 31);
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(instruction.getAddress());
            println(String.format(
                "address=%s operation=%s value=r%d displacement=%d base=r%d owner=%s",
                instruction.getAddress(), operation, valueRegister, displacement,
                baseRegister, owner == null ? "<no-function>" :
                    owner.getEntryPoint() + " " + owner.getName()));
            hits++;
        }
        println("SUMMARY hits=" + hits + " targets=" + targets);
    }
}
