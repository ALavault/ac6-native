// Disassembles the given addresses in the NON-CANONICAL Xenon scratch project,
// then lists the instructions reached. Never run against ghidra-projects/ace-combat-6.
//
// EVERY ARGUMENT AFTER THE OUTPUT FILE IS A START ADDRESS. None of them is an
// end address, and passing two hex values does not disassemble the range
// between them -- it disassembles two blocks, one per value.
//
// That mattered. A verification run passed `820A7070 820A7EB0` intending the
// whole of the unit constructor, got two 300-instruction blocks -- the second
// beginning past the function's end -- and covered 300 of the 912 instructions
// in the range. The listing ended on a plausible instruction with no marker, so
// a negative drawn over a third of a function looked exhaustive. The claim
// under test happened to survive the corrected 912/912 read, which is luck, not
// method.
//
// The cap is 300 instructions per start. It is now stated in the output rather
// than left to the reader, following the rule Ac6XenonForceScan already keeps:
// a scan reports its own denominator. To cover more, pass successive starts
// 300 instructions (0x4B0 bytes) apart and check the trailer of each block.
//
// usage: Ac6XenonDisasm OUT startHex [more startHex...]
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class Ac6XenonDisasm extends GhidraScript {
  private static final int kMaxInstructions = 300;

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (!currentProgram.getDomainFile().getPathname().contains("default.xex")) return;
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    for (int i = 1; i < args.length; ++i) {
      long v = Long.parseLong(args[i], 16);
      Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v);
      boolean ok = disassemble(a);
      out.println("=== " + args[i] + " disassemble=" + ok + "  (this is a START, not a range end)");
      Instruction ins = getInstructionAt(a);
      int n = 0;
      Address last = a;
      while (ins != null && n < kMaxInstructions) {
        out.println(ins.getAddress() + "  " + ins);
        last = ins.getAddress();
        ins = ins.getNext();
        n++;
      }
      // The trailer is the point of this script's existence: without it a
      // truncated block and a finished function look identical.
      if (n == kMaxInstructions && ins != null) {
        out.println("--- " + args[i] + " emitted=" + n + " last=" + last +
                    "  TRUNCATED AT THE " + kMaxInstructions + "-INSTRUCTION CAP;" +
                    " the code continues at " + ins.getAddress() +
                    " -- a negative over this block is NOT a negative over the function");
      } else {
        out.println("--- " + args[i] + " emitted=" + n + " last=" + last +
                    "  ended because the listing ran out, not at the cap");
      }
    }
    out.close();
    println("WROTE " + args[0]);
  }
}
