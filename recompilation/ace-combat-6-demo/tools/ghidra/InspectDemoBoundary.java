import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class InspectDemoBoundary extends GhidraScript {
  @Override
  public void run() throws Exception {
    String language = currentProgram.getLanguageID().toString();
    println("project=" + state.getProject().getName() +
        " program=" + currentProgram.getName() +
        " sha256=" + currentProgram.getExecutableSHA256() +
        " language=" + language + " length=" + language.length());
    for (String argument : getScriptArgs()) {
      long value = Long.decode(argument) & 0xffffffffL;
      Address address = toAddr(value);
      Function at = currentProgram.getFunctionManager().getFunctionAt(address);
      Function containing = currentProgram.getFunctionManager().getFunctionContaining(address);
      Instruction instruction = currentProgram.getListing().getInstructionAt(address);
      println(String.format("address=0x%08X function_at=%s function_containing=%s instruction=%s",
          value, at == null ? "-" : at.getEntryPoint(),
          containing == null ? "-" : containing.getEntryPoint(),
          instruction == null ? "-" : instruction.toString()));
      println(String.format("  raw=0x%08X", currentProgram.getMemory().getInt(address)));
      if (instruction == null) {
        Disassembler.getDisassembler(currentProgram, monitor, null).disassemble(address, null);
        Instruction decoded = currentProgram.getListing().getInstructionAt(address);
        println("  after_disassemble=" + (decoded == null ? "-" : decoded.toString()));
      }
      Function owner = at != null ? at : containing;
      if (owner != null) {
        println("  body_min=" + owner.getBody().getMinAddress() +
            " body_max=" + owner.getBody().getMaxAddress() +
            " ranges=" + owner.getBody().getNumAddressRanges());
      }
      if (instruction != null) {
        println("  delay_depth=" + instruction.getDelaySlotDepth() +
            " has_fallthrough=" + instruction.getFlowType().hasFallthrough() +
            " terminal=" + instruction.getFlowType().isTerminal());
      }
    }
    String[] arguments = getScriptArgs();
    if (arguments.length == 2) {
      long start = Long.decode(arguments[0]) & 0xffffffffL;
      long end = Long.decode(arguments[1]) & 0xffffffffL;
      for (long value = start; value <= end; value += 4) {
        Instruction instruction = currentProgram.getListing().getInstructionAt(toAddr(value));
        println(String.format("range=0x%08X instruction=%s flow=%s", value,
            instruction == null ? "-" : instruction.toString(),
            instruction == null ? "-" : instruction.getFlowType()));
      }
    }
  }
}
