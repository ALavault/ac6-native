// List direct loads using a displacement. This is a candidate finder only;
// it does not infer pointer provenance or object type.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class FindLoadsAtDisplacement extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "usage: FindLoadsAtDisplacement <displacement>");
        }
        long value = Long.decode(args[0]);
        String decimal = Long.toString(value);
        String[] hexForms = {
            "0x" + Long.toHexString(value),
            "-0x" + Long.toHexString(-value)
        };
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String mnemonic = instruction.getMnemonicString();
                if (!(mnemonic.equals("lwz") || mnemonic.equals("lha")
                        || mnemonic.equals("lhz") || mnemonic.equals("lbz")
                        || mnemonic.equals("lfs") || mnemonic.equals("lfd"))) {
                    continue;
                }
                String text = instruction.toString();
                int comma = text.indexOf(',');
                if (comma < 0) continue;
                String operand = text.substring(comma + 1).trim();
                boolean match = operand.startsWith(decimal + "(");
                for (String hex : hexForms) match |= operand.startsWith(hex + "(");
                if (match) {
                    println(function.getEntryPoint() + " "
                        + instruction.getAddress() + " " + instruction);
                }
            }
        }
    }
}
