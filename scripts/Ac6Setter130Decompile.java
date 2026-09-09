// Decompile Function_823ADBD8, which writes offset 0x130 twice (per r488's
// store scan) -- candidate setter for the field Function_823AD9C0 reads to
// decide the movie-worker poll vs progression branch.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.util.task.ConsoleTaskMonitor;

public class Ac6Setter130Decompile extends GhidraScript {
    private void decompileAndCallers(long addrLong, DecompInterface decomp) throws Exception {
        Address addr = toAddr(addrLong);
        Function fn = getFunctionAt(addr);
        if (fn == null) { println("AC6_SET130 no function at " + addr); return; }
        println("AC6_SET130 === " + fn.getName() + " @ " + addr + " ===");
        DecompileResults res = decomp.decompileFunction(fn, 60, new ConsoleTaskMonitor());
        if (res != null && res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("AC6_SET130 decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        println("AC6_SET130 callers of " + fn.getName() + ":");
        for (ghidra.program.model.symbol.Reference ref : getReferencesTo(addr)) {
            Function caller = getFunctionContaining(ref.getFromAddress());
            println("AC6_SET130  caller_from=" + ref.getFromAddress() + " fn=" + (caller == null ? "NONE" : caller.getName() + "@" + caller.getEntryPoint()) + " type=" + ref.getReferenceType());
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decompileAndCallers(0x823adbd8L, decomp);
        decomp.dispose();
    }
}
