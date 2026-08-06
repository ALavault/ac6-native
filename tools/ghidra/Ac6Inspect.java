// Read-only PAL AC6 inspection. Run with -noanalysis against the canonical
// ace-combat-6/default.xex program; this script never changes the database.
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.nio.charset.StandardCharsets;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.symbol.Reference;

public class Ac6Inspect extends GhidraScript {
  private static final long[] FUNCTIONS = {
      0x82199BD8L, 0x82199D08L, 0x82199F68L,
      0x821A7260L, 0x821A72C0L, 0x821A75D0L, 0x821A7A70L,
      0x820A7F48L, 0x822A6560L, 0x822A65B0L, 0x822A6710L,
      0x8226D1C8L, 0x822707C8L, 0x823093C8L, 0x823094D8L
  };

  private static final long[] VTABLES = {
      0x82007A10L, 0x82054F7CL, 0x820568D4L, 0x82064264L,
      0x8206432CL, 0x82064384L, 0x820643DCL
  };

  // Canonical Ghidra currently splits these functions at the save-register
  // islands. Endpoints come from the qualified PAL literal instruction stream
  // and are inspected without repairing or renaming the database.
  private static final long[][] SPLIT_RANGES = {
      {0x8226D1C8L, 0x8226E0CCL},
      {0x822707C8L, 0x82270AECL}
  };

  private static final String[] STRINGS = {
      "UpBegin", "UpSpecial", "UpLocaUnit", "UpLocaObj", "UpLocaDb",
      "UpLocaArms", "UpFlag", "UpInput", "UpObj", "UpRep", "UpArms",
      "UpCam", "UpMap", "UpHud", "UpRadio", "UpSubWin", "UpEff",
      "UpObjPreSync", "UpObjAsync", "UpNormalizeMat", "UpObjSync"
  };

  private Address address(long value) {
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
  }

  private long u32(Address at) throws MemoryAccessException {
    return Integer.toUnsignedLong(currentProgram.getMemory().getInt(at));
  }

  private String functionAt(long value) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(address(value));
    return f == null ? "-" : f.getName() + "@" + f.getEntryPoint();
  }

  private void inspectFunction(long value) throws Exception {
    Address entry = address(value);
    Function f = currentProgram.getFunctionManager().getFunctionAt(entry);
    if (f == null) {
      println(String.format("FUNCTION 0x%08X missing", value));
      return;
    }
    println(String.format("FUNCTION 0x%08X name=%s body=%s entry=%s", value,
        f.getName(), f.getBody(), f.getEntryPoint()));
    for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
      if (monitor.isCancelled()) {
        return;
      }
      String mnemonic = ins.getMnemonicString();
      if (mnemonic.equals("bl") || mnemonic.equals("bctrl") ||
          mnemonic.equals("stw") || mnemonic.equals("stb") ||
          mnemonic.equals("sth") || mnemonic.equals("stfs")) {
        println(String.format("  %s %s", ins.getAddress(), ins));
      }
    }
    Set<String> callers = new HashSet<>();
    for (Reference ref : getReferencesTo(entry)) {
      Function caller = getFunctionContaining(ref.getFromAddress());
      callers.add((caller == null ? "-" : caller.getName()) + "@" + ref.getFromAddress());
    }
    String[] sorted = callers.toArray(new String[0]);
    Arrays.sort(sorted);
    println("  CALLERS " + String.join(",", sorted));
  }

  private void inspectVtable(long value) throws Exception {
    Address table = address(value);
    println(String.format("VTABLE 0x%08X", value));
    Set<String> refs = new HashSet<>();
    for (Reference ref : getReferencesTo(table)) {
      Function owner = getFunctionContaining(ref.getFromAddress());
      refs.add(ref.getFromAddress() + "@" +
          (owner == null ? "split-or-data" : owner.getName()));
    }
    String[] sortedRefs = refs.toArray(new String[0]);
    Arrays.sort(sortedRefs);
    println("  REFS " + String.join(",", sortedRefs));
    for (int offset = -4; offset <= 0xD0; offset += 4) {
      Address at = table.add(offset);
      long word = u32(at);
      println(String.format("  %s0x%03X %s = 0x%08X %s",
          offset < 0 ? "-" : "+", Math.abs(offset), at, word,
          functionAt(word)));
    }
  }

  private void inspectSplitRange(long start, long end) throws Exception {
    println(String.format("SPLIT_RANGE 0x%08X..0x%08X", start, end));
    AddressSet range = new AddressSet(address(start), address(end));
    for (Instruction ins : currentProgram.getListing().getInstructions(range, true)) {
      if (monitor.isCancelled()) {
        return;
      }
      String mnemonic = ins.getMnemonicString();
      if (mnemonic.equals("bl") || mnemonic.equals("bctrl") ||
          mnemonic.equals("stw") || mnemonic.equals("stb") ||
          mnemonic.equals("sth") || mnemonic.equals("stfs")) {
        println(String.format("  %s %s", ins.getAddress(), ins));
      }
    }
  }

  private void inspectString(String value) throws Exception {
    byte[] pattern = (value + "\0").getBytes(StandardCharsets.US_ASCII);
    Address cursor = currentProgram.getMinAddress();
    int found = 0;
    while (cursor != null) {
      Address at = currentProgram.getMemory().findBytes(cursor, pattern, null,
          true, monitor);
      if (at == null) {
        break;
      }
      found++;
      Set<String> refs = new HashSet<>();
      for (Reference ref : getReferencesTo(at)) {
        Function owner = getFunctionContaining(ref.getFromAddress());
        refs.add(ref.getFromAddress() + "@" +
            (owner == null ? "split-or-data" : owner.getName()));
      }
      String[] sorted = refs.toArray(new String[0]);
      Arrays.sort(sorted);
      println("STRING " + value + " at=" + at + " refs=" +
          String.join(",", sorted));
      cursor = at.next();
    }
    if (found == 0) {
      println("STRING " + value + " missing");
    }
  }

  private void inspectWords(long start, long end) throws Exception {
    println(String.format("WORDS 0x%08X..0x%08X", start, end));
    for (long value = start; value <= end; value += 4) {
      long word = u32(address(value));
      println(String.format("  0x%08X = 0x%08X %s", value, word,
          functionAt(word)));
    }
  }

  @Override
  public void run() throws Exception {
    println("PROJECT " + state.getProject().getName());
    println("PROGRAM " + currentProgram.getName());
    println("LANGUAGE " + currentProgram.getLanguageID());
    println("IMAGE_BASE " + currentProgram.getImageBase());
    for (long value : FUNCTIONS) {
      inspectFunction(value);
    }
    for (long value : VTABLES) {
      inspectVtable(value);
    }
    for (long[] range : SPLIT_RANGES) {
      inspectSplitRange(range[0], range[1]);
    }
    for (String value : STRINGS) {
      inspectString(value);
    }
    inspectWords(0x82008140L, 0x82008280L);
  }
}
