// Decompile the movie-worker SetEvent function and list its callers.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.util.task.ConsoleTaskMonitor;

public class Ac6MovieWorkerDecompile extends GhidraScript {
    private void decompileAndCallers(long addrLong, DecompInterface decomp) throws Exception {
        Address addr = toAddr(addrLong);
        Function fn = getFunctionAt(addr);
        if (fn == null) { println("AC6_DECOMP no function at " + addr); return; }
        println("AC6_DECOMP === " + fn.getName() + " @ " + addr + " ===");
        DecompileResults res = decomp.decompileFunction(fn, 60, new ConsoleTaskMonitor());
        if (res != null && res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("AC6_DECOMP decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        println("AC6_DECOMP callers of " + fn.getName() + ":");
        for (ghidra.program.model.symbol.Reference ref : getReferencesTo(addr)) {
            Function caller = getFunctionContaining(ref.getFromAddress());
            println("AC6_DECOMP  caller_from=" + ref.getFromAddress() + " fn=" + (caller == null ? "NONE" : caller.getName() + "@" + caller.getEntryPoint()) + " type=" + ref.getReferenceType());
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decompileAndCallers(0x823ad380L, decomp); // pure SetEvent function
        decompileAndCallers(0x823ad9c0L, decomp); // has all 3 addresses
        decompileAndCallers(0x823adbd8L, decomp); // has all 3 addresses too
        decomp.dispose();
    }
}
