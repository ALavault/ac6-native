// Raw big-endian dword dump of a memory range. usage: Ac6Bytes OUT start end
import java.io.PrintWriter;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class Ac6Bytes extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] a = getScriptArgs();
    PrintWriter out = new PrintWriter(a[0], "UTF-8");
    long s = Long.parseLong(a[1], 16), e = Long.parseLong(a[2], 16);
    for (long p = s; p < e; p += 8) {
      Address addr = currentProgram.getAddressFactory().getAddress(Long.toHexString(p));
      try {
        int w0 = currentProgram.getMemory().getInt(addr);
        int w1 = currentProgram.getMemory().getInt(addr.add(4));
        out.printf("%08x  idx=%2d  %08x %08x%n", p, (p - s) / 8, w0, w1);
      } catch (Exception ex) { out.printf("%08x  <unreadable>%n", p); }
    }
    out.close();
    println("WROTE " + a[0]);
  }
}
