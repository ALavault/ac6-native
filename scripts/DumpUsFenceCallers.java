// Read-only characterization of the four static call sites to
// sub_821E5FD0 (the +0x2af8 counting-fence adjust/release helper), found by
// FindDirectCallsTo.java against ghidra-projects/ac6-us. For each site:
// containing function, and the five instructions immediately before the
// call (to read what value is loaded into r7, the delta argument).
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpUsFenceCallers extends GhidraScript {
    private void dumpSite(long value) throws Exception {
        Address address = toAddr(value);
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        println("=== CALL SITE " + address + " in " +
            (function == null ? "<no function>" : function.getEntryPoint() + " " + function.getName()) +
            " ===");
        Instruction insn = currentProgram.getListing().getInstructionAt(address);
        // Walk backward up to 12 instructions to capture the argument setup.
        Instruction cursor = insn;
        java.util.List<Instruction> before = new java.util.ArrayList<>();
        for (int i = 0; i < 12 && cursor != null; i++) {
            cursor = currentProgram.getListing().getInstructionBefore(cursor.getAddress());
            if (cursor == null) break;
            before.add(0, cursor);
        }
        for (Instruction i : before) {
            println("PRE  " + i.getAddress() + " " + i);
        }
        println("CALL " + insn.getAddress() + " " + insn);
        Instruction after = insn;
        for (int i = 0; i < 4; i++) {
            after = currentProgram.getListing().getInstructionAfter(after.getAddress());
            if (after == null) break;
            println("POST " + after.getAddress() + " " + after);
        }
        println("=== END " + address + " ===");
    }

    @Override
    public void run() throws Exception {
        dumpSite(0x821e5f80L);
        dumpSite(0x821e6130L);
        dumpSite(0x821ef2acL);
        dumpSite(0x821ef2f4L);
    }
}
