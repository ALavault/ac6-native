// Repair the byte- and callsite-qualified PAL indirect vtable thunk reached
// during the Mission 01 campaign handoff. Run on the canonical project only.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;
import java.security.MessageDigest;

public class RepairOracleIndirectThunk extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
  private static final String BODY_SHA256 =
      "829ec6aaa88b2ebb142acfd3ec48057553b05b59a7ef3254f8ba632b086825c9";
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
    Instruction caller = getInstructionAt(toAddr(CALL));
    if (caller == null || !"bctrl".equals(caller.getMnemonicString())) {
      throw new AssertionError("wrong callsite at " + toAddr(CALL) + ": " + caller);
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
    AddressSet body = new AddressSet(entry, end);
    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    disassembler.disassemble(entry, body, true);
    String[] mnemonics = {"lwz", "lwz", "mtspr", "bctr"};
    for (int index = 0; index < mnemonics.length; ++index) {
      Address address = entry.add(index * 4L);
      Instruction instruction = getInstructionAt(address);
      if (instruction == null || !mnemonics[index].equals(instruction.getMnemonicString())) {
        throw new AssertionError("wrong instruction at " + address + ": " + instruction);
      }
    }

    Function function = getFunctionAt(entry);
    if (function == null) {
      function = createFunction(entry, "OracleIndirectThunk_820F6180");
    }
    if (function == null) {
      throw new AssertionError("cannot create function at " + entry);
    }
    function.setBody(body);
    function.setName("OracleIndirectThunk_820F6180", SourceType.USER_DEFINED);
    println("ORACLE_INDIRECT_THUNK_FUNCTION entry=" + entry + " body=" +
        function.getBody() + " sha256=" + actual + " caller=" + toAddr(CALL));
  }
}
