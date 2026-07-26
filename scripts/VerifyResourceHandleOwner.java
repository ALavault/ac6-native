// Read-only validation for the AC6 PAL resource-handle owner at 0x821b9408.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class VerifyResourceHandleOwner extends GhidraScript {
    private void require(long value, String mnemonic, String fragment) throws Exception {
        Address address = toAddr(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null || !mnemonic.equals(instruction.getMnemonicString()) ||
                !instruction.toString().contains(fragment)) {
            throw new IllegalStateException(address + " did not match");
        }
        println("AC6_RESOURCE_OWNER=" + address + " " + instruction);
    }

    @Override
    public void run() throws Exception {
        require(0x821b9408L, "mfspr", "LR");
        require(0x821b95d4L, "addi", "0x5a50");
        require(0x821b95e4L, "bl", "821d2ad8");
        require(0x821b95f4L, "addi", "0x5a68");
        require(0x821b95fcL, "bl", "821d2ad8");
        require(0x821b960cL, "addi", "0x5a7c");
        require(0x821b9610L, "bl", "821d2ad8");
        require(0x821b9624L, "addi", "0x5a90");
        require(0x821b962cL, "bl", "821d2ad8");
        require(0x821b963cL, "addi", "0x5aa0");
        require(0x821b9644L, "bl", "821d2ad8");
        require(0x821b9734L, "or", "r3,r30,r30");
        require(0x821b973cL, "b", "82382f40");
    }
}
