// Read-only checks for the entry handoff to the synchronized task loop.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyEntryTaskHandoff extends GhidraScript {
    private void require(long value, String mnemonic, String target) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                (target != null && !instruction.toString().contains(target))) {
            throw new IllegalStateException(address + " did not match");
        }
        println("AC6_TASK_HANDOFF=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        require(0x821f6024L, "bl", "821d7d90");
        require(0x821d7d9cL, "bl", "821d5ef8");
        require(0x821d7df0L, "bl", "821d6bd0");
        require(0x821d7e34L, "bl", "821d7a90");
        require(0x821d7e38L, "bl", "821d7c80");
        require(0x821d7e88L, "b", "821d7e34");
    }
}
