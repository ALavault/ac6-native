import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;

public class Ac6XenonGaps extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(args[0], "UTF-8");
    long lo = Long.parseLong(args[1], 16), hi = Long.parseLong(args[2], 16);
    AddressSet decoded = new AddressSet();
    InstructionIterator it = currentProgram.getListing().getInstructions(true);
    while (it.hasNext()) { Instruction i = it.next(); decoded.add(i.getMinAddress(), i.getMaxAddress()); }
    for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
      if (!b.isExecute()) continue;
      AddressSet blockSet = new AddressSet(b.getStart(), b.getEnd());
      AddressSet gaps = blockSet.subtract(decoded);
      long count = 0, bytes = 0, biggest = 0;
      for (AddressRange r : gaps.getAddressRanges()) { count++; bytes += r.getLength(); biggest = Math.max(biggest, r.getLength()); }
      out.println(String.format("%s gaps=%d bytes=%d biggest=%d", b.getName(), count, bytes, biggest));
      Address a = b.getStart().getNewAddress(Math.max(lo, b.getStart().getOffset()));
      Address z = b.getStart().getNewAddress(Math.min(hi, b.getEnd().getOffset()));
      if (a.compareTo(z) >= 0) continue;
      AddressSet window = gaps.intersect(new AddressSet(a, z));
      out.println("  window " + a + ".." + z + " gap_bytes=" + window.getNumAddresses());
      for (AddressRange r : window.getAddressRanges())
        out.println(String.format("  GAP %s..%s len=%d", r.getMinAddress(), r.getMaxAddress(), r.getLength()));
    }
    out.close();
    println("WROTE");
  }
}
