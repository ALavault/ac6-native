import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;

public class ReferencesTo extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: ReferencesTo <address>");
        }
        Address target = toAddr(Long.decode(args[0]));
        for (Reference reference : currentProgram.getReferenceManager().getReferencesTo(target)) {
            println(reference.getFromAddress() + " " + reference.getReferenceType());
        }
    }
}
