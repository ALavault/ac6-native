// Read-only qualification for the PAL save-dialog M350 listener boundary.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.security.MessageDigest;

public class InspectOracleSaveMessageBoundary extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
  private static final long VTABLE = 0x8205DF4CL;
  private static final long ENTRY = 0x82159350L;
  private static final long END = 0x821593F8L;

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
    long slotTarget = Integer.toUnsignedLong(
        currentProgram.getMemory().getInt(toAddr(VTABLE + 8)));
    if (slotTarget != ENTRY) {
      throw new AssertionError(String.format("vtable slot mismatch: 0x%08X", slotTarget));
    }

    byte[] bytes = new byte[(int) (END - ENTRY + 4)];
    for (int index = 0; index < bytes.length; ++index) {
      bytes[index] = currentProgram.getMemory().getByte(entry.add(index));
    }
    String digest = hex(MessageDigest.getInstance("SHA-256").digest(bytes));

    Function atEntry = getFunctionAt(entry);
    Function containing = getFunctionContaining(entry);
    println("PROJECT " + state.getProject().getName());
    println("PROGRAM " + currentProgram.getName());
    println(String.format(
        "SAVE_MESSAGE_BOUNDARY vtable=0x%08X slot=+0x08 entry=0x%08X " +
        "end=0x%08X bytes=%d sha256=%s function_at=%s containing=%s",
        VTABLE, ENTRY, END, bytes.length, digest,
        atEntry == null ? "missing" : atEntry.getEntryPoint().toString(),
        containing == null ? "missing" : containing.getEntryPoint().toString()));

    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    AddressSet range = new AddressSet(entry, end);
    disassembler.disassemble(entry, range, true);
    for (Instruction instruction :
         currentProgram.getListing().getInstructions(range, true)) {
      String mnemonic = instruction.getMnemonicString();
      if (mnemonic.startsWith("b") || mnemonic.equals("stw") ||
          mnemonic.equals("lwz")) {
        println("  " + instruction.getAddress() + " " + instruction);
      }
    }
  }
}
