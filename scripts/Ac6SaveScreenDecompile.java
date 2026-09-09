// Decompile the three save-screen state-machine functions hooked by
// ac6_route_sync_ntsc_uj.cpp (rex_sub_821C3800/821C5268/821C5708), plus
// any function that writes offset+28 ("type") on the screen struct, to
// understand what "type28=30" (the wait-pulse target of
// us-pretype28-startup.steps) actually represents and whether reaching
// it can legitimately take several real seconds.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.util.task.ConsoleTaskMonitor;

public class Ac6SaveScreenDecompile extends GhidraScript {
    private void decompileAndCallers(long addrLong, DecompInterface decomp) throws Exception {
        Address addr = toAddr(addrLong);
        Function fn = getFunctionAt(addr);
        if (fn == null) { println("AC6_SAVE no function at " + addr); return; }
        println("AC6_SAVE === " + fn.getName() + " @ " + addr + " ===");
        DecompileResults res = decomp.decompileFunction(fn, 60, new ConsoleTaskMonitor());
        if (res != null && res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("AC6_SAVE decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        println("AC6_SAVE callers of " + fn.getName() + ":");
        for (ghidra.program.model.symbol.Reference ref : getReferencesTo(addr)) {
            Function caller = getFunctionContaining(ref.getFromAddress());
            println("AC6_SAVE  caller_from=" + ref.getFromAddress() + " fn=" + (caller == null ? "NONE" : caller.getName() + "@" + caller.getEntryPoint()) + " type=" + ref.getReferenceType());
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decompileAndCallers(0x821C3800L, decomp);
        decompileAndCallers(0x821C5268L, decomp);
        decompileAndCallers(0x821C5708L, decomp);
        decomp.dispose();
    }
}
