// Print the registers an instruction names as inputs and results, and the
// language's own names for the vector files. The register-file bridge in
// MicroExecuteFunction.java keys on those names, and it has never been checked
// that the names it sees are the names it expects.
//
// Usage: -postScript Ac6RegisterProbe.java ADDR[,ADDR...]
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Instruction;

public class Ac6RegisterProbe extends GhidraScript {
    @Override
    protected void run() throws Exception {
        for (String text : getScriptArgs()[0].split(",")) {
            Instruction instruction = getInstructionAt(toAddr(Long.decode(text) & 0xffffffffL));
            println("AC6_PROBE " + text + "  " + instruction);
            for (Object result : instruction.getResultObjects()) {
                println("   result " + result.getClass().getSimpleName() + " " + result);
            }
            for (Object input : instruction.getInputObjects()) {
                println("   input  " + input.getClass().getSimpleName() + " " + input);
            }
        }
        for (String name : new String[] {"v13", "vs45", "vr13", "v0", "vs32", "vr0"}) {
            Register register = currentProgram.getLanguage().getRegister(name);
            println("AC6_REG " + name + " -> " + (register == null ? "absent"
                : register.getAddress() + " size=" + register.getBitLength() / 8
                  + " parent=" + (register.getParentRegister() == null ? "-"
                                  : register.getParentRegister().getName())));
        }
    }
}
