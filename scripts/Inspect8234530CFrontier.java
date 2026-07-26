// Read-only listing for the AC6 PAL 0x8234530C frontier.
// @category AC6

import ghidra.app.script.GhidraScript;
import ghidra.program.disassemble.Disassembler;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.ReferenceIterator;

public class Inspect8234530CFrontier extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (long value = 0x82345230L; value <= 0x82345330L; value += 4) {
            Address address = toAddr(value);
            Instruction instruction = currentProgram.getListing().getInstructionAt(address);
            if (instruction == null) {
                Disassembler.getDisassembler(currentProgram, monitor, null).disassemble(address, null);
                instruction = currentProgram.getListing().getInstructionAt(address);
            }
            println("AC6_8234530C_LISTING=" + address + " " +
                (instruction == null ? "<none>" : instruction.toString()));
        }
        for (long value : new long[] {0x8234524cL, 0x82345250L, 0x82345260L,
                                      0x823452a8L, 0x8234530cL}) {
            Address address = toAddr(value);
            ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(address);
            int count = 0;
            while (references.hasNext()) { println("AC6_8234530C_REF=" + address + " " + references.next()); count++; }
            println("AC6_8234530C_META=" + address + " refs=" + count + " function=" +
                (currentProgram.getFunctionManager().getFunctionAt(address) != null));
        }
    }
}
