// List stores to a field displacement in functions that directly reference a
// global address.  This is intentionally a static candidate finder; it does
// not claim that a register loaded from the global remains live at the store.
// @category AC6

import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class FindGlobalFieldWrites extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: FindGlobalFieldWrites <global-address> <field-displacement>");
        }
        Address global = toAddr(Long.decode(args[0]));
        String field = Long.decode(args[1]).toString();
        Set<Address> entries = new HashSet<>();
        for (Reference reference : currentProgram.getReferenceManager()
                .getReferencesTo(global)) {
            Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
            if (function != null) {
                entries.add(function.getEntryPoint());
            }
        }
        for (Address entry : entries) {
            Function function = currentProgram.getFunctionManager()
                    .getFunctionAt(entry);
            if (function == null) {
                continue;
            }
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String text = instruction.toString();
                if (text.startsWith("stw ") && text.contains(field + "(")) {
                    println(function.getEntryPoint() + " " + instruction);
                }
            }
        }
    }
}
