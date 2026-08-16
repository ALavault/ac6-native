// Export only non-.pdata function starts from the qualified AC6 demo program.
// The Ghidra project and JSON output are build artifacts; the XEX is never
// copied into the product source tree.
import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class ExportQualifiedDemoChunks extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static final long TEXT_START = 0x82090000L;
  private static final int TEXT_BYTES = 0x2E67C4;
  private static final long PDATA_START = 0x82077200L;
  private static final int PDATA_BYTES = 0x10438;
  private static final String PDATA_SHA256 =
      "82c68b78f3256dd0c2bdd0df40e97daf6f3cf6dd1e162d5ddee4a47d1d14e50b";
  private final List<long[]> pdataRanges = new ArrayList<>();

  private static final class Chunk {
    final long entry;
    final long size;
    final String name;
    final String kind;
    final long rangeMin;
    final long rangeMax;

    Chunk(long entry, long size, String name, String kind, long rangeMin, long rangeMax) {
      this.entry = entry;
      this.size = size;
      this.name = name;
      this.kind = kind;
      this.rangeMin = rangeMin;
      this.rangeMax = rangeMax;
    }
  }

  private static final class DataRange {
    final long address;
    final long size;
    final long followingOwner;
    final String evidence;

    DataRange(long address, long size, long followingOwner, String evidence) {
      this.address = address;
      this.size = size;
      this.followingOwner = followingOwner;
      this.evidence = evidence;
    }
  }

  private static String address(long value) {
    return String.format("0x%08X", value);
  }

  private static String quote(String value) {
    return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
  }

  private List<long[]> readConfirmedFunctionRanges(Path path) throws Exception {
    List<long[]> ranges = new ArrayList<>();
    if (path == null) {
      return ranges;
    }
    long address = -1;
    for (String line : Files.readAllLines(path, StandardCharsets.UTF_8)) {
      String trimmed = line.trim();
      if (trimmed.startsWith("address") && trimmed.contains("=")) {
        address = Long.decode(trimmed.substring(trimmed.indexOf('=') + 1).trim());
      } else if (trimmed.startsWith("size") && trimmed.contains("=") && address >= 0) {
        long size = Long.decode(trimmed.substring(trimmed.indexOf('=') + 1).trim());
        if (size <= 0 || size % 4 != 0 || address % 4 != 0) {
          throw new IllegalStateException("invalid confirmed function range");
        }
        ranges.add(new long[] {address, address + size - 1});
        address = -1;
      }
    }
    return ranges;
  }

  private String sha256(long entry, int size) throws Exception {
    byte[] bytes = new byte[size];
    currentProgram.getMemory().getBytes(toAddr(entry), bytes);
    byte[] digest = java.security.MessageDigest.getInstance("SHA-256").digest(bytes);
    StringBuilder result = new StringBuilder();
    for (byte value : digest) {
      result.append(String.format("%02x", value & 0xff));
    }
    return result.toString();
  }

  private String sha256(byte[] bytes) throws Exception {
    byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
    StringBuilder result = new StringBuilder();
    for (byte value : digest) {
      result.append(String.format("%02x", value & 0xff));
    }
    return result.toString();
  }

  private Set<Long> readPdataStarts() throws Exception {
    byte[] bytes = new byte[PDATA_BYTES];
    currentProgram.getMemory().getBytes(toAddr(PDATA_START), bytes);
    if (!PDATA_SHA256.equals(sha256(bytes))) {
      throw new IllegalStateException("demo .pdata SHA-256 mismatch");
    }
    pdataRanges.clear();
    Set<Long> starts = new HashSet<>();
    for (int offset = 0; offset < bytes.length; offset += 8) {
      long entry = ((long) (bytes[offset] & 0xff) << 24)
          | ((long) (bytes[offset + 1] & 0xff) << 16)
          | ((long) (bytes[offset + 2] & 0xff) << 8)
          | (long) (bytes[offset + 3] & 0xff);
      long packed = ((long) (bytes[offset + 4] & 0xff) << 24)
          | ((long) (bytes[offset + 5] & 0xff) << 16)
          | ((long) (bytes[offset + 6] & 0xff) << 8)
          | (long) (bytes[offset + 7] & 0xff);
      long size = ((packed >>> 8) & 0x3fffffL) * 4;
      if (entry != 0 && size != 0) {
        starts.add(entry);
        pdataRanges.add(new long[] {entry, entry + size - 1});
      }
    }
    return starts;
  }

  private boolean inPdata(long address) {
    for (long[] range : pdataRanges) {
      if (address >= range[0] && address <= range[1]) {
        return true;
      }
    }
    return false;
  }

  private boolean terminal(Instruction instruction) {
    return instruction != null && (instruction.getFlowType().isTerminal() ||
        ((instruction.getFlowType().isJump() || instruction.getFlowType().isCall()) &&
         !instruction.getFlowType().hasFallthrough()));
  }

  private List<Chunk> readBoundedUnownedChunks(MemoryBlock text, Set<Long> pdataStarts)
      throws Exception {
    List<Chunk> result = new ArrayList<>();
    Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
    long cursor = text.getStart().getOffset();
    long end = text.getEnd().getOffset();
    while (cursor <= end) {
      Address current = toAddr(cursor);
      Instruction instruction = currentProgram.getListing().getInstructionAt(current);
      Function owner = currentProgram.getFunctionManager().getFunctionContaining(current);
      if (instruction == null && cursor >= text.getStart().getOffset() + 4) {
        Instruction previous = currentProgram.getListing().getInstructionAt(toAddr(cursor - 4));
        Instruction branch = cursor >= text.getStart().getOffset() + 8
            ? currentProgram.getListing().getInstructionAt(toAddr(cursor - 8)) : null;
        boolean afterTerminal = terminal(previous) ||
            (currentProgram.getMemory().getInt(toAddr(cursor - 4)) == 0 && terminal(branch));
        if (afterTerminal) {
          disassembler.disassemble(current, null);
          instruction = currentProgram.getListing().getInstructionAt(current);
        }
      }
      if (instruction == null || owner != null || inPdata(cursor)) {
        cursor += 4;
        continue;
      }

      long start = cursor;
      long last = cursor;
      boolean hasTerminal = false;
      while (cursor <= end) {
        Address candidate = toAddr(cursor);
        Instruction candidateInstruction = currentProgram.getListing().getInstructionAt(candidate);
        Function candidateOwner = currentProgram.getFunctionManager().getFunctionContaining(candidate);
        if (candidateInstruction == null && cursor >= text.getStart().getOffset() + 4) {
          Instruction previous = currentProgram.getListing().getInstructionAt(toAddr(cursor - 4));
          Instruction branch = cursor >= text.getStart().getOffset() + 8
              ? currentProgram.getListing().getInstructionAt(toAddr(cursor - 8)) : null;
          boolean afterTerminal = terminal(previous) ||
              (currentProgram.getMemory().getInt(toAddr(cursor - 4)) == 0 && terminal(branch));
          if (afterTerminal) {
            disassembler.disassemble(candidate, null);
            candidateInstruction = currentProgram.getListing().getInstructionAt(candidate);
          }
        }
        if (candidateInstruction == null || candidateOwner != null || inPdata(cursor)) {
          break;
        }
        last = cursor;
        hasTerminal |= terminal(candidateInstruction);
        cursor += 4;
      }

      Instruction lastInstruction = currentProgram.getListing().getInstructionAt(toAddr(last));
      Instruction previous = start >= text.getStart().getOffset() + 4
          ? currentProgram.getListing().getInstructionAt(toAddr(start - 4)) : null;
      boolean previousBoundary = previous != null ? terminal(previous) :
          currentProgram.getMemory().getInt(toAddr(start - 4)) == 0;
      if (hasTerminal && terminal(lastInstruction) && previousBoundary) {
        int size = (int) (last - start + 4);
        result.add(new Chunk(start, size, "DemoBoundedChunk_" + ExportQualifiedDemoChunks.address(start),
            "bounded-unowned", start, last + 3));
      }
      if (cursor == start) {
        cursor += 4;
      }
    }
    return result;
  }

  private boolean coveredByChunk(List<Chunk> chunks, long address) {
    for (Chunk chunk : chunks) {
      if (address >= chunk.rangeMin && address <= chunk.rangeMax) {
        return true;
      }
    }
    return false;
  }

  private boolean inConfirmedFunction(List<long[]> ranges, long address) {
    for (long[] range : ranges) {
      if (address >= range[0] && address <= range[1]) {
        return true;
      }
    }
    return false;
  }

  private List<DataRange> readQualifiedDataRanges(MemoryBlock text, Set<Long> pdataStarts,
      List<Chunk> chunks, List<long[]> confirmedFunctions) throws Exception {
    List<DataRange> result = new ArrayList<>();
    List<long[]> sortedPdata = new ArrayList<>(pdataRanges);
    sortedPdata.sort(Comparator.comparingLong(range -> range[0]));
    for (int index = 0; index + 1 < sortedPdata.size(); index++) {
      long[] current = sortedPdata.get(index);
      long[] next = sortedPdata.get(index + 1);
      long gapStart = current[1] + 1;
      long gapEnd = next[0] - 1;
      if (gapStart > gapEnd) {
        continue;
      }
      long cursor = gapStart;
      while (cursor <= gapEnd) {
        if (coveredByChunk(chunks, cursor) ||
            currentProgram.getFunctionManager().getFunctionContaining(toAddr(cursor)) != null ||
            pdataStarts.contains(cursor) || inConfirmedFunction(confirmedFunctions, cursor)) {
          cursor += 4;
          continue;
        }
        Instruction previous = getInstructionAt(toAddr(cursor - 4));
        Instruction branch = cursor >= text.getStart().getOffset() + 8
            ? getInstructionAt(toAddr(cursor - 8)) : null;
        boolean afterTerminal = terminal(previous) ||
            (currentProgram.getMemory().getInt(toAddr(cursor - 4)) == 0 && terminal(branch));
        boolean untypedWord = getInstructionAt(toAddr(cursor)) == null;
        if (!afterTerminal && !untypedWord) {
          cursor += 4;
          continue;
        }
        long rangeStart = cursor;
        while (cursor <= gapEnd && !coveredByChunk(chunks, cursor) &&
            currentProgram.getFunctionManager().getFunctionContaining(toAddr(cursor)) == null &&
            !pdataStarts.contains(cursor) && !inConfirmedFunction(confirmedFunctions, cursor)) {
          cursor += 4;
        }
        long size = cursor - rangeStart;
        result.add(new DataRange(rangeStart, size, next[0],
            "ghidra:ace-combat-6-demo/Default.xex predecessor=" +
            address(gapStart - 4) + "; terminal-boundary=" + address(gapStart) +
            "; no function owner; next .pdata owner=" + address(next[0])));
      }
    }
    return result;
  }

  private List<DataRange> mergeDataRanges(List<DataRange> ranges) {
    ranges.sort(Comparator.comparingLong(range -> range.address));
    List<DataRange> merged = new ArrayList<>();
    for (DataRange range : ranges) {
      if (merged.isEmpty()) {
        merged.add(range);
        continue;
      }
      DataRange previous = merged.get(merged.size() - 1);
      long previousEnd = previous.address + previous.size;
      long rangeEnd = range.address + range.size;
      if (range.address > previousEnd) {
        merged.add(range);
        continue;
      }
      long end = Math.max(previousEnd, rangeEnd);
      merged.set(merged.size() - 1, new DataRange(previous.address, end - previous.address,
          previous.followingOwner, previous.evidence + "; merged=" + address(range.address)));
    }
    return merged;
  }

  private void qualify() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName())) {
      throw new IllegalStateException("wrong Ghidra project: " + state.getProject().getName());
    }
    if (!"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString())) {
      throw new IllegalStateException("wrong processor language: " +
          currentProgram.getLanguageID());
    }
    if (!"Default.xex".equals(currentProgram.getName())) {
      throw new IllegalStateException("wrong program: " + currentProgram.getName());
    }
    if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong XEX: " + currentProgram.getExecutableSHA256());
    }
    MemoryBlock text = currentProgram.getMemory().getBlock(".text");
    if (text == null || !text.isExecute() ||
        text.getStart().getOffset() != TEXT_START || text.getSize() != TEXT_BYTES) {
      throw new IllegalStateException("executable .text block is missing");
    }
  }

  @Override
  public void run() throws Exception {
    qualify();
    String[] arguments = getScriptArgs();
    if (arguments.length < 1 || arguments.length > 2) {
      throw new IllegalArgumentException("expected output path and optional confirmed-chunks.toml");
    }
    Set<Long> pdataStarts = readPdataStarts();
    List<long[]> confirmedFunctions = arguments.length == 2
        ? readConfirmedFunctionRanges(Path.of(arguments[1])) : new ArrayList<>();
    MemoryBlock text = currentProgram.getMemory().getBlock(".text");
    List<Chunk> chunks = new ArrayList<>();
    List<DataRange> functionPrefixData = new ArrayList<>();
    List<String> skipped = new ArrayList<>();
    FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
    while (iterator.hasNext()) {
      Function function = iterator.next();
      long entry = function.getEntryPoint().getOffset();
      if (function.isExternal() || pdataStarts.contains(entry) ||
          !text.contains(function.getEntryPoint())) {
        continue;
      }
      if (function.getBody().getNumAddressRanges() != 1) {
        skipped.add(address(entry) + ":non-contiguous");
        continue;
      }
      long bodyMin = function.getBody().getMinAddress().getOffset();
      long bodyMax = function.getBody().getMaxAddress().getOffset();
      if (bodyMin != entry) {
        boolean branchSeparated = (bodyMin + 4 == entry || bodyMin + 8 == entry) &&
            terminal(currentProgram.getListing().getInstructionAt(toAddr(bodyMin - 4)));
        if (!branchSeparated) {
          skipped.add(address(entry) + ":non-contiguous");
          continue;
        }
        functionPrefixData.add(new DataRange(bodyMin, entry - bodyMin, entry,
            "ghidra:ace-combat-6-demo/Default.xex unreachable function-body prefix=" +
            address(bodyMin) + "; entry=" + address(entry)));
      }
      long size = bodyMax - entry + 1;
      if (size <= 0 || size % 4 != 0) {
        skipped.add(address(entry) + ":unaligned");
        continue;
      }
      String kind = bodyMin == entry ? "function" : "function-after-branch-evidence";
      chunks.add(new Chunk(entry, size, function.getName(), kind, entry, bodyMax));
    }
    chunks.addAll(readBoundedUnownedChunks(text, pdataStarts));
    chunks.sort((left, right) -> Long.compare(left.entry, right.entry));
    List<DataRange> dataRanges = readQualifiedDataRanges(text, pdataStarts, chunks, confirmedFunctions);
    dataRanges.addAll(functionPrefixData);
    dataRanges = mergeDataRanges(dataRanges);

    Path output = Path.of(arguments[0]);
    try (BufferedWriter writer = Files.newBufferedWriter(
        output, StandardCharsets.UTF_8, StandardOpenOption.CREATE,
        StandardOpenOption.TRUNCATE_EXISTING, StandardOpenOption.WRITE)) {
      writer.write("{\n");
      writer.write("  \"schema\": \"ac6-demo-ghidra-chunks.export.v2\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"program\": \"Default.xex\",\n");
      writer.write("  \"module\": \"Default.xex\",\n");
      writer.write("  \"language\": " +
          quote(currentProgram.getLanguageID().toString()) + ",\n");
      writer.write("  \"xex_sha256\": " + quote(currentProgram.getExecutableSHA256().toLowerCase()) + ",\n");
      writer.write("  \"image_base\": \"0x82000000\",\n");
      writer.write("  \"entry_point\": \"0x821A7160\",\n");
      writer.write("  \"text\": {\"address\": \"0x82090000\", " +
          "\"size\": \"0x002E67C4\", \"byte_sha256\": " +
          quote(sha256(TEXT_START, TEXT_BYTES)) + "},\n");
      writer.write("  \"pdata\": {\"address\": \"0x82077200\", " +
          "\"size\": \"0x00010438\", \"byte_sha256\": " +
          quote(PDATA_SHA256) + "},\n");
      writer.write("  \"pdata_sha256\": " + quote(PDATA_SHA256) + ",\n");
      writer.write("  \"skipped_chunks\": [");
      for (int index = 0; index < skipped.size(); index++) {
        if (index != 0) {
          writer.write(", ");
        }
        writer.write(quote(skipped.get(index)));
      }
      writer.write("],\n");
      writer.write("  \"chunks\": [\n");
      for (int index = 0; index < chunks.size(); index++) {
        Chunk chunk = chunks.get(index);
        writer.write("    {\"address\": " + quote(address(chunk.entry)) +
            ", \"size\": " + quote(address(chunk.size)) +
            ", \"name\": " + quote(chunk.name) +
            ", \"kind\": " + quote(chunk.kind) +
            ", \"byte_sha256\": " + quote(sha256(chunk.entry, (int) chunk.size)) +
            ", \"range\": [" + quote(address(chunk.rangeMin)) +
            ", " + quote(address(chunk.rangeMax)) + "]}");
        writer.write(index + 1 == chunks.size() ? "\n" : ",\n");
      }
      writer.write("  ],\n");
      writer.write("  \"data_ranges\": [\n");
      for (int index = 0; index < dataRanges.size(); index++) {
        DataRange range = dataRanges.get(index);
        writer.write("    {\"address\": " + quote(address(range.address)) +
            ", \"size\": " + quote(address(range.size)) +
            ", \"kind\": \"unreachable-data\", \"evidence\": " +
            quote(range.evidence) + ", \"byte_sha256\": " +
            quote(sha256(range.address, (int) range.size)) + "}");
        writer.write(index + 1 == dataRanges.size() ? "\n" : ",\n");
      }
      writer.write("  ]\n}\n");
    }
    println("AC6_DEMO_GHIDRA_CHUNKS_PASS pdata=" + pdataStarts.size() +
        " chunks=" + chunks.size() + " data_ranges=" + dataRanges.size() +
        " skipped=" + skipped.size() + " output=" + output);
  }
}
