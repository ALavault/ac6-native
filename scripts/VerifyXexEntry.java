import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyXexEntry extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Address address = toAddr(0x821f5e90L);
        byte[] bytes = new byte[32];
        currentProgram.getMemory().getBytes(address, bytes);

        StringBuilder hex = new StringBuilder();
        for (byte value : bytes) {
            hex.append(String.format("%02x", value & 0xff));
        }
        println("AC6_ENTRY_BYTES=" + hex);

        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        for (int i = 0; i < 8 && instruction != null; i++) {
            println("AC6_ENTRY_INSN=" + instruction.getAddress() + " " + instruction);
            instruction = instruction.getNext();
        }
    }
}
