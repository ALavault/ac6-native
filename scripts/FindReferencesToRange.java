// List every reference into an address range, with the referring function.
//
// Useful when a table has no RTTI locator and its purpose has to be read from
// how code reaches it: a constructor storing its base at object offset 0 means
// a vtable, an indexed load means a dispatch table, and a single pointer means
// a descriptor. Read-only.
//
// usage: FindReferencesToRange.java LOW HIGH
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class FindReferencesToRange extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            printerr("usage: FindReferencesToRange.java LOW HIGH");
            return;
        }
        long low = Long.decode(args[0]);
        long high = Long.decode(args[1]);

        int total = 0;
        for (long value = low; value < high; value += 4) {
            Address target = toAddr(value);
            ReferenceIterator references =
                    currentProgram.getReferenceManager().getReferencesTo(target);
            while (references.hasNext()) {
                Reference reference = references.next();
                Address from = reference.getFromAddress();
                Function function = currentProgram.getFunctionManager()
                        .getFunctionContaining(from);
                Instruction instruction =
                        currentProgram.getListing().getInstructionAt(from);
                println(String.format("AC6_REF to=%s from=%s function=%s type=%s %s",
                        target, from,
                        function == null ? "none" : function.getEntryPoint().toString(),
                        reference.getReferenceType(),
                        instruction == null ? "" : instruction.toString()));
                total++;
            }
        }
        println("AC6_REF_TOTAL " + total);
    }
}
