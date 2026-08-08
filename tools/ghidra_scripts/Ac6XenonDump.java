// Read-only dump of a function's disassembly and decompilation from the Xenon
// -language import. Never writes to the database.
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class Ac6XenonDump extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    println("PROGRAM " + currentProgram.getName() + " lang=" +
            currentProgram.getLanguageID() + " base=" + currentProgram.getImageBase());
    println("RANGE " + currentProgram.getMinAddress() + " .. " + currentProgram.getMaxAddress());
    if (args.length < 2) { println("usage: OUTFILE addr..."); return; }
    DecompInterface decomp = new DecompInterface();
    decomp.openProgram(currentProgram);
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    for (int i = 1; i < args.length; ++i) {
      long value = Long.parseLong(args[i], 16);
      Address entry = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
      Function f = getFunctionAt(entry);
      out.println("=== " + args[i] + " function=" + (f == null ? "none" : f.getName()));
      if (f == null) {
        // No function: disassemble linearly from the entry for a window.
        Instruction ins = getInstructionAt(entry);
        int n = 0;
        while (ins != null && n < 400) {
          out.println(ins.getAddress() + "  " + ins.toString());
          ins = ins.getNext();
          n++;
        }
        out.println("--- no function body, linear listing above");
        continue;
      }
      InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
      while (it.hasNext()) {
        Instruction ins = it.next();
        out.println(ins.getAddress() + "  " + ins.toString());
      }
      out.println("--- decompiled");
      DecompileResults res = decomp.decompileFunction(f, 120, monitor);
      out.println(res.decompileCompleted() && res.getDecompiledFunction() != null
          ? res.getDecompiledFunction().getC() : ("DECOMPILE FAILED: " + res.getErrorMessage()));
    }
    out.close();
    decomp.dispose();
    println("WROTE " + args[0]);
  }
}
