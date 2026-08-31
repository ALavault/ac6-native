// Export canonical US AC6 function bodies without modifying the Ghidra project.
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

public class ExportUsFunctionBoundaries extends GhidraScript {
  private static final String PROJECT = "ac6-us";
  private static final String XEX_SHA256 =
      "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";
  private static final String LANGUAGE = "PowerPC:BE:64:Xenon";

  private static String address(long value) {
    return String.format("0x%08X", value);
  }

  private void qualify() {
    if (!PROJECT.equals(state.getProject().getName()) ||
        !"default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new AssertionError("US project/program/XEX identity mismatch");
    }
    String language = currentProgram.getLanguageID().toString();
    if (!LANGUAGE.equals(language) && !(LANGUAGE + ":default").equals(language)) {
      throw new AssertionError("wrong language: " + currentProgram.getLanguageID());
    }
  }

  @Override
  public void run() throws Exception {
    qualify();
    String[] arguments = getScriptArgs();
    if (arguments.length != 1) throw new IllegalArgumentException("expected one output path");
    List<Function> functions = new ArrayList<>();
    FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
    while (iterator.hasNext()) {
      Function function = iterator.next();
      if (!function.isExternal()) functions.add(function);
    }
    Path output = Path.of(arguments[0]);
    try (BufferedWriter writer = Files.newBufferedWriter(
        output, StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW,
        StandardOpenOption.WRITE)) {
      writer.write("{\n");
      writer.write("  \"schema\": \"ac6.ghidra-function-boundaries.v1\",\n");
      writer.write("  \"project\": \"" + PROJECT + "\",\n");
      writer.write("  \"program\": \"default.xex\",\n");
      writer.write("  \"sha256\": \"" + XEX_SHA256 + "\",\n");
      writer.write("  \"language\": \"" + LANGUAGE + "\",\n");
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
          if (!first) writer.write(", ");
          first = false;
          writer.write("[\"" + address(range.getMinAddress().getOffset()) + "\", \"" +
              address(range.getMaxAddress().getOffset()) + "\"]");
        }
        writer.write("]}");
        writer.write(index + 1 == functions.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    println("AC6_US_BOUNDARY_EXPORT_PASS functions=" + functions.size() +
        " output=" + output);
  }
}
