// Decompile the accessor functions that return the struct whose +0x130
// field gates Function_823AD9C0's branch (per r488), and scan .text for
// any store instruction (stw/sth/stb) writing to offset 0x130 of a
// register, to find what SETS that field.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.util.task.ConsoleTaskMonitor;

public class Ac6ProgressionAccessors extends GhidraScript {
    private void decompileAndCallers(long addrLong, DecompInterface decomp) throws Exception {
        Address addr = toAddr(addrLong);
        Function fn = getFunctionAt(addr);
        if (fn == null) { println("AC6_ACC no function at " + addr); return; }
        println("AC6_ACC === " + fn.getName() + " @ " + addr + " ===");
        DecompileResults res = decomp.decompileFunction(fn, 60, new ConsoleTaskMonitor());
        if (res != null && res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("AC6_ACC decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decompileAndCallers(0x82382a18L, decomp);
        decompileAndCallers(0x82382a10L, decomp);
        decomp.dispose();

        // Scan .text for stw rX,0x130(rY) patterns (store word at offset 0x130)
        Address start = toAddr(0x82090000L);
        Address end = toAddr(0x823d0c6bL);
        long hits = 0;
        for (Instruction instr : currentProgram.getListing().getInstructions(new AddressSet(start, end), true)) {
            String mnem = instr.getMnemonicString();
            if (!(mnem.equals("stw") || mnem.equals("sth") || mnem.equals("stb"))) continue;
            for (int i = 0; i < instr.getNumOperands(); i++) {
                for (Object obj : instr.getOpObjects(i)) {
                    if (obj instanceof Scalar && ((Scalar) obj).getUnsignedValue() == 0x130L) {
                        hits++;
                        Function fn = getFunctionContaining(instr.getAddress());
                        println("AC6_STORE130 at=" + instr.getAddress() + " fn=" + (fn == null ? "NONE" : fn.getName() + "@" + fn.getEntryPoint()) + " instr=" + instr.toString());
                    }
                }
            }
        }
        println("AC6_STORE130 done hits=" + hits);
    }
}
