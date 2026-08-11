// Repair the byte- and vtable-qualified PAL M350 listener function.
// Run only on the canonical AC6 PAL project, without -readOnly.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;
import java.security.MessageDigest;

public class RepairOracleSaveMessageFunction extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
  private static final String BODY_SHA256 =
      "3987f4699a54bd4bbeeecbf9c49af73ff248c3bd456e5f8a03c8a9f991ed8d0e";
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
    if (!"ace-combat-6".equals(state.getProject().getName())) {
      throw new AssertionError("wrong project: " + state.getProject().getName());
    }
    if (!"default.xex".equals(currentProgram.getName())) {
      throw new AssertionError("wrong module: " + currentProgram.getName());
    }
    if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new AssertionError("wrong XEX: " + currentProgram.getExecutableSHA256());
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
    String actual = hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    if (!BODY_SHA256.equals(actual)) {
      throw new AssertionError("byte mismatch at " + entry + ": " + actual);
    }

    Function containing = getFunctionContaining(entry);
    if (containing != null && !entry.equals(containing.getEntryPoint())) {
      throw new AssertionError(entry + " already belongs to " + containing.getEntryPoint());
    }
    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    AddressSet body = new AddressSet(entry, end);
    disassembler.disassemble(entry, body, true);
    for (long value = ENTRY; value <= END; value += 4) {
      if (getInstructionAt(toAddr(value)) == null) {
        throw new AssertionError(String.format("undisassembled instruction 0x%08X", value));
      }
    }
    Instruction terminator = getInstructionAt(end);
    if (!"blr".equals(terminator.getMnemonicString())) {
      throw new AssertionError("wrong terminator at " + end + ": " + terminator);
    }

    Function function = getFunctionAt(entry);
    if (function == null) {
      function = createFunction(entry, "OracleMessageListener_82159350");
    }
    if (function == null) {
      throw new AssertionError("cannot create function at " + entry);
    }
    function.setBody(body);
    function.setName("OracleMessageListener_82159350", SourceType.USER_DEFINED);
    println("ORACLE_SAVE_MESSAGE_FUNCTION entry=" + entry + " body=" +
        function.getBody() + " sha256=" + actual + " vtable=0x8205DF4C slot=+0x08");
  }
}
