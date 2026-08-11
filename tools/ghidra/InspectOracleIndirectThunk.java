// Read-only qualification for the PAL indirect vtable thunk reached from
// 0x820E1854 with LR 0x820E1858 during the Mission 01 campaign handoff.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.security.MessageDigest;

public class InspectOracleIndirectThunk extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
  private static final long CALL = 0x820E1854L;
  private static final long ENTRY = 0x820F6180L;
  private static final long END = 0x820F618CL;

  private static String hex(byte[] bytes) {
    StringBuilder output = new StringBuilder(bytes.length * 2);
    for (byte value : bytes) {
      output.append(String.format("%02x", value & 0xff));
    }
    return output.toString();
  }

  private void qualify() {
    if (!"ace-combat-6".equals(state.getProject().getName()) ||
        !"default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new AssertionError("wrong canonical AC6 PAL target");
    }
  }

  @Override
  public void run() throws Exception {
    qualify();
    Address entry = toAddr(ENTRY);
    Address end = toAddr(END);
    byte[] bytes = new byte[(int) (END - ENTRY + 4)];
    for (int index = 0; index < bytes.length; ++index) {
      bytes[index] = currentProgram.getMemory().getByte(entry.add(index));
    }
    String digest = hex(MessageDigest.getInstance("SHA-256").digest(bytes));

    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    AddressSet body = new AddressSet(entry, end);
    disassembler.disassemble(entry, body, true);
    Function atEntry = getFunctionAt(entry);
    Function containing = getFunctionContaining(entry);
    Instruction caller = getInstructionAt(toAddr(CALL));
    println("PROJECT " + state.getProject().getName());
    println("PROGRAM " + currentProgram.getName());
    println(String.format(
        "ORACLE_INDIRECT_THUNK caller=0x%08X lr=0x%08X entry=0x%08X " +
        "end=0x%08X bytes=%d sha256=%s function_at=%s containing=%s",
        CALL, CALL + 4, ENTRY, END, bytes.length, digest,
        atEntry == null ? "missing" : atEntry.getEntryPoint().toString(),
        containing == null ? "missing" : containing.getEntryPoint().toString()));
    println("CALLER " + (caller == null ? "missing" : caller.toString()));
    for (Instruction instruction :
         currentProgram.getListing().getInstructions(body, true)) {
      println("  " + instruction.getAddress() + " " + instruction);
    }
  }
}
