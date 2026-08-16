import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

/** Dump a bounded Xenon big-endian vtable from the canonical demo project. */
public class DumpDemoVtable extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    final String[] arguments = getScriptArgs();
    if (arguments.length != 2) {
      throw new IllegalArgumentException("expected vtable address and slot count");
    }
    final long baseValue = Long.decode(arguments[0]) & 0xffffffffL;
    final int slots = Integer.decode(arguments[1]);
    if ((baseValue & 3L) != 0L || slots <= 0 || slots > 256) {
      throw new IllegalArgumentException("invalid aligned base or slot count");
    }
    final Address base = toAddr(baseValue);
    println(String.format("vtable=0x%08X slots=%d", baseValue, slots));
    for (int index = 0; index < slots; ++index) {
      final Address address = base.add(index * 4L);
      final long raw = currentProgram.getMemory().getInt(address) & 0xffffffffL;
      final Function function = currentProgram.getFunctionManager()
          .getFunctionAt(toAddr(raw));
      println(String.format("  slot=%d address=0x%08X raw=0x%08X function=%s",
          index, address.getOffset() & 0xffffffffL, raw,
          function == null ? "-" : function.getEntryPoint()));
    }
  }
}
