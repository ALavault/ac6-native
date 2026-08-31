// Finds the pattern: a register loaded via `lwz rX, <disp>(robj)` (a
// pointer field read), later used as the base of a `stw rY, 0x0(rX)`
// within the same function (a write through that pointer's offset 0),
// without the register being redefined in between. Read-only; helps find
// indirect writers that a plain FindStoresAtDisplacement can't see because
// the write target is *(ptr)+0, not a fixed struct displacement.
// @category AC6

import java.util.HashMap;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.lang.Register;
import ghidra.program.model.scalar.Scalar;

public class FindDerefWritesAtDisplacement extends GhidraScript {
    private static String reg(Instruction i, int operand) {
        if (operand >= i.getNumOperands()) return null;
        for (Object value : i.getOpObjects(operand)) {
            if (value instanceof Register) return ((Register) value).getName();
        }
        return null;
    }

    private static Long scalar(Instruction i, int operand) {
        if (operand >= i.getNumOperands()) return null;
        for (Object value : i.getOpObjects(operand)) {
            if (value instanceof Scalar) return ((Scalar) value).getSignedValue();
        }
        return null;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "usage: FindDerefWritesAtDisplacement <displacement>");
        }
        long targetDisp = Long.decode(args[0]);

        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            // Track, per register, the address at which it was last loaded
            // from <targetDisp>(anything), cleared on any redefinition.
            Map<String, Instruction> pending = new HashMap<>();
            for (Instruction insn : currentProgram.getListing().getInstructions(
                    function.getBody(), true)) {
                String mnemonic = insn.getMnemonicString();

                if ("lwz".equals(mnemonic)) {
                    String dest = reg(insn, 0);
                    Long disp = scalar(insn, 1);
                    if (dest != null && disp != null && disp == targetDisp) {
                        pending.put(dest, insn);
                        continue;
                    }
                }

                if ("stw".equals(mnemonic) || "stwx".equals(mnemonic)) {
                    String base = reg(insn, 1);
                    Long disp = scalar(insn, 1);
                    if (base != null && pending.containsKey(base)
                            && disp != null && disp == 0) {
                        println(function.getEntryPoint() + " LOAD " +
                            pending.get(base).getAddress() + " " + pending.get(base) +
                            " -> STORE " + insn.getAddress() + " " + insn);
                    }
                }

                // Any destination write clears that register's pending state.
                String dest = reg(insn, 0);
                if (dest != null && !"lwz".equals(mnemonic)) {
                    pending.remove(dest);
                }
                if ("lwz".equals(mnemonic)) {
                    // already handled above for the matching case; for a
                    // non-matching displacement this still redefines dest.
                    String d = reg(insn, 0);
                    Long disp = scalar(insn, 1);
                    if (d != null && !(disp != null && disp == targetDisp)) {
                        pending.remove(d);
                    }
                }
            }
        }
    }
}
