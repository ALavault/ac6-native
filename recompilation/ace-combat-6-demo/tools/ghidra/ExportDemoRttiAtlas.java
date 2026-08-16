import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
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
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Deterministic byte-derived MSVC RTTI/vtable export for the PAL demo. */
public class ExportDemoRttiAtlas extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";
  private static final long TEXT_START = 0x82090000L;
  private static final long TEXT_END = TEXT_START + 0x2E67C4L - 1;

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
  private long u32(long address) throws Exception {
    return currentProgram.getMemory().getInt(toAddr(address)) & 0xffffffffL;
  }
  private int s32(long address) throws Exception {
    return currentProgram.getMemory().getInt(toAddr(address));
  }
  private boolean mapped(long address, int size) {
    return size > 0 && currentProgram.getMemory().contains(toAddr(address)) &&
        currentProgram.getMemory().contains(toAddr(address + size - 1L));
  }
  private boolean codePointer(long address) {
    return address >= TEXT_START && address <= TEXT_END && (address & 3) == 0;
  }
  private String readTypeName(long descriptor) throws Exception {
    long start = descriptor + 8;
    StringBuilder value = new StringBuilder();
    for (int offset = 0; offset < 512; offset++) {
      byte current = currentProgram.getMemory().getByte(toAddr(start + offset));
      if (current == 0) return value.toString();
      if ((current & 0x80) != 0 || current < 0x20) return "";
      value.append((char) current);
    }
    return "";
  }

  private static final class Base {
    long descriptor;
    int contained;
    int mdisp;
    int pdisp;
    int vdisp;
    long attributes;
  }
  private static final class Slot {
    int index;
    long target;
    boolean functionKnown;
  }
  private static final class Vtable {
    long address;
    long locator;
    long descriptor;
    String typeHash;
    int objectOffset;
    int constructorDisplacement;
    long hierarchy;
    List<Base> bases = new ArrayList<>();
    List<Slot> slots = new ArrayList<>();
  }

  private Map<Long, String> scanTypeDescriptors() throws Exception {
    Map<Long, String> result = new HashMap<>();
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = block.getStart().getOffset();
      long end = block.getEnd().getOffset();
      for (long current = start; current + 4 <= end; current++) {
        if (currentProgram.getMemory().getByte(toAddr(current)) != '.' ||
            currentProgram.getMemory().getByte(toAddr(current + 1)) != '?' ||
            currentProgram.getMemory().getByte(toAddr(current + 2)) != 'A') continue;
        byte kind = currentProgram.getMemory().getByte(toAddr(current + 3));
        if (kind != 'V' && kind != 'U') continue;
        String name = readTypeName(current - 8);
        if (!name.isEmpty()) result.put(current - 8, name);
      }
    }
    return result;
  }

  private Map<Long, Long> scanLocators(Map<Long, String> types) throws Exception {
    Map<Long, Long> result = new HashMap<>();
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = (block.getStart().getOffset() + 3) & ~3L;
      long end = block.getEnd().getOffset();
      for (long current = start; current + 3 <= end; current += 4) {
        long descriptor = u32(current);
        if (!types.containsKey(descriptor) || current < start + 12) continue;
        long locator = current - 12;
        long hierarchy = u32(locator + 16);
        if (u32(locator) <= 1 && mapped(hierarchy, 16)) result.put(locator, descriptor);
      }
    }
    return result;
  }

  private List<Base> readBases(long hierarchy, Map<Long, String> types) throws Exception {
    List<Base> result = new ArrayList<>();
    long count = u32(hierarchy + 8);
    long array = u32(hierarchy + 12);
    if (count > 256 || !mapped(array, (int) count * 4)) return result;
    for (int index = 0; index < count; index++) {
      long record = u32(array + index * 4L);
      if (!mapped(record, 24)) continue;
      long descriptor = u32(record);
      if (!types.containsKey(descriptor)) continue;
      Base base = new Base();
      base.descriptor = descriptor;
      base.contained = s32(record + 4);
      base.mdisp = s32(record + 8);
      base.pdisp = s32(record + 12);
      base.vdisp = s32(record + 16);
      base.attributes = u32(record + 20);
      result.add(base);
    }
    return result;
  }

  private List<Vtable> scanVtables(Map<Long, String> types, Map<Long, Long> locators)
      throws Exception {
    List<Vtable> result = new ArrayList<>();
    Set<Long> seen = new HashSet<>();
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = (block.getStart().getOffset() + 3) & ~3L;
      long end = block.getEnd().getOffset();
      for (long current = start; current + 7 <= end; current += 4) {
        long locator = u32(current);
        if (!locators.containsKey(locator) || !codePointer(u32(current + 4))) continue;
        long address = current + 4;
        if (!seen.add(address)) continue;
        Vtable table = new Vtable();
        table.address = address;
        table.locator = locator;
        table.descriptor = locators.get(locator);
        table.typeHash = sha256(types.get(table.descriptor));
        table.objectOffset = s32(locator + 4);
        table.constructorDisplacement = s32(locator + 8);
        table.hierarchy = u32(locator + 16);
        table.bases = readBases(table.hierarchy, types);
        for (int slotIndex = 0; slotIndex < 256 && mapped(address + slotIndex * 4L, 4);
             slotIndex++) {
          long target = u32(address + slotIndex * 4L);
          if (!codePointer(target)) break;
          Slot slot = new Slot();
          slot.index = slotIndex;
          slot.target = target;
          Function function = currentProgram.getFunctionManager().getFunctionAt(toAddr(target));
          slot.functionKnown = function != null;
          table.slots.add(slot);
        }
        result.add(table);
      }
    }
    result.sort(Comparator.comparingLong(value -> value.address));
    return result;
  }

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    if (getScriptArgs().length != 1) throw new IllegalArgumentException("expected output path");
    Map<Long, String> types = scanTypeDescriptors();
    Map<Long, Long> locators = scanLocators(types);
    List<Vtable> tables = scanVtables(types, locators);
    if (types.size() != 772 || locators.size() != 801 || tables.size() != 801) {
      throw new IllegalStateException("unexpected RTTI census types=" + types.size() +
          " locators=" + locators.size() + " vtables=" + tables.size());
    }
    Path output = Path.of(getScriptArgs()[0]);
    Path temporary = output.resolveSibling(output.getFileName().toString() + ".new");
    if (Files.exists(output) || Files.exists(temporary))
      throw new IllegalStateException("refusing RTTI output collision");
    Files.createDirectories(output.toAbsolutePath().getParent());
    try (BufferedWriter writer = Files.newBufferedWriter(temporary, StandardCharsets.UTF_8,
        StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
      writer.write("{\n  \"schema\": \"ac6-demo-rtti-atlas.export/v1\",\n");
      writer.write("  \"target_id\": \"ac6-demo-xbox360-pal\",\n");
      writer.write("  \"xex_sha256\": \"" + XEX_SHA256 + "\",\n");
      writer.write("  \"project\": \"ace-combat-6-demo\",\n");
      writer.write("  \"language\": \"PowerPC:BE:64:Xenon\",\n");
      writer.write("  \"type_descriptor_count\": 772,\n  \"vtables\": [\n");
      for (int index = 0; index < tables.size(); index++) {
        Vtable table = tables.get(index);
        writer.write("    {\"address\": " + quote(address(table.address)) +
            ", \"locator\": " + quote(address(table.locator)) +
            ", \"type_descriptor\": " + quote(address(table.descriptor)) +
            ", \"type_name_sha256\": " + quote(table.typeHash) +
            ", \"object_offset\": " + table.objectOffset +
            ", \"constructor_displacement\": " + table.constructorDisplacement +
            ", \"class_hierarchy_descriptor\": " + quote(address(table.hierarchy)) +
            ", \"bases\": [");
        for (int baseIndex = 0; baseIndex < table.bases.size(); baseIndex++) {
          Base base = table.bases.get(baseIndex);
          if (baseIndex != 0) writer.write(", ");
          writer.write("{\"type_descriptor\": " + quote(address(base.descriptor)) +
              ", \"contained_bases\": " + base.contained + ", \"mdisp\": " + base.mdisp +
              ", \"pdisp\": " + base.pdisp + ", \"vdisp\": " + base.vdisp +
              ", \"attributes\": " + base.attributes + "}");
        }
        writer.write("], \"slots\": [");
        for (int slotIndex = 0; slotIndex < table.slots.size(); slotIndex++) {
          Slot slot = table.slots.get(slotIndex);
          if (slotIndex != 0) writer.write(", ");
          writer.write("{\"index\": " + slot.index + ", \"target\": " +
              quote(address(slot.target)) + ", \"function_known\": " + slot.functionKnown + "}");
        }
        writer.write("]}");
        writer.write(index + 1 == tables.size() ? "\n" : ",\n");
      }
      writer.write("  ],\n  \"rejections\": []\n}\n");
    }
    Files.move(temporary, output, StandardCopyOption.ATOMIC_MOVE);
    println("AC6_DEMO_RTTI_ATLAS_PASS types=772 vtables=801 output=" + output);
  }
}
