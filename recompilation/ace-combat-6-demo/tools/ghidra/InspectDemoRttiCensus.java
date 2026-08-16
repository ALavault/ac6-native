import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.mem.MemoryBlock;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Read-only census used to select the demo RTTI/vtable export rule. */
public class InspectDemoRttiCensus extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    int rtti = 0;
    int vtable = 0;
    int typeDescriptor = 0;
    int msvcTypeNames = 0;
    int descriptorReferences = 0;
    int locatorReferences = 0;
    Map<Long, String> descriptorsByAddress = new HashMap<>();
    SymbolIterator iterator = currentProgram.getSymbolTable().getAllSymbols(true);
    while (iterator.hasNext()) {
      Symbol symbol = iterator.next();
      String lower = symbol.getName(true).toLowerCase();
      boolean rttiMatch = lower.contains("rtti") || lower.contains("type_descriptor") ||
          lower.contains("typeinfo") || lower.contains("class_hierarchy");
      boolean vtableMatch = lower.contains("vftable") || lower.contains("vtable");
      if (rttiMatch) rtti++;
      if (vtableMatch) vtable++;
      if (lower.contains("type_descriptor")) typeDescriptor++;
      if ((rttiMatch || vtableMatch) && rtti + vtable <= 40) {
        println(String.format("symbol=0x%08X type=%s name=%s",
            symbol.getAddress().getOffset() & 0xffffffffL,
            symbol.getSymbolType(), symbol.getName(true)));
      }
    }
    DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
    while (dataIterator.hasNext()) {
      Data data = dataIterator.next();
      Object value = data.getValue();
      if (!(value instanceof String)) continue;
      String string = (String) value;
      if (!(string.startsWith(".?AV") || string.startsWith(".?AU"))) continue;
      msvcTypeNames++;
      long descriptorValue = data.getAddress().getOffset() - 8;
      descriptorsByAddress.put(descriptorValue, string);
      ReferenceIterator descriptors = currentProgram.getReferenceManager()
          .getReferencesTo(toAddr(descriptorValue));
      while (descriptors.hasNext()) {
        long reference = descriptors.next().getFromAddress().getOffset();
        descriptorReferences++;
        long locator = reference - 12;
        ReferenceIterator locators = currentProgram.getReferenceManager()
            .getReferencesTo(toAddr(locator));
        while (locators.hasNext()) {
          locators.next();
          locatorReferences++;
        }
      }
    }
    Set<Long> locators = new HashSet<>();
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = block.getStart().getOffset();
      long end = block.getEnd().getOffset();
      for (long address = start; address + 4 <= end; address++) {
        if (currentProgram.getMemory().getByte(toAddr(address)) != '.' ||
            currentProgram.getMemory().getByte(toAddr(address + 1)) != '?' ||
            currentProgram.getMemory().getByte(toAddr(address + 2)) != 'A') continue;
        byte kind = currentProgram.getMemory().getByte(toAddr(address + 3));
        if (kind != 'V' && kind != 'U') continue;
        boolean terminated = false;
        for (long cursor = address + 4; cursor <= end && cursor < address + 512; cursor++) {
          if (currentProgram.getMemory().getByte(toAddr(cursor)) == 0) {
            terminated = true;
            break;
          }
        }
        if (terminated && address >= start + 8) {
          descriptorsByAddress.putIfAbsent(address - 8, "raw-msvc-type");
        }
      }
    }
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = (block.getStart().getOffset() + 3) & ~3L;
      long end = block.getEnd().getOffset();
      for (long address = start; address + 3 <= end; address += 4) {
        long value = currentProgram.getMemory().getInt(toAddr(address)) & 0xffffffffL;
        if (!descriptorsByAddress.containsKey(value) || address < start + 12) continue;
        long locator = address - 12;
        long signature = currentProgram.getMemory().getInt(toAddr(locator)) & 0xffffffffL;
        long hierarchy = currentProgram.getMemory().getInt(toAddr(locator + 16)) & 0xffffffffL;
        if (signature <= 1 && currentProgram.getMemory().contains(toAddr(hierarchy))) {
          locators.add(locator);
        }
      }
    }
    Set<Long> rawVtables = new HashSet<>();
    for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
      if (!block.isInitialized() || block.isExecute()) continue;
      long start = (block.getStart().getOffset() + 3) & ~3L;
      long end = block.getEnd().getOffset();
      for (long address = start; address + 7 <= end; address += 4) {
        long value = currentProgram.getMemory().getInt(toAddr(address)) & 0xffffffffL;
        if (!locators.contains(value)) continue;
        long firstSlot = currentProgram.getMemory().getInt(toAddr(address + 4)) & 0xffffffffL;
        MemoryBlock targetBlock = currentProgram.getMemory().getBlock(toAddr(firstSlot));
        if (targetBlock != null && targetBlock.isExecute()) rawVtables.add(address + 4);
      }
    }
    println("AC6_DEMO_RTTI_CENSUS rtti=" + rtti + " vtable=" + vtable +
        " type_descriptor=" + typeDescriptor + " msvc_type_names=" + msvcTypeNames +
        " descriptor_refs=" + descriptorReferences +
        " locator_refs=" + locatorReferences + " raw_locators=" + locators.size() +
        " raw_type_descriptors=" + descriptorsByAddress.size() +
        " raw_vtables=" + rawVtables.size());
  }
}
