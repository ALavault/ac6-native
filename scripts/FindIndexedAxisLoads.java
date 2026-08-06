// Find the bounded PPC sequence used by the canonical input sign-split table:
//   materialize 0x8201250c, load an axis slot, add 0x14, scale by two,
//   then consume it as lhzx(..., r3).  This reports candidates only; it does
//   not claim that r3 is the live XInput device.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class FindIndexedAxisLoads extends GhidraScript {
    private static long u32(byte[] b) {
        return ((long)(b[0] & 0xff) << 24) | ((long)(b[1] & 0xff) << 16) |
               ((long)(b[2] & 0xff) << 8) | (long)(b[3] & 0xff);
    }
    private static int s16(long v) {
        int n = (int)(v & 0xffffL);
        return n >= 0x8000 ? n - 0x10000 : n;
    }
    private static boolean isOpcode(long w, int opcode) {
        return (w >>> 26) == opcode;
    }
    private static int rt(long w) { return (int)((w >>> 21) & 31); }
    private static int ra(long w) { return (int)((w >>> 16) & 31); }
    private static int rb(long w) { return (int)((w >>> 11) & 31); }

    private Instruction next(Instruction instruction) {
        Address address = instruction.getMaxAddress().add(1);
        return currentProgram.getListing().getInstructionAt(address);
    }
    private Instruction previous(Instruction instruction) {
        Address address = instruction.getMinAddress().subtract(1);
        return currentProgram.getListing().getInstructionAt(address);
    }

    @Override
    protected void run() throws Exception {
        long hits = 0;
        for (Instruction lis : currentProgram.getListing().getInstructions(true)) {
            byte[] first = lis.getBytes();
            if (first.length != 4) continue;
            long w0 = u32(first);
            // lis rX,-0x7dff (upper half of 0x8201250c)
            if (!isOpcode(w0, 15) || s16(w0) != -0x7dff) continue;
            int tableReg = rt(w0);
            Instruction add = next(lis);
            for (int pair = 1; pair <= 16 && add != null; ++pair, add = next(add)) {
                if (add.getBytes().length != 4) break;
                long w1 = u32(add.getBytes());
                if (!isOpcode(w1, 14) || rt(w1) != tableReg || ra(w1) != tableReg ||
                    s16(w1) != 0x250c) continue;

            Instruction cursor = add;
            for (int distance = 1; distance <= 64; ++distance) {
                cursor = next(cursor);
                if (cursor == null || cursor.getBytes().length != 4) break;
                long w = u32(cursor.getBytes());
                // Keep both the table load and any lhzx(r3, index) in this
                // bounded window.  Register liveness is intentionally left
                // to the follow-up inspection.
                if (isOpcode(w, 31) && ((w >>> 1) & 0x3ff) == 279 &&
                    (ra(w) == 3 || rb(w) == 3)) {
                    Instruction load = cursor;
                    int tableOffset = 0;
                    boolean tableLoadFound = false;
                    int indexReg = ra(w) == 3 ? rb(w) : ra(w);
                    Instruction prior = load;
                    for (int back = 1; back <= 8; ++back) {
                        prior = previous(prior);
                        if (prior == null || prior.getBytes().length != 4) break;
                        long wp = u32(prior.getBytes());
                        if (isOpcode(wp, 32) && ra(wp) == tableReg) {
                            tableOffset = s16(wp);
                            tableLoadFound = true;
                            break;
                        }
                    }
                Function owner = currentProgram.getFunctionManager()
                    .getFunctionContaining(load.getAddress());
                println(String.format(
                    "table=%s table_offset=%s load=%s index_reg=r%d dest=r%d owner=%s",
                    add.getAddress(), tableLoadFound ? Integer.toString(tableOffset) : "unknown",
                    load.getAddress(), indexReg, rt(w),
                    owner == null ? "<no-function>" :
                        owner.getEntryPoint() + " " + owner.getName()));
                hits++;
                }
            }
            }
        }
        println("SUMMARY indexed_axis_loads=" + hits + " table=0x8201250c");
    }
}
