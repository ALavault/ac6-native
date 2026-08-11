// Repair byte-qualified leaf/dispatcher functions reached through two measured switch tables.
// Run only on the canonical AC6 PAL project, without -readOnly.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;
import java.security.MessageDigest;

public class RepairOracleSwitchTableFunctions extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

  private static final class Record {
    final long start;
    final long end;
    final String digest;
    final String terminator;

    Record(long start, long end, String digest, String terminator) {
      this.start = start;
      this.end = end;
      this.digest = digest;
      this.terminator = terminator;
    }
  }

  private static final Record[] RECORDS = {
      new Record(0x8237BF08L, 0x8237BF30L,
          "bf8d0a72dcd79f0e802c5a1e76bc67742b8e3da1e72ba562a6e6bf808303c619", "bctr"),
      new Record(0x8237BF38L, 0x8237BF40L,
          "4561ef515bcc9ba1025fdc5f6ae4e73303a07a5ac647ffa832e67b322803aebb", "blr"),
      new Record(0x8237C640L, 0x8237C654L,
          "d9f5adbd4f2d1bf9b08d13c17af5504c1a336439d08f83f79d383dbc40da20d5", "blr"),
      new Record(0x8237C658L, 0x8237C66CL,
          "5d3515187236e81a0027685ff0fd8a53943f7599dc93ffc969a565a32376b79c", "blr"),
      new Record(0x8237C670L, 0x8237C684L,
          "64643f32fcadd0b71b2aaa7564f17855a946387c7d60a8e58eab75244c6f1359", "blr"),
      new Record(0x8237C688L, 0x8237C69CL,
          "2ded41d13e8af14e2e1c80e65a7d5de6890863f2b763cff0a30301967d6cd6da", "blr"),
      new Record(0x8237C6A0L, 0x8237C6B4L,
          "4b72509428a58ee8eaea6f12e87d1a8a1a38c306ba15952480f011d0c2ca624f", "blr"),
      new Record(0x8237C828L, 0x8237C850L,
          "f47b975d7a54fb116f203e0f0e82792177706c1e3f1446c5782b17a987ddc827", "bctr"),
      new Record(0x8237C878L, 0x8237C8A0L,
          "65c4ee73c35a1ce378d9bf6f8a29fdea79e3936d980865d40d02bac8fe6b1fa1", "bctr"),
  };

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

  private void repair(Record record, Disassembler disassembler) throws Exception {
    Address start = toAddr(record.start);
    Address end = toAddr(record.end);
    int length = (int) (record.end - record.start + 4);
    byte[] bytes = new byte[length];
    for (int index = 0; index < length; index++) {
      bytes[index] = currentProgram.getMemory().getByte(start.add(index));
    }
    String actual = hex(MessageDigest.getInstance("SHA-256").digest(bytes));
    if (!record.digest.equals(actual)) {
      throw new AssertionError("byte mismatch at " + start + ": " + actual);
    }

    Function owner = getFunctionContaining(start);
    if (owner != null && !owner.getEntryPoint().equals(start)) {
      throw new AssertionError(start + " already belongs to " + owner.getEntryPoint());
    }
    for (long address = record.start; address <= record.end; address += 4) {
      Address cursor = toAddr(address);
      if (getInstructionAt(cursor) == null) {
        disassembler.disassemble(cursor, null);
      }
      if (getInstructionAt(cursor) == null) {
        throw new AssertionError("undisassembled instruction " + cursor);
      }
    }
    Instruction terminator = getInstructionAt(end);
    if (!record.terminator.equals(terminator.getMnemonicString())) {
      throw new AssertionError("wrong terminator at " + end + ": " + terminator);
    }

    Function function = getFunctionAt(start);
    if (function == null) {
      function = createFunction(start, "OracleSwitchTarget_" + start.toString().toUpperCase());
    }
    if (function == null) {
      throw new AssertionError("cannot create function at " + start);
    }
    function.setBody(new AddressSet(start, end));
    function.setName("OracleSwitchTarget_" + start.toString().toUpperCase(),
        SourceType.USER_DEFINED);
    println("ORACLE_SWITCH_FUNCTION entry=" + start + " body=" + function.getBody() +
        " sha256=" + actual + " terminator=" + record.terminator);
  }

  @Override
  public void run() throws Exception {
    qualify();
    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    for (Record record : RECORDS) {
      repair(record, disassembler);
    }
    println("ORACLE_SWITCH_FUNCTION_REPAIR_PASS count=" + RECORDS.length);
  }
}
