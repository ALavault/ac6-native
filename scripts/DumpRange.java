import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.disassemble.Disassembler;

public class DumpRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: DumpRange <start> <end-exclusive>");
        }

        long startValue = Long.decode(args[0]);
        long endValue = Long.decode(args[1]);
        Address start = toAddr(startValue);
        Address end = toAddr(endValue);
        Address cursor = start;
        Disassembler disassembler = Disassembler.getDisassembler(currentProgram, monitor, null);
        while (cursor.compareTo(end) < 0) {
            Instruction instruction = currentProgram.getListing().getInstructionAt(cursor);
            if (instruction == null) {
                disassembler.disassemble(cursor, null);
                instruction = currentProgram.getListing().getInstructionAt(cursor);
            }
            if (instruction == null) {
                println(cursor + " <not-disassembled>");
                cursor = cursor.add(4);
                continue;
            }
            println(instruction.getAddress() + " " + instruction);
            cursor = instruction.getMaxAddress().add(1);
        }
    }
}
