// Verify AC6_recomp heuristic starts against the canonical PAL Ghidra program.
// Run read-only with -noanalysis; this script never modifies the database.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class InspectOracleCodegenBoundaries extends GhidraScript {
  private static final String XEX_SHA256 =
      "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
  private static final long[][] FALSE_STARTS = {
      {0x821F7B18L, 0x821F7AE8L},
      {0x821F7B28L, 0x821F7AE8L},
      {0x821F7C08L, 0x821F7BC8L},
      {0x821F7C80L, 0x821F7C40L},
      {0x821F7CE0L, 0x821F7CA0L},
      {0x821F8A00L, 0x821F89C0L},
      {0x821F8B38L, 0x821F8AF8L},
      {0x82393E30L, 0x82393E28L},
      {0x82393EB8L, 0x82393E28L},
      {0x8239D8A0L, 0x8239D880L},
      {0x8239E970L, 0x8239E8E8L},
      {0x823A0238L, 0x823A0160L},
      {0x823A0240L, 0x823A0160L},
      {0x823A0298L, 0x823A0160L},
      {0x823A02D0L, 0x823A0160L},
      {0x823849F0L, 0x823849C8L},
      {0x82384AACL, 0x823849C8L},
      {0x82384AE8L, 0x823849C8L},
  };

  private Address address(long value) {
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
  }

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6".equals(state.getProject().getName())) {
      throw new AssertionError("wrong project: " + state.getProject().getName());
    }
    if (!"default.xex".equals(currentProgram.getName())) {
      throw new AssertionError("wrong module: " + currentProgram.getName());
    }
    if (!XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new AssertionError("wrong XEX: " + currentProgram.getExecutableSHA256());
    }

    println("QUALIFIED project=" + state.getProject().getName() +
        " module=" + currentProgram.getName() + " sha256=" +
        currentProgram.getExecutableSHA256() + " language=" +
        currentProgram.getLanguageID());
    for (long[] pair : FALSE_STARTS) {
      Address candidate = address(pair[0]);
      Address expected = address(pair[1]);
      Function exact = currentProgram.getFunctionManager().getFunctionAt(candidate);
      Function owner = currentProgram.getFunctionManager().getFunctionContaining(candidate);
      Instruction instruction = currentProgram.getListing().getInstructionAt(candidate);
      int directCalls = 0;
      for (Reference reference : getReferencesTo(candidate)) {
        if (reference.getReferenceType().isCall()) {
          directCalls++;
        }
      }
      if (exact != null || owner == null || !owner.getEntryPoint().equals(expected) ||
          instruction == null || directCalls != 0) {
        throw new AssertionError(String.format(
            "boundary mismatch candidate=0x%08X exact=%s owner=%s instruction=%s calls=%d",
            pair[0], exact, owner, instruction, directCalls));
      }
      println(String.format(
          "FALSE_START candidate=0x%08X owner=0x%08X instruction=%s direct_calls=%d",
          pair[0], pair[1], instruction.getMnemonicString(), directCalls));
    }
    println("ORACLE_CODEGEN_BOUNDARIES PASS count=" + FALSE_STARTS.length);
  }
}
