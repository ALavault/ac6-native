// Export every canonical AC6 PAL function body for deterministic oracle config generation.
// Run read-only with -noanalysis; this script never modifies the Ghidra database.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

public class ExportOracleFunctionBoundaries extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

  private static String address(long value) {
    return String.format("0x%08X", value);
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
    String[] arguments = getScriptArgs();
    if (arguments.length != 1) {
      throw new IllegalArgumentException("expected one new output path");
    }

    List<Function> functions = new ArrayList<>();
    FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
    while (iterator.hasNext()) {
      Function function = iterator.next();
      if (!function.isExternal()) {
        functions.add(function);
      }
    }

    Path output = Path.of(arguments[0]);
    try (BufferedWriter writer = Files.newBufferedWriter(
        output, StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW,
        StandardOpenOption.WRITE)) {
      writer.write("{\n");
      writer.write("  \"schema\": \"ac6.ghidra-function-boundaries.v1\",\n");
      writer.write("  \"project\": \"ace-combat-6\",\n");
      writer.write("  \"program\": \"default.xex\",\n");
      writer.write("  \"sha256\": \"" + currentProgram.getExecutableSHA256().toLowerCase() + "\",\n");
      writer.write("  \"language\": \"" + currentProgram.getLanguageID() + "\",\n");
      writer.write("  \"function_count\": " + functions.size() + ",\n");
      writer.write("  \"functions\": [\n");
      for (int index = 0; index < functions.size(); index++) {
        Function function = functions.get(index);
        writer.write("    {\"entry\": \"" + address(function.getEntryPoint().getOffset()) +
            "\", \"ranges\": [");
        AddressRangeIterator ranges = function.getBody().getAddressRanges(true);
        boolean first = true;
        while (ranges.hasNext()) {
          AddressRange range = ranges.next();
          if (!first) {
            writer.write(", ");
          }
          first = false;
          writer.write("[\"" + address(range.getMinAddress().getOffset()) + "\", \"" +
              address(range.getMaxAddress().getOffset()) + "\"]");
        }
        writer.write("]}");
        writer.write(index + 1 == functions.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    println("AC6_ORACLE_BOUNDARY_EXPORT_PASS functions=" + functions.size() +
        " output=" + output);
  }
}
