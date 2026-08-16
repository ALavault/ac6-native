import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Bounded evidence scan for ObjBin's +0x0C child pointer and its first
 * indirect payload reads. It does not assign a payload schema.
 */
public class ScanDurablePayload extends GhidraScript {
  private static final Pattern LOAD = Pattern.compile(
      "^(lwz|lbz|lhz|lha|lfs|lvx|lvxl) (r\\d+),(-?0x[0-9a-f]+)\\((r\\d+)\\)$");
  private static final Pattern COPY = Pattern.compile(
      "^(or|mr|addi|addis) (r\\d+),.*(r\\d+).*$");

  private static String register(String text) {
    return text == null ? "" : text;
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length != 1) {
      throw new IllegalArgumentException("usage: ScanDurablePayload <output>");
    }
    try (PrintWriter output = new PrintWriter(args[0], "UTF-8")) {
      int anchors = 0;
      int payloadCandidates = 0;
      FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
      while (functions.hasNext()) {
        Function function = functions.next();
        List<Instruction> instructions = new ArrayList<>();
        InstructionIterator iterator = currentProgram.getListing().getInstructions(
            function.getBody(), true);
        while (iterator.hasNext()) {
          instructions.add(iterator.next());
        }
        for (int index = 0; index < instructions.size(); ++index) {
          String anchorText = instructions.get(index).toString();
          Matcher anchor = LOAD.matcher(anchorText);
          if (!anchor.matches() || !anchor.group(3).equals("0xc")) {
            continue;
          }
          anchors++;
          String wrapper = anchor.group(2);
          StringBuilder evidence = new StringBuilder();
          evidence.append("ANCHOR ").append(instructions.get(index).getAddress())
              .append(" function=").append(function.getEntryPoint())
              .append(" text=").append(anchorText);
          String payload = null;
          int end = Math.min(instructions.size(), index + 65);
          for (int next = index + 1; next < end; ++next) {
            String text = instructions.get(next).toString();
            Matcher load = LOAD.matcher(text);
            if (load.matches() && load.group(4).equals(wrapper)) {
              evidence.append(" | ").append(instructions.get(next).getAddress())
                  .append(" ").append(text);
              if (load.group(3).equals("0x0")) {
                payload = load.group(2);
                break;
              }
            }
            Matcher copy = COPY.matcher(text);
            if (copy.matches() && copy.group(3).equals(wrapper)) {
              evidence.append(" | ").append(instructions.get(next).getAddress())
                  .append(" ").append(text);
            }
          }
          if (payload != null) {
            payloadCandidates++;
            int payloadEnd = Math.min(instructions.size(), index + 65);
            for (int next = index + 1; next < payloadEnd; ++next) {
              String text = instructions.get(next).toString();
              Matcher load = LOAD.matcher(text);
              if (load.matches() && load.group(4).equals(payload)) {
                evidence.append(" | payload-read ")
                    .append(instructions.get(next).getAddress()).append(" ").append(text);
              }
            }
          }
          output.println(evidence);
        }
      }
      output.println("SUMMARY anchors=" + anchors + " payload_candidates=" + payloadCandidates);
      println("WROTE anchors=" + anchors + " payload_candidates=" + payloadCandidates);
    }
  }
}
