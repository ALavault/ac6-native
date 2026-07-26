// Decompile the function containing ADDRESS.
// @category AC6

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: DecompileAt.java ADDRESS");
            return;
        }
        Address address = toAddr(args[0]);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) {
            printerr("no function contains " + address);
            return;
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        if (!result.decompileCompleted()) {
            printerr(result.getErrorMessage());
            return;
        }
        println("FUNCTION " + function.getEntryPoint() + " " + function.getName());
        println(result.getDecompiledFunction().getC());
        decompiler.dispose();
    }
}
