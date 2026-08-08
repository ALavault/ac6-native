// Real cross-references to an address, from Ghidra's reference manager.
//
// Text matching on disassembly misses references for three reasons at once: the
// address may be split across lis/addi with a negative low half, Ghidra may
// render a symbol name instead of the offset, and a data reference may be
// recorded without any literal appearing. Cycle 1149 searched for "0x7ec0" and
// concluded the NTXR literal was unreferenced; this asks the database instead.
//
// usage: Ac6Xrefs OUT 82067ec0 [more addresses...]
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class Ac6Xrefs extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    for (int i = 1; i < args.length; ++i) {
      Address target = currentProgram.getAddressFactory().getAddress(args[i]);
      out.println("=== xrefs to " + args[i] + " ===");
      int n = 0;
      ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(target);
      while (it.hasNext()) {
        Reference r = it.next();
        Address from = r.getFromAddress();
        Function f = getFunctionContaining(from);
        Instruction ins = currentProgram.getListing().getInstructionAt(from);
        out.println("  " + from + "  " + r.getReferenceType() +
                    "  in " + (f == null ? "none" : f.getName() + "@" + f.getEntryPoint()) +
                    "  " + (ins == null ? "(data)" : ins.toString()));
        n++;
      }
      out.println("  total " + n);
      println("xrefs " + args[i] + " = " + n);
    }
    out.close();
    println("WROTE " + args[0]);
  }
}
