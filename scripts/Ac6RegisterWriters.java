// Every instruction in a function that WRITES a named register, plus the
// function's own extent. Cycle 1318 exhausted r20 inside 0x821CAA50; a caller
// that sets it before the `bl` is outside that search's population, which is the
// seventeenth shape and the reason this script exists.
//
// Usage: -postScript Ac6RegisterWriters.java REGISTER ADDR[,ADDR...]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class Ac6RegisterWriters extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String wanted = args[0];
        for (String text : args[1].split(",")) {
            Address entry = toAddr(Long.decode(text) & 0xffffffffL);
            Function function = getFunctionContaining(entry);
            if (function == null) {
                println("AC6_WRITERS " + text + " no function");
                continue;
            }
            long count = 0;
            long writes = 0;
            StringBuilder found = new StringBuilder();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                count++;
                boolean mentions = false;
                for (Object input : instruction.getInputObjects()) {
                    if (input.toString().equals(wanted)) {
                        mentions = true;
                    }
                }
                for (Object result : instruction.getResultObjects()) {
                    if (result.toString().equals(wanted)) {
                        writes++;
                        found.append("\n    WRITE ").append(instruction.getAddress())
                             .append("  ").append(instruction);
                    }
                }
                if (mentions) {
                    found.append("\n    read  ").append(instruction.getAddress())
                         .append("  ").append(instruction);
                }
            }
            println("AC6_WRITERS " + wanted + " in " + function.getName() + " @"
                + function.getEntryPoint() + ".." + function.getBody().getMaxAddress()
                + " instructions=" + count + " writes=" + writes + found);
        }
    }
}
