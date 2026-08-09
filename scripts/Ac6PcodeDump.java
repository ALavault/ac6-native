// Dump the raw p-code of an instruction range, CALLOTHER operands included.
//
// A CALLOTHER behaviour registered with EmulatorHelper receives whatever the
// SLEIGH module chose to pass, and nothing else says what that is. Reading the
// module's source would answer it; measuring the p-code answers it for the
// build actually in use, which is the one that will run.
//
// Usage:
//   -postScript Ac6PcodeDump.java START END OUT_TXT
// Read-only. Run with -readOnly -noanalysis.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Language;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import java.io.PrintWriter;

public class Ac6PcodeDump extends GhidraScript {

    private String describe(Varnode node) {
        if (node == null) {
            return "-";
        }
        String space = node.getAddress().getAddressSpace().getName();
        if (node.isRegister()) {
            var register = currentProgram.getRegister(node.getAddress(), node.getSize());
            if (register != null) {
                return register.getName() + ":" + node.getSize();
            }
        }
        if (node.isConstant()) {
            return "0x" + Long.toHexString(node.getOffset()) + ":" + node.getSize();
        }
        return space + "[0x" + Long.toHexString(node.getOffset()) + "]:" + node.getSize();
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException("usage: Ac6PcodeDump START END OUT_TXT");
        }
        Address start = toAddr(Long.decode(args[0]) & 0xffffffffL);
        Address end = toAddr(Long.decode(args[1]) & 0xffffffffL);
        Language language = currentProgram.getLanguage();

        try (PrintWriter out = new PrintWriter(args[2])) {
            out.println("# Raw p-code over " + start + ".." + end);
            out.println("# Produced by scripts/Ac6PcodeDump.java. Read-only, no oracle.");
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(start, true)) {
                if (instruction.getAddress().compareTo(end) >= 0) {
                    break;
                }
                out.println(instruction.getAddress() + "  " + instruction);
                for (PcodeOp op : instruction.getPcode()) {
                    StringBuilder line = new StringBuilder("    ");
                    line.append(describe(op.getOutput())).append(" = ");
                    if (op.getOpcode() == PcodeOp.CALLOTHER) {
                        int index = (int) op.getInput(0).getOffset();
                        String name = language.getUserDefinedOpName(index);
                        line.append("CALLOTHER<").append(name == null ? index : name).append(">(");
                        for (int i = 1; i < op.getNumInputs(); ++i) {
                            line.append(i == 1 ? "" : ", ").append(describe(op.getInput(i)));
                        }
                        line.append(")");
                    }
                    else {
                        line.append(op.getMnemonic()).append("(");
                        for (int i = 0; i < op.getNumInputs(); ++i) {
                            line.append(i == 0 ? "" : ", ").append(describe(op.getInput(i)));
                        }
                        line.append(")");
                    }
                    out.println(line);
                }
            }
        }
        println("AC6_PCODE_DUMP out=" + args[2]);
    }
}
