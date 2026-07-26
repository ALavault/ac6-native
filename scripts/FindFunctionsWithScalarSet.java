// Report functions containing instruction operands from every requested scalar set.
// Usage: FindFunctionsWithScalarSet.java <scalar> [<scalar> ...]
// This is a static candidate finder only; it does not infer register liveness.
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class FindFunctionsWithScalarSet extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: FindFunctionsWithScalarSet <scalar> [<scalar> ...]");
        }
        Set<Long> expected = new HashSet<>();
        for (String arg : args) {
            expected.add(Long.decode(arg));
        }

        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            Map<Long, String> matches = new HashMap<>();
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                    for (Object object : instruction.getOpObjects(operand)) {
                        if (!(object instanceof Scalar)) {
                            continue;
                        }
                        long value = ((Scalar)object).getUnsignedValue();
                        if (expected.contains(value)) {
                            matches.put(value, instruction.toString());
                        }
                    }
                }
            }
            if (matches.size() == expected.size()) {
                println(function.getEntryPoint() + " " + function.getName()
                    + " " + matches);
            }
        }
    }
}
