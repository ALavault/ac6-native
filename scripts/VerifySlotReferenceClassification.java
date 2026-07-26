// Read-only classification of the first slot-target's non-code WRITE reference.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class VerifySlotReferenceClassification extends GhidraScript {
    private void requireDirectCall(long sourceAddress, long targetAddress, String label) throws Exception {
        Function source = currentProgram.getFunctionManager().getFunctionAt(toAddr(sourceAddress));
        if (source == null) throw new IllegalStateException("missing source");
        for (Function callee : source.getCalledFunctions(monitor)) {
            if (callee.getEntryPoint().getOffset() == targetAddress) {
                println(String.format("AC6_SLOT_BOOTSTRAP=%s %08x -> %08x", label, sourceAddress, targetAddress));
                return;
            }
        }
        throw new IllegalStateException("missing expected direct call");
    }

    @Override public void run() throws Exception {
        Address target = toAddr(0x82758e38L);
        boolean foundWrite = false;
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(target)) {
            if (reference.getFromAddress().getOffset() == 0x823d2b24L) {
                foundWrite = true;
                Function function = currentProgram.getFunctionManager().getFunctionContaining(reference.getFromAddress());
                if (function != null) throw new IllegalStateException("unexpected code consumer " + function.getEntryPoint());
                println("AC6_SLOT_REFERENCE=data-write 823d2b24 -> 82758e38");
            }
        }
        if (!foundWrite) throw new IllegalStateException("missing expected data WRITE reference");
        requireDirectCall(0x821d0cf8L, 0x82138430L, "post-slot-service-a");
        requireDirectCall(0x82138430L, 0x82382eccL, "post-slot-service-a-body");
        requireDirectCall(0x821d0cf8L, 0x82332318L, "post-slot-service-b");
        requireDirectCall(0x82332318L, 0x823d6a7cL, "post-slot-lock");
        requireDirectCall(0x82332318L, 0x8233b790L, "post-slot-service-body");
        requireDirectCall(0x82332318L, 0x823d6a8cL, "post-slot-unlock");
        requireDirectCall(0x8233b790L, 0x8233abe8L, "post-slot-body-branch");
        requireDirectCall(0x8233abe8L, 0x823462a8L, "post-slot-body-virtual-dispatch");
        requireDirectCall(0x8233abe8L, 0x823465f0L, "post-slot-body-terminal-branch");
    }
}
