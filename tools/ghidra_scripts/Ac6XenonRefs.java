// Read-only scan for instructions whose text mentions a target address.
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Function;

public class Ac6XenonRefs extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    long total = 0;
    InstructionIterator it = currentProgram.getListing().getInstructions(true);
    while (it.hasNext()) {
      Instruction ins = it.next();
      total++;
      String text = ins.toString();
      for (int i = 1; i < args.length; ++i) {
        if (text.toLowerCase().contains(args[i].toLowerCase())) {
          Function f = getFunctionContaining(ins.getAddress());
          out.println(args[i] + "  at " + ins.getAddress() + "  in " +
                      (f == null ? "none" : f.getName() + "@" + f.getEntryPoint()) + "  " + text);
        }
      }
    }
    out.println("scanned_instructions " + total);
    out.close();
    println("WROTE " + args[0] + " instructions=" + total);
  }
}
