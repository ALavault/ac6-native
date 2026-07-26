// List functions whose entry points are in [START, END).
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ListFunctionsRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            printerr("usage: ListFunctionsRange.java START END");
            return;
        }
        Address start = toAddr(args[0]);
        Address end = toAddr(args[1]);
        FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(start, true);
        while (functions.hasNext()) {
            Function function = functions.next();
            if (function.getEntryPoint().compareTo(end) >= 0) break;
            println(function.getEntryPoint() + " " + function.getName() + " " + function.getBody());
        }
    }
}
