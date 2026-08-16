import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * Export the narrow, address-qualified DurableBin consumer chain. This script
 * records only observed loads; it deliberately does not assign payload fields.
 */
public class ExportDurableConsumerEvidence extends GhidraScript {
  private static String normalized(String text) {
    return text.toLowerCase().replace(" ", "");
  }

  private List<Instruction> instructions(long address) throws Exception {
    Function function = getFunctionAt(toAddr(address));
    if (function == null) {
      throw new IllegalStateException(String.format("missing function at 0x%08X", address));
    }
    List<Instruction> result = new ArrayList<>();
    InstructionIterator iterator = currentProgram.getListing().getInstructions(
        function.getBody(), true);
    while (iterator.hasNext()) {
      result.add(iterator.next());
    }
    return result;
  }

  private void requireOrdered(List<Instruction> instructions, String label,
                              String... expected) {
    int cursor = 0;
    for (Instruction instruction : instructions) {
      if (normalized(instruction.toString()).equals(expected[cursor])) {
        ++cursor;
        if (cursor == expected.length) {
          return;
        }
      }
    }
    throw new IllegalStateException("missing ordered evidence: " + label);
  }

  private void dump(PrintWriter output, String label, long... addresses) {
    output.println("EVIDENCE " + label);
    for (long address : addresses) {
      Instruction instruction = currentProgram.getListing().getInstructionAt(toAddr(address));
      if (instruction == null) {
        throw new IllegalStateException(String.format("missing instruction at 0x%08X", address));
      }
      output.printf("  %s  %s%n", instruction.getAddress(), instruction);
    }
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length != 1) {
      throw new IllegalArgumentException("usage: ExportDurableConsumerEvidence <output>");
    }
    List<Instruction> consumer = instructions(0x82095D90L);
    requireOrdered(consumer, "consumer",
        "lwzr11,0x2a0(r3)",
        "lwzr10,0xc(r11)",
        "lwzr11,0xc(r11)",
        "lwzr11,0x0(r11)",
        "lbzr3,0x0(r11)");

    List<Instruction> producer = instructions(0x82095E98L);
    requireOrdered(producer, "runtime ObjBin attachment",
        "stwr31,0x2a0(r29)");
    requireOrdered(producer, "producer DurableBin payload",
        "lwzr11,0x2a0(r29)",
        "lwzr10,0xc(r11)",
        "lwzr11,0xc(r11)",
        "lwzr11,0x0(r11)",
        "lbzr11,0x0(r11)");

    List<Instruction> tableLookup = instructions(0x821EE0F8L);
    requireOrdered(tableLookup, "parallel table lookup",
        "lwzr11,0xc(r3)",
        "lwzxr11,r10,r11",
        "lwzr10,0x4(r3)",
        "addr3,r10,r11");

    List<Instruction> reader = instructions(0x82333738L);
    requireOrdered(reader, "ObjBin reader child slot",
        "lwzr11,0xc(r28)");

    try (PrintWriter output = new PrintWriter(args[0], "UTF-8")) {
      output.println("AC6_DEMO_DURABLE_CONSUMER_PASS");
      output.println("xex_sha256=de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8");
      output.println("consumer=0x82095D90");
      output.println("producer=0x82095E98");
      output.println("table_lookup=0x821EE0F8");
      output.println("reader=0x82333738");
      output.println("consumer_chain=runtime+0x2A0 -> ObjBin+0x0C -> DurableBin+0x00 -> lbz");
      output.println("payload_observation=lbz payload+0 at 0x82095DC8 and 0x8209611C");
      dump(output, "consumer", 0x82095D90L, 0x82095D9CL, 0x82095DB8L,
          0x82095DC4L, 0x82095DC8L);
      dump(output, "producer", 0x82096034L, 0x820960C8L, 0x820960D4L,
          0x820960F0L, 0x8209610CL, 0x82096118L, 0x8209611CL);
      dump(output, "table_lookup", 0x821EE10CL, 0x821EE114L, 0x821EE120L,
          0x821EE124L,
          0x821EE128L);
      dump(output, "reader", 0x8233380CL);
    }
    println("AC6_DEMO_DURABLE_CONSUMER_PASS output=" + args[0]);
  }
}
