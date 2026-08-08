// Read-only word dump plus function dump from the Xenon-language import.
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class Ac6XenonWords extends GhidraScript {
  private Address at(long v) {
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v);
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    DecompInterface decomp = new DecompInterface();
    decomp.openProgram(currentProgram);
    for (int i = 1; i < args.length; ++i) {
      String[] parts = args[i].split(":");
      long value = Long.parseLong(parts[0], 16);
      if (parts.length == 3 && parts[1].equals("w")) {
        int count = Integer.parseInt(parts[2]);
        out.println("=== words at " + parts[0] + " x" + count);
        for (int k = 0; k < count; ++k) {
          Address a = at(value + 4L * k);
          int w = currentProgram.getMemory().getInt(a);
          out.println(String.format("%s  %08x  %.6g", a, w, Float.intBitsToFloat(w)));
        }
        continue;
      }
      Address entry = at(value);
      Function f = getFunctionAt(entry);
      out.println("=== " + parts[0] + " function=" + (f == null ? "none" : f.getName()));
      if (f == null) continue;
      InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
      while (it.hasNext()) { Instruction ins = it.next(); out.println(ins.getAddress() + "  " + ins); }
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
