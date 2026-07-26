// Read-only bounded inspection for a suspected XEX thunk/import island.
// Usage: InspectFunctionIsland.java <start-address> <end-address>
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;

public class InspectFunctionIsland extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (getScriptArgs().length != 2) {
            throw new IllegalArgumentException("expected <start-address> <end-address>");
        }
        Address start = currentProgram.getAddressFactory().getAddress(getScriptArgs()[0]);
        Address end = currentProgram.getAddressFactory().getAddress(getScriptArgs()[1]);
        if (start == null || end == null || start.compareTo(end) > 0) {
            throw new IllegalArgumentException("invalid address range");
        }
        FunctionManager functions = currentProgram.getFunctionManager();
        for (Function function : functions.getFunctions(start, true)) {
            if (function.getEntryPoint().compareTo(end) > 0) {
                break;
            }
            int instructionCount = 0;
            for (Instruction ignored : currentProgram.getListing().getInstructions(function.getBody(), true)) {
                instructionCount++;
            }
            println(String.format(
                "entry=%s body=%s instructions=%d noreturn=%s external=%s thunk=%s name=%s",
                function.getEntryPoint(), function.getBody(), instructionCount,
                function.hasNoReturn(), function.isExternal(), function.isThunk(), function.getName()));
        }
    }
}
