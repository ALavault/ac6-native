// Read-only dump of sub_821E5FD0 and sub_821EFAF0, the only two static
// writers of displacement +0x2af8 found by FindStoresAtDisplacement.java
// against ghidra-projects/ac6-us. Establishes what value each writes and
// under what condition, to identify the real completion signal for the
// spin-wait in sub_821E64A8 (0x821e6500..0x821e6508).
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class DumpUsWaitWriters extends GhidraScript {
    private void dumpFunctionAt(long value) throws Exception {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) {
            println("NO_FUNCTION_AT=" + address);
            return;
        }
        println("=== FUNCTION " + function.getEntryPoint() + " " + function.getName() +
            " body=" + function.getBody() + " ===");
        AddressSetView body = function.getBody();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(body, true);
        int count = 0;
        int cap = 400;
        while (instructions.hasNext() && count < cap) {
            Instruction insn = instructions.next();
            println("INSN " + insn.getAddress() + " " + insn.toString());
            count++;
        }
        println("INSTRUCTION_COUNT=" + count + (count >= cap ? " (CAPPED)" : " (COMPLETE)"));
        println("=== END " + function.getEntryPoint() + " ===");
    }

    @Override
    public void run() throws Exception {
        dumpFunctionAt(0x821e5fd0L);
        dumpFunctionAt(0x821efaf0L);
    }
}
