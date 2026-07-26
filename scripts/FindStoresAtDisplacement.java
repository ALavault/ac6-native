// List direct stw/sth/stb stores whose effective address uses a displacement.
// This is a static candidate finder; it does not infer pointer provenance.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class FindStoresAtDisplacement extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "usage: FindStoresAtDisplacement <displacement>");
        }
        String displacement = Long.decode(args[0]).toString();
        String[] hexForms = {
            "0x" + Long.toHexString(Long.decode(args[0])),
            "-0x" + Long.toHexString(-Long.decode(args[0]))
        };
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            for (Instruction instruction : currentProgram.getListing()
                    .getInstructions(function.getBody(), true)) {
                String mnemonic = instruction.getMnemonicString();
                if (!(mnemonic.equals("stw") || mnemonic.equals("sth")
                        || mnemonic.equals("stb"))) {
                    continue;
                }
                String text = instruction.toString();
                boolean match = text.contains(displacement + "(");
                for (String hex : hexForms) {
                    match |= text.contains(hex + "(");
                }
                if (match) {
                    println(function.getEntryPoint() + " " + instruction.getAddress()
                        + " " + instruction);
                }
            }
        }
    }
}
