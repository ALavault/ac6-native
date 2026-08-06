// Count defined instructions in byte-qualified AC6 helper ranges.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class CountQualifiedInstructions extends GhidraScript {
    private void report(String startText, String endText) {
        Address start = toAddr(startText);
        Address end = toAddr(endText);
        Function function = getFunctionAt(start);
        int count = 0;
        Address first = null;
        Address last = null;
        for (Instruction instruction : currentProgram.getListing()
                .getInstructions(new AddressSet(start, end), true)) {
            if (first == null) {
                first = instruction.getAddress();
            }
            last = instruction.getAddress();
            count++;
        }
        println(start + ".." + end + " instructions=" + count +
                " first=" + first + " last=" + last +
                " function=" + (function == null ? "<none>" : function.getName()) +
                " body=" + (function == null ? "<none>" : function.getBody()));
    }

    @Override
    protected void run() throws Exception {
        report("0x821C3BE8", "0x821C402C");
        report("0x821C4FA0", "0x821C5254");
        report("0x821C5258", "0x821C56F4");
        report("0x821C56F8", "0x821C59AC");
    }
}
