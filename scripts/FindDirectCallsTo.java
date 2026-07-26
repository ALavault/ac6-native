// Finds direct PPC call instructions to one or more exact target addresses.
// Read-only helper for reconciling function boundaries in the qualified XEX.
// @category AC6

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class FindDirectCallsTo extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (getScriptArgs().length == 0) {
            throw new IllegalArgumentException("usage: FindDirectCallsTo <address>...");
        }
        Set<Address> targets = new HashSet<>();
        for (String argument : getScriptArgs()) targets.add(toAddr(Long.decode(argument)));

        for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
            for (Address destination : instruction.getFlows()) {
                if (targets.contains(destination)) {
                    println(instruction.getAddress() + " -> " + destination + " " + instruction);
                }
            }
        }
    }
}
