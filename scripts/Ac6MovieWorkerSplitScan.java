// Split lis/addi-ori scan for the movie-worker addresses: upper half 0x8291
// shared by all three, lower halves 0x6E3C/0x6E2C/0x6E08.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class Ac6MovieWorkerSplitScan extends GhidraScript {
    private final long[] lowers = {0x6E3CL, 0x6E2CL, 0x6E08L};

    private boolean matchesLower(long v) {
        for (long t : lowers) if (v == t) return true;
        return false;
    }

    @Override
    protected void run() throws Exception {
        Address start = toAddr(0x82090000L);
        Address end = toAddr(0x823d0c6bL);
        long scanned = 0;
        long hisHits = 0;
        long loHits = 0;
        for (Instruction instr : currentProgram.getListing().getInstructions(new AddressSet(start, end), true)) {
            scanned++;
            String mnem = instr.getMnemonicString();
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (!(obj instanceof Scalar)) continue;
                    long v = ((Scalar) obj).getUnsignedValue();
                    if (v == 0x8291L) {
                        hisHits++;
                        Function fn = getFunctionContaining(instr.getAddress());
                        println("AC6_SPLIT HI at=" + instr.getAddress() + " fn=" + (fn == null ? "NONE" : fn.getName() + "@" + fn.getEntryPoint()) + " instr=" + instr.toString());
                    }
                    if (matchesLower(v)) {
                        loHits++;
                        Function fn = getFunctionContaining(instr.getAddress());
                        println("AC6_SPLIT LO=0x" + Long.toHexString(v) + " at=" + instr.getAddress() + " fn=" + (fn == null ? "NONE" : fn.getName() + "@" + fn.getEntryPoint()) + " instr=" + instr.toString());
                    }
                }
            }
        }
        println("AC6_SPLIT done scanned=" + scanned + " hiHits=" + hisHits + " loHits=" + loHits);
    }
}
