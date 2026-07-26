// Decompile each function containing an address.
// @category AC6

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileMany extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            printerr("usage: DecompileMany.java ADDRESS [...]");
            return;
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (String arg : args) {
            Address address = toAddr(arg);
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(address);
            if (function == null) {
                printerr("no function contains " + address);
                continue;
            }
            DecompileResults result = decompiler.decompileFunction(
                function, 60, monitor);
            println("FUNCTION " + function.getEntryPoint() + " "
                + function.getName());
            if (!result.decompileCompleted()) {
                println("DECOMPILE_ERROR " + result.getErrorMessage());
                continue;
            }
            println(result.getDecompiledFunction().getC());
        }
        decompiler.dispose();
    }
}
