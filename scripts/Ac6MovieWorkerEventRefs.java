// Cross-references to the three "AC6 movie worker" kernel-object addresses
// (xboxkrnl_threading.cpp:102-104), for r487's static read of what guest
// code sets/waits on them and what gates the KeSetEvent call.
//
// Usage: -postScript Ac6MovieWorkerEventRefs.java
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class Ac6MovieWorkerEventRefs extends GhidraScript {
    private void dump(String label, long addrLong) throws Exception {
        Address addr = toAddr(addrLong);
        println("AC6_MOVIEWORKER === " + label + " @ " + addr + " ===");
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
        int count = 0;
        while (refs.hasNext()) {
            Reference ref = refs.next();
            Address from = ref.getFromAddress();
            count++;
            Function fn = getFunctionContaining(from);
            Instruction instr = currentProgram.getListing().getInstructionAt(from);
            String fnDesc = (fn == null) ? "NO_FUNCTION" : (fn.getName() + "@" + fn.getEntryPoint());
            println("AC6_MOVIEWORKER_REF " + label + " from=" + from + " fn=" + fnDesc
                + " type=" + ref.getReferenceType()
                + " instr=" + (instr == null ? "?" : instr.toString()));
        }
        println("AC6_MOVIEWORKER_COUNT " + label + " refs=" + count);
    }

    @Override
    protected void run() throws Exception {
        dump("SetEvent_0x82916E3C", 0x82916E3CL);
        dump("WaitEvent0_0x82916E2C", 0x82916E2CL);
        dump("WaitEvent1_0x82916E08", 0x82916E08L);
    }
}
