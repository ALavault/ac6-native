// Revalidate canonical inner entries against the fresh Xenon import.
import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;

public class ValidateDemoBoundarySet extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static boolean selectedKind(String kind) {
    return "canonical-reimport-boundary".equals(kind) ||
        "callable-bounded-entry".equals(kind);
  }

  private static final class Record {
    long address = -1;
    int size = -1;
    String kind = "";
    String digest = "";
  }

  private static String value(String line) {
    String result = line.substring(line.indexOf('=') + 1).trim();
    if (result.startsWith("\"") && result.endsWith("\"")) {
      return result.substring(1, result.length() - 1);
    }
    return result;
  }

  private static void appendIfSelected(List<Record> records, Record record) {
    if (selectedKind(record.kind)) {
      if (record.address < 0 || record.size <= 0 || (record.address & 3) != 0 ||
          (record.size & 3) != 0 || record.digest.length() != 64) {
        throw new IllegalStateException("invalid canonical boundary record");
      }
      records.add(record);
    }
  }

  private List<Record> readRecords(Path path) throws Exception {
    List<Record> records = new ArrayList<>();
    Record current = null;
    for (String line : Files.readAllLines(path, StandardCharsets.UTF_8)) {
      String trimmed = line.trim();
      if ("[[function]]".equals(trimmed)) {
        if (current != null) {
          appendIfSelected(records, current);
        }
        current = new Record();
      } else if (current != null && trimmed.startsWith("address")) {
        current.address = Long.decode(value(trimmed));
      } else if (current != null && trimmed.startsWith("size")) {
        current.size = Integer.decode(value(trimmed));
      } else if (current != null && trimmed.startsWith("kind")) {
        current.kind = value(trimmed);
      } else if (current != null && trimmed.startsWith("byte_sha256")) {
        current.digest = value(trimmed);
      }
    }
    if (current != null) {
      appendIfSelected(records, current);
    }
    return records;
  }

  private String sha256(Address start, int size) throws Exception {
    byte[] bytes = new byte[size];
    currentProgram.getMemory().getBytes(start, bytes);
    byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
    StringBuilder result = new StringBuilder();
    for (byte value : digest) {
      result.append(String.format("%02x", value & 0xff));
    }
    return result.toString();
  }

  private Instruction instructionAt(PseudoDisassembler pseudo, Address address) {
    Instruction instruction = getInstructionAt(address);
    if (instruction != null) {
      return instruction;
    }
    try {
      return pseudo.disassemble(address);
    } catch (Exception ignored) {
      return null;
    }
  }

  private static boolean terminal(Instruction instruction) {
    return instruction != null && (instruction.getFlowType().isTerminal() ||
        (instruction.getFlowType().isJump() &&
         !instruction.getFlowType().hasFallthrough()));
  }

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    String[] arguments = getScriptArgs();
    if (arguments.length != 1) {
      throw new IllegalArgumentException("expected confirmed-chunks.toml");
    }
    List<Record> records = readRecords(Path.of(arguments[0]));
    if (records.isEmpty()) {
      throw new IllegalStateException("canonical boundary set is empty");
    }
    PseudoDisassembler pseudo = new PseudoDisassembler(currentProgram);
    for (Record record : records) {
      Address start = toAddr(record.address);
      Address last = start.add(record.size - 4L);
      if (!record.digest.equals(sha256(start, record.size))) {
        throw new IllegalStateException("byte hash mismatch at " + start);
      }
      for (int offset = 0; offset < record.size; offset += 4) {
        Address address = start.add(offset);
        Instruction instruction = instructionAt(pseudo, address);
        if (instruction == null) {
          Instruction previous = offset == 0 ? instructionAt(pseudo, start.subtract(4)) :
              instructionAt(pseudo, address.subtract(4));
          if (currentProgram.getMemory().getInt(address) != 0 || !terminal(previous)) {
            throw new IllegalStateException("undecoded word at " + address);
          }
        }
      }
      Instruction tail = instructionAt(pseudo, last);
      if (!terminal(tail)) {
        throw new IllegalStateException("non-terminal canonical boundary at " + last);
      }
      Instruction previous = instructionAt(pseudo, start.subtract(4));
      boolean referenced = currentProgram.getReferenceManager().getReferencesTo(start).hasNext();
      if (!referenced && !terminal(previous)) {
        throw new IllegalStateException("unreferenced/unbounded entry at " + start);
      }
    }
    println("AC6_DEMO_BOUNDARY_SET_PASS count=" + records.size());
  }
}
