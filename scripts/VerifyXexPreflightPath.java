// Read-only mnemonic checks for the entry-adjacent XEX preflight path.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyXexPreflightPath extends GhidraScript {
    private void requireInstruction(long value, String mnemonic, String target) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                (target != null && !instruction.toString().contains(target))) {
            throw new IllegalStateException(address + " did not match expected instruction");
        }
        println("AC6_PREFLIGHT=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        requireInstruction(0x821f7d48L, "bl", "823d6dac");
        requireInstruction(0x821f7db4L, "bl", "821f9820");
        requireInstruction(0x821f7de0L, "blr", null);
        requireInstruction(0x821f7df4L, "bl", "821f7d10");
        requireInstruction(0x821f7e2cL, "bl", "823d6dbc");
        requireInstruction(0x821f7e3cL, "blr", null);
    }
}
