// Describes bounded addresses without changing the Ghidra project.
// @category AC6.Evidence

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DescribeQualifiedAddresses extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0) {
            throw new IllegalArgumentException(
                "usage: DescribeQualifiedAddresses <address>...");
        }
        println("program=" + currentProgram.getName());
        println("sha256=" + currentProgram.getExecutableSHA256());
        for (String argument : args) {
            Address address = toAddr(Long.decode(argument));
            MemoryBlock block = currentProgram.getMemory().getBlock(address);
            Data data = currentProgram.getListing().getDataContaining(address);
            println("address=" + address + " block=" +
                (block == null ? "<none>" : block.getName()) + " data=" +
                (data == null ? "<undefined>" : data.toString()));

            SymbolIterator before = currentProgram.getSymbolTable()
                .getSymbolIterator(address, false);
            if (before.hasNext()) {
                Symbol symbol = before.next();
                println("  symbol-before=" + symbol.getAddress() + " " +
                    symbol.getName(true));
            }
            SymbolIterator after = currentProgram.getSymbolTable()
                .getSymbolIterator(address, true);
            if (after.hasNext()) {
                Symbol symbol = after.next();
                println("  symbol-after=" + symbol.getAddress() + " " +
                    symbol.getName(true));
            }
            long references = 0;
            for (Reference ignored : currentProgram.getReferenceManager()
                    .getReferencesTo(address)) {
                references++;
            }
            println("  references=" + references);
        }
    }
}
