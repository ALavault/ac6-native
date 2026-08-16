import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;

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

/** Export every computed call/jump site without guessing a target. */
public class ExportDemoIndirectSites extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static String address(long value) { return String.format("0x%08X", value); }
  private static String quote(String value) { return "\"" + value.replace("\"", "\\\"") + "\""; }
  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte value : bytes) result.append(String.format("%02x", value & 0xff));
    return result.toString();
  }
  private String instructionHash(long address) throws Exception {
    byte[] bytes = new byte[4];
    currentProgram.getMemory().getBytes(toAddr(address), bytes);
    return hex(MessageDigest.getInstance("SHA-256").digest(bytes));
  }
  private static final class Site {
    long address;
    long owner;
    String flow;
    String mnemonic;
    String hash;
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
    List<Site> sites = new ArrayList<>();
    InstructionIterator iterator = currentProgram.getListing().getInstructions(text.getStart(), true);
    while (iterator.hasNext()) {
      Instruction instruction = iterator.next();
      if (!text.contains(instruction.getAddress())) break;
      if (!instruction.getFlowType().isComputed() ||
          !(instruction.getFlowType().isCall() || instruction.getFlowType().isJump())) continue;
      Site site = new Site();
      site.address = instruction.getAddress().getOffset();
      Function owner = currentProgram.getFunctionManager().getFunctionContaining(instruction.getAddress());
      site.owner = owner == null ? 0 : owner.getEntryPoint().getOffset();
      site.flow = instruction.getFlowType().isCall() ? "computed-call" : "computed-jump";
      site.mnemonic = instruction.getMnemonicString();
      site.hash = instructionHash(site.address);
      sites.add(site);
    }
    sites.sort(Comparator.comparingLong(value -> value.address));
    Path output = Path.of(getScriptArgs()[0]);
    Path temporary = output.resolveSibling(output.getFileName().toString() + ".new");
    if (Files.exists(output) || Files.exists(temporary))
      throw new IllegalStateException("refusing indirect output collision");
    Files.createDirectories(output.toAbsolutePath().getParent());
    try (BufferedWriter writer = Files.newBufferedWriter(temporary, StandardCharsets.UTF_8,
        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
      writer.write("{\n  \"schema\": \"ac6-demo-indirect-sites.export/v1\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"xex_sha256\": \"" + XEX_SHA256 + "\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"language\": \"PowerPC:BE:64:Xenon\",\n  \"sites\": [\n");
      for (int index = 0; index < sites.size(); index++) {
        Site site = sites.get(index);
        writer.write("    {\"address\": " + quote(address(site.address)) +
            ", \"owner\": " + (site.owner == 0 ? "null" : quote(address(site.owner))) +
            ", \"flow\": " + quote(site.flow) + ", \"mnemonic\": " + quote(site.mnemonic) +
            ", \"instruction_sha256\": " + quote(site.hash) +
            ", \"resolution\": \"unknown\", \"targets\": []}");
        writer.write(index + 1 == sites.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE);
    println("AC6_DEMO_INDIRECT_SITES_PASS sites=" + sites.size() + " output=" + output);
  }
}
