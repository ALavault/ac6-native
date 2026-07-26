// Read-only checks for the platform lifecycle portion of the XEX entry wrapper.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyEntryLifecycleCalls extends GhidraScript {
    private void require(long value, String mnemonic, String target) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                (target != null && !instruction.toString().contains(target))) {
            throw new IllegalStateException(address + " did not match");
        }
        println("AC6_LIFECYCLE=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        require(0x821f5ec8L, "bl", "821f7c40");
        require(0x821f5eccL, "bl", "821f5ca8");
        require(0x821f5ed8L, "bl", "823d6a5c");
        require(0x821f5ee4L, "bl", "821f7bc8");
        require(0x821f5eecL, "bl", "821f7ae8");
        require(0x821f7c5cL, "bl", "823d6a7c");
        require(0x821f7c94L, "bl", "823d6a8c");
    }
}
