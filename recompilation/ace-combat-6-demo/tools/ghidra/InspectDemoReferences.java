import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

/** Print bounded incoming references for a canonical demo address. */
public class InspectDemoReferences extends GhidraScript {
  private static final String XEX_SHA256 =
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";

  @Override
  public void run() throws Exception {
    if (!"ace-combat-6-demo".equals(state.getProject().getName()) ||
        !"PowerPC:BE:64:Xenon".equals(currentProgram.getLanguageID().toString()) ||
        !"Default.xex".equals(currentProgram.getName()) ||
        !XEX_SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
      throw new IllegalStateException("wrong canonical demo project, language or XEX");
    }
    if (getScriptArgs().length == 0) {
      throw new IllegalArgumentException("expected one or more addresses");
    }
    for (String argument : getScriptArgs()) {
      long value = Long.decode(argument) & 0xffffffffL;
      Address target = toAddr(value);
      Function at = currentProgram.getFunctionManager().getFunctionAt(target);
      Function containing = currentProgram.getFunctionManager().getFunctionContaining(target);
      Instruction instruction = currentProgram.getListing().getInstructionAt(target);
      println(String.format("target=0x%08X function_at=%s function_containing=%s instruction=%s",
          value,
          at == null ? "-" : at.getEntryPoint(),
          containing == null ? "-" : containing.getEntryPoint(),
          instruction == null ? "-" : instruction.toString()));
      ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(target);
      int count = 0;
      while (references.hasNext()) {
        Reference reference = references.next();
        Address from = reference.getFromAddress();
        Function owner = currentProgram.getFunctionManager().getFunctionContaining(from);
        println(String.format("  from=0x%08X type=%s owner=%s",
            from.getOffset() & 0xffffffffL,
            reference.getReferenceType(),
            owner == null ? "-" : owner.getEntryPoint()));
        count++;
      }
      println("  incoming_count=" + count);
    }
  }
}
