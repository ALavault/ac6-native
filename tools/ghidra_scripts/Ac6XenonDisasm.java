// Disassembles the given addresses in the NON-CANONICAL Xenon scratch project,
// then lists the instructions reached. Never run against ghidra-projects/ace-combat-6.
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class Ac6XenonDisasm extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (!currentProgram.getDomainFile().getPathname().contains("default.xex")) return;
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    for (int i = 1; i < args.length; ++i) {
      long v = Long.parseLong(args[i], 16);
      Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v);
      boolean ok = disassemble(a);
      out.println("=== " + args[i] + " disassemble=" + ok);
      Instruction ins = getInstructionAt(a);
      int n = 0;
      while (ins != null && n < 300) {
        out.println(ins.getAddress() + "  " + ins);
        ins = ins.getNext();
        n++;
      }
    }
    out.close();
    println("WROTE " + args[0]);
  }
}
