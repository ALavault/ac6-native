import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** Export source-addressed global/string/import references for atlas joining. */
public class ExportDemoStaticReferences extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static String address(long value) { return String.format("0x%08X", value); }
  private static String quote(String value) {
    return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
  }
  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte value : bytes) result.append(String.format("%02x", value & 0xff));
    return result.toString();
  }
  private static String sha256(String value) throws Exception {
    return hex(MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8)));
  }
  private static final class Record {
    long source;
    String kind;
    String value;
  }
  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    if (getScriptArgs().length != 1) throw new IllegalArgumentException("expected output path");
    MemoryBlock text = currentProgram.getMemory().getBlock(".text");
    List<Record> records = new ArrayList<>();
    InstructionIterator iterator = currentProgram.getListing().getInstructions(text.getStart(), true);
    while (iterator.hasNext()) {
      Instruction instruction = iterator.next();
      if (!text.contains(instruction.getAddress())) break;
      for (Reference reference : instruction.getReferencesFrom()) {
        Address destination = reference.getToAddress();
        Record record = new Record();
        record.source = instruction.getAddress().getOffset();
        if (destination.isExternalAddress()) {
          Symbol symbol = currentProgram.getSymbolTable().getPrimarySymbol(destination);
          record.kind = "import";
          record.value = symbol == null ? destination.toString() : symbol.getName(true);
        } else if (currentProgram.getMemory().contains(destination) && !text.contains(destination)) {
          Data data = currentProgram.getListing().getDefinedDataContaining(destination);
          if (data != null && data.getValue() instanceof String) {
            record.kind = "string";
            record.value = address(destination.getOffset()) + ":" + sha256((String) data.getValue());
          } else {
            record.kind = "global";
            record.value = address(destination.getOffset());
          }
        } else {
          continue;
        }
        records.add(record);
      }
    }
    records.sort(Comparator.comparingLong((Record value) -> value.source)
        .thenComparing(value -> value.kind).thenComparing(value -> value.value));
    Path output = Path.of(getScriptArgs()[0]);
    Path temporary = output.resolveSibling(output.getFileName().toString() + ".new");
    if (Files.exists(output) || Files.exists(temporary))
      throw new IllegalStateException("refusing reference output collision");
    Files.createDirectories(output.toAbsolutePath().getParent());
    try (BufferedWriter writer = Files.newBufferedWriter(temporary, StandardCharsets.UTF_8,
        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
      writer.write("{\n  \"schema\": \"ac6-demo-static-references.export/v1\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"xex_sha256\": \"" + XEX_SHA256 + "\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"language\": \"PowerPC:BE:64:Xenon\",\n  \"references\": [\n");
      for (int index = 0; index < records.size(); index++) {
        Record record = records.get(index);
        writer.write("    {\"source\": " + quote(address(record.source)) +
            ", \"kind\": " + quote(record.kind) + ", \"value\": " + quote(record.value) + "}");
        writer.write(index + 1 == records.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE);
    println("AC6_DEMO_STATIC_REFERENCES_PASS references=" + records.size() + " output=" + output);
  }
}
