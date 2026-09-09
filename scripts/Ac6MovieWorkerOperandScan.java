// Raw operand scan for the three "AC6 movie worker" constants across .text,
// bypassing the Reference Manager (r487: getReferencesTo returned 0 hits,
// need to know if that's a real absence or an unresolved lis/addi split).
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class Ac6MovieWorkerOperandScan extends GhidraScript {
    private final long[] targets = {0x82916E3CL, 0x82916E2CL, 0x82916E08L};

    private boolean matches(long v) {
        for (long t : targets) if (v == t) return true;
        return false;
    }

    @Override
    protected void run() throws Exception {
        Address start = toAddr(0x82090000L);
        Address end = toAddr(0x823d0c6bL);
        long scanned = 0;
        long hits = 0;
        for (Instruction instr : currentProgram.getListing().getInstructions(new AddressSet(start, end), true)) {
            scanned++;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    long v = -1;
                    if (obj instanceof Scalar) { v = ((Scalar) obj).getUnsignedValue(); }
                    else if (obj instanceof Address) { v = ((Address) obj).getOffset(); }
                    else continue;
                    if (matches(v)) {
                        hits++;
                        Function fn = getFunctionContaining(instr.getAddress());
                        println("AC6_OPSCAN hit=0x" + Long.toHexString(v) + " at=" + instr.getAddress()
                            + " fn=" + (fn == null ? "NONE" : (fn.getName() + "@" + fn.getEntryPoint()))
                            + " instr=" + instr.toString());
                    }
                }
            }
            if (scanned % 200000 == 0) println("AC6_OPSCAN progress scanned=" + scanned);
        }
        println("AC6_OPSCAN done scanned=" + scanned + " hits=" + hits);
    }
}
