import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

public class FindSymbolReferences extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "usage: FindSymbolReferences <exact-symbol-name>");
        }
        boolean found = false;
        for (Symbol symbol : currentProgram.getSymbolTable().getAllSymbols(true)) {
            if (!symbol.getName().contains(args[0])) {
                continue;
            }
            found = true;
            println(symbol.getName() + " " + symbol.getAddress() + " " +
                    symbol.getSymbolType());
            for (Reference reference : currentProgram.getReferenceManager()
                    .getReferencesTo(symbol.getAddress())) {
                println("REF " + reference.getFromAddress() + " " +
                        reference.getReferenceType());
            }
        }
        if (!found) {
            println("NOT_FOUND " + args[0]);
        }
    }
}
