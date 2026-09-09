// Decompile Function_823AD0D8 / Function_823AD1C0 (the "progression" branch
// taken when *(iVar2+0x130) != 0, per r487), plus their callers, to trace
// what the 0x130 offset field represents and what writes it.
// @category AC6
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.util.task.ConsoleTaskMonitor;

public class Ac6ProgressionDecompile extends GhidraScript {
    private void decompileAndCallers(long addrLong, DecompInterface decomp) throws Exception {
        Address addr = toAddr(addrLong);
        Function fn = getFunctionAt(addr);
        if (fn == null) { println("AC6_PROG no function at " + addr); return; }
        println("AC6_PROG === " + fn.getName() + " @ " + addr + " ===");
        DecompileResults res = decomp.decompileFunction(fn, 60, new ConsoleTaskMonitor());
        if (res != null && res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("AC6_PROG decompile failed: " + (res == null ? "null" : res.getErrorMessage()));
        }
        println("AC6_PROG callers of " + fn.getName() + ":");
        for (ghidra.program.model.symbol.Reference ref : getReferencesTo(addr)) {
            Function caller = getFunctionContaining(ref.getFromAddress());
            println("AC6_PROG  caller_from=" + ref.getFromAddress() + " fn=" + (caller == null ? "NONE" : caller.getName() + "@" + caller.getEntryPoint()) + " type=" + ref.getReferenceType());
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decompileAndCallers(0x823ad0d8L, decomp);
        decompileAndCallers(0x823ad1c0L, decomp);
        // also re-decompile the caller itself for reference to the 0x130 check
        decompileAndCallers(0x823ad9c0L, decomp);
        decomp.dispose();
    }
}
