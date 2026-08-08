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

    // This is a SUBSTRING matcher, not a regex. A pattern written with regex
    // syntax silently searches for the metacharacter itself and returns zero -
    // which reads exactly like a clean negative. That happened once, and was
    // caught only because a known-good hit existed to check against. Warn
    // loudly instead of relying on the next reader having one.
    for (int i = 1; i < args.length; ++i) {
      if (args[i].matches(".*[\\^$\\\\\\[\\]|+*?].*")) {
        println("WARNING: pattern \"" + args[i] + "\" contains regex syntax. " +
                "This tool matches SUBSTRINGS - the metacharacter is searched literally. " +
                "A zero result here is not a negative.");
        out.println("# WARNING regex-looking pattern matched literally: " + args[i]);
      }
    }
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
