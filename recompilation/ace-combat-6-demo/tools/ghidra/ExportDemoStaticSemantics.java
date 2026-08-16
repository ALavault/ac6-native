// Export deterministic semantic facts from the qualified AC6 demo project.
// This script is read-only: it never creates functions, names or data types.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.MemoryBlock;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

public class ExportDemoStaticSemantics extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static final long TEXT_START = 0x82090000L;
  private static final long TEXT_END = TEXT_START + 0x2E67C4L - 1;

  private static String address(long value) {
    return String.format("0x%08X", value);
  }

  private static String quote(String value) {
    if (value == null) {
      return "null";
    }
    return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"")
        .replace("\n", "\\n").replace("\r", "\\r") + "\"";
  }

  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte value : bytes) {
      result.append(String.format("%02x", value & 0xff));
    }
    return result.toString();
  }

  private String sha256(long start, int size) throws Exception {
    byte[] bytes = new byte[size];
    currentProgram.getMemory().getBytes(toAddr(start), bytes);
    return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
  }

  private static String sha256(String text) throws Exception {
    return hex(MessageDigest.getInstance("SHA-256").digest(
        text.getBytes(StandardCharsets.UTF_8)));
  }

  private void qualify() {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    MemoryBlock text = currentProgram.getMemory().getBlock(".text");
    if (text == null || !text.isExecute() || text.getStart().getOffset() != TEXT_START ||
        text.getEnd().getOffset() != TEXT_END) {
      throw new IllegalStateException("wrong executable .text range");
    }
  }

  private static final class Record {
    long entry;
    int size;
    String bytes;
    String symbol;
    String status;
    String pseudocode;
    List<Long> calls = new ArrayList<>();
  }

  @Override
  public void run() throws Exception {
    qualify();
    String[] arguments = getScriptArgs();
    if (arguments.length != 1) {
      throw new IllegalArgumentException("expected one output path");
    }
    DecompInterface decompiler = new DecompInterface();
    decompiler.toggleCCode(true);
    decompiler.toggleSyntaxTree(false);
    if (!decompiler.openProgram(currentProgram)) {
      throw new IllegalStateException("decompiler setup failed");
    }
    List<Record> records = new ArrayList<>();
    FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
    while (iterator.hasNext()) {
      monitor.checkCancelled();
      Function function = iterator.next();
      long entry = function.getEntryPoint().getOffset();
      if (function.isExternal() || entry < TEXT_START || entry > TEXT_END ||
          function.getBody().getNumAddressRanges() != 1 ||
          function.getBody().getMinAddress().getOffset() != entry) {
        continue;
      }
      long rawSize = function.getBody().getMaxAddress().getOffset() - entry + 1;
      if (rawSize <= 0 || rawSize > Integer.MAX_VALUE || (rawSize & 3) != 0) {
        continue;
      }
      Record record = new Record();
      record.entry = entry;
      record.size = (int) rawSize;
      record.bytes = sha256(entry, record.size);
      record.symbol = function.getName();
      Set<Function> called = function.getCalledFunctions(monitor);
      for (Function target : called) {
        long targetAddress = target.getEntryPoint().getOffset();
        if (targetAddress >= 0 && targetAddress <= 0xffffffffL) {
          record.calls.add(targetAddress);
        }
      }
      Collections.sort(record.calls);
      DecompileResults result = decompiler.decompileFunction(function, 20, monitor);
      if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
        String normalized = result.getDecompiledFunction().getC().replaceAll("\\s+", " ").trim();
        record.status = "success";
        record.pseudocode = sha256(normalized);
      } else if (result.isTimedOut()) {
        record.status = "timeout";
        record.pseudocode = null;
      } else {
        record.status = "failed";
        record.pseudocode = null;
      }
      records.add(record);
    }
    decompiler.dispose();

    Path output = Path.of(arguments[0]);
    Path temporary = output.resolveSibling(output.getFileName().toString() + ".new");
    if (Files.exists(output) || Files.exists(temporary)) {
      throw new IllegalStateException("refusing semantic output collision");
    }
    Files.createDirectories(output.toAbsolutePath().getParent());
    try (BufferedWriter writer = Files.newBufferedWriter(temporary, StandardCharsets.UTF_8,
        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
      writer.write("{\n  \"schema\": \"ac6-demo-static-semantics.export/v1\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"xex_sha256\": \"" + XEX_SHA256 + "\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"program\": \"Default.xex\",\n");
      writer.write("  \"language\": \"PowerPC:BE:64:Xenon\",\n");
      writer.write("  \"functions\": [\n");
      for (int index = 0; index < records.size(); index++) {
        Record record = records.get(index);
        writer.write("    {\"entry\": " + quote(address(record.entry)) +
            ", \"size\": " + quote(address(record.size)) +
            ", \"byte_sha256\": " + quote(record.bytes) +
            ", \"symbol\": " + quote(record.symbol) +
            ", \"decompilation\": {\"status\": " + quote(record.status) +
            ", \"pseudocode_sha256\": " + quote(record.pseudocode) + "}, \"direct_calls\": [");
        for (int call = 0; call < record.calls.size(); call++) {
          if (call != 0) writer.write(", ");
          writer.write(quote(address(record.calls.get(call))));
        }
        writer.write("], \"imports\": [], \"globals\": [], \"strings\": [], " +
            "\"rtti_vtables\": [], \"role\": \"unknown\", \"confidence\": \"unknown\"}");
        writer.write(index + 1 == records.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE);
    println("AC6_DEMO_STATIC_SEMANTICS_PASS functions=" + records.size());
  }
}
