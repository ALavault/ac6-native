// Summarize functions that reference a global, without dumping the whole XEX.
// Usage: SummarizeGlobalReferenceFunctions <global-address> <context-instructions>
// @category AC6

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class SummarizeGlobalReferenceFunctions extends GhidraScript {
    private static String register(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object object : instruction.getOpObjects(operand)) {
            if (object instanceof Register) return ((Register) object).getName();
        }
        return null;
    }

    private static Long scalar(Instruction instruction, int operand) {
        if (operand >= instruction.getNumOperands()) return null;
        for (Object object : instruction.getOpObjects(operand)) {
            if (object instanceof Scalar) return ((Scalar) object).getSignedValue();
        }
        return null;
    }

    private static boolean references(Instruction instruction, Address target) {
        for (Reference reference : instruction.getReferencesFrom()) {
            if (target.equals(reference.getToAddress())) return true;
        }
        return false;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException(
                "usage: SummarizeGlobalReferenceFunctions <global-address> <context-instructions>");
        }
        Address global = toAddr(Long.decode(args[0]));
        int context = Integer.parseInt(args[1]);
        Set<Function> functions = new HashSet<>();
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(global)) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            if (function != null) functions.add(function);
        }
        List<Function> ordered = new ArrayList<>(functions);
        ordered.sort(Comparator.comparing(Function::getEntryPoint));
        for (Function function : ordered) {
            List<String> refs = new ArrayList<>();
            List<String> stores8 = new ArrayList<>();
            List<String> virtualC = new ArrayList<>();
            List<Instruction> instructions = new ArrayList<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                instructions.add(instruction);
                if (references(instruction, global)) refs.add(instruction.getAddress().toString());
                String text = instruction.toString();
                if (text.startsWith("stw ") && text.contains("0x8(")) {
                    stores8.add(instruction.getAddress().toString() + " " + text);
                }
                if (instruction.getMnemonicString().equals("lwz")
                        && scalar(instruction, 1) != null
                        && scalar(instruction, 1) == 0xc) {
                    virtualC.add(instruction.getAddress().toString() + " " + text);
                }
            }
            println("FUNCTION " + function.getEntryPoint() + " refs=" + refs
                + " stores8=" + stores8.size() + " slotCLoads=" + virtualC.size()
                + " body=" + function.getBody());
            for (String item : stores8) println("  STORE8 " + item);
            for (String item : virtualC) println("  SLOTC " + item);
            for (int i = 0; i < instructions.size(); i++) {
                Instruction instruction = instructions.get(i);
                if (!references(instruction, global)) continue;
                int first = Math.max(0, i - context);
                int last = Math.min(instructions.size(), i + context + 1);
                for (int j = first; j < last; j++) {
                    println("  CONTEXT " + instructions.get(j));
                }
            }
        }
    }
}
