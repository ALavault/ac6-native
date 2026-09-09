import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.mem.MemoryBlock;

public class Ac6SanityCheck extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int fnCount = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) { fnCount++; }
        println("AC6_SANITY function_count=" + fnCount);
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println("AC6_SANITY block=" + b.getName() + " start=" + b.getStart() + " end=" + b.getEnd() + " initialized=" + b.isInitialized());
        }
        Address known = toAddr(0x821F5ED0L);
        Function fn = getFunctionContaining(known);
        println("AC6_SANITY entrypoint_fn=" + (fn == null ? "NONE" : fn.getName()));
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(known);
        int c = 0;
        while (refs.hasNext()) { refs.next(); c++; }
        println("AC6_SANITY entrypoint_refs=" + c);
    }
}
